#include "tasks/tmi_tasks.h"

extern "C" {
    #include "proto_msgs/uart_data.pb.h"
    #include "pb_decode.h"
    #include "pb_encode.h"
}

// global variables
TMI_SharedData tmi_data;
QueueHandle_t injection_event_queue;

// non-global
static QueueHandle_t report_ready_queue;

// forward declarations of helper functions
static void calculate_final_verdict();
static bool is_modbus_error_response(uint8_t* response, size_t len);
static size_t receive_modbus_response(uint8_t* buffer, size_t max_len, uint32_t timeout_ms);

// ============================================================================
// INITIALIZATION & SHARED FUNCTIONS
// ============================================================================

void TMI_Init()
{
    tmi_data.mutex = xSemaphoreCreateMutex();
    if(tmi_data.mutex == NULL)
    {
        Serial.println("[TMI_SHARED] Failed to create Mutex at initialization!");
        return;
    }

    injection_event_queue = xQueueCreate(10, sizeof(InjectionEvent));
    if(injection_event_queue == NULL)
    {
        Serial.println("[TMI_SHARED] Failed to create injection event queue");
        return;
    }

    tmi_data.test_active = false;
    tmi_data.test_start_time = 0;
    tmi_data.next_frame_id = 0;

    memset(&tmi_data.report, 0, sizeof(TMI_Report));
    Serial.println("[TMI_SHARED] TMI initialized successfully!");
}

void TMI_LockReport()
{
    xSemaphoreTake(tmi_data.mutex, portMAX_DELAY);
}

void TMI_UnlockReport()
{
    xSemaphoreGive(tmi_data.mutex);
}

void TMI_ResetReport(uint32_t command_id)
{
    TMI_LockReport();
    memset(&tmi_data.report, 0, sizeof(TMI_Report));

    tmi_data.report.id = command_id;
    tmi_data.report.timestamp_ms = millis();
    tmi_data.report.status = nxf1_v1_ExecStatus_STATUS_RUNNING;
    tmi_data.report.verdict = nxf1_v1_TestVerdict_VERDICT_UNSET;
    tmi_data.report.reason = nxf1_v1_FailureReason_FAIL_NONE;
    
    tmi_data.next_frame_id = 0;
    
    TMI_UnlockReport();
    
    Serial.printf("[TMI_SHARED] Report reset for command ID %u\n", command_id);
}

void TMI_StartTest()
{
    TMI_LockReport();
    tmi_data.test_active = true;
    tmi_data.test_start_time = millis();
    TMI_UnlockReport();

    Serial.println("[TMI_SHARED] Test started!");
}

void TMI_StopTest()
{
    TMI_LockReport();
    tmi_data.test_active = false;
    uint32_t duration = millis() - tmi_data.test_start_time;
    tmi_data.report.injection_duration_ms = duration;
    TMI_UnlockReport();

    Serial.printf("[TMI_SHARED] Test stopped! Duration: %u ms \n", duration);
}

// ============================================================================
// TASK 1: MONITORING (collects all stats for Modbus)
// ============================================================================

static void tmi_monitoring_task(void* pv)
{
    (void)pv;
    
    uint32_t consecutive_timeouts = 0;
    uint32_t total_response_time = 0;
    uint32_t response_count = 0;
    
    Serial.println("[TMI_MON] Monitoring task started on Core " + String(xPortGetCoreID()));
    
    for(;;)
    {
        InjectionEvent event;
        if(xQueueReceive(injection_event_queue, &event, portMAX_DELAY) == pdTRUE)
        {
            Serial.printf("[TMI_MON] Received injection event (frame_id=%u)\n", event.frame_id);
            
            // listen for UUT response on Serial1 (Modbus)
            uint32_t start_time = millis();
            uint8_t response[512];
            size_t resp_len = receive_modbus_response(response, 512, 200);
            uint32_t response_time = millis() - start_time;
            
            // print what we received
            if(resp_len > 0) {
                Serial.print("[TMI_MON] Received response: ");
                for(size_t i = 0; i < resp_len; i++) {
                    Serial.printf("%02X ", response[i]);
                }
                Serial.println();
            } else {
                Serial.println("[TMI_MON] TIMEOUT - no response");
            }
            
            // update ALL stats (mutex protected)
            TMI_LockReport();
            
            // traffic counters
            tmi_data.report.bytes_transmitted += event.bytes_sent;
            tmi_data.report.bytes_received += resp_len;
            tmi_data.report.frames_sent++;
            
            // classify response
            if(resp_len == 0) {
                // TIMEOUT
                tmi_data.report.responses_timeout++;
                consecutive_timeouts++;
                
                Serial.printf("[TMI_MON] Timeout count: %u consecutive\n", consecutive_timeouts);
                
                // update max streak
                if(consecutive_timeouts > tmi_data.report.consecutive_timeout_streak) {
                    tmi_data.report.consecutive_timeout_streak = consecutive_timeouts;
                }
                
                // crash detection
                if(consecutive_timeouts >= 10 && !tmi_data.report.uut_reset_suspected) {
                    tmi_data.report.uut_reset_suspected = true;
                    tmi_data.report.crash_timestamp_ms = millis() - tmi_data.test_start_time;
                    Serial.println("[TMI_MON] CRASH DETECTED - UUT stopped responding!");
                }
            }
            else {
                // got response - reset timeout counter
                consecutive_timeouts = 0;
                
                // check if error or success
                if(is_modbus_error_response(response, resp_len)) {
                    // ERROR response (good - UUT rejected corruption)
                    tmi_data.report.responses_error++;
                    Serial.println("[TMI_MON] UUT rejected frame (error response)");
                } else {
                    // SUCCESS response (bad - UUT accepted corruption)
                    tmi_data.report.responses_ok++;
                    Serial.println("[TMI_MON] UUT accepted corrupted frame!");
                }
                
                // performance tracking
                total_response_time += response_time;
                response_count++;
                tmi_data.report.avg_response_time_ms = total_response_time / response_count;
                if(response_time > tmi_data.report.max_response_time_ms) {
                    tmi_data.report.max_response_time_ms = response_time;
                }
                
                Serial.printf("[TMI_MON] Response time: %u ms (avg: %u ms, max: %u ms)\n", 
                             response_time, 
                             tmi_data.report.avg_response_time_ms,
                             tmi_data.report.max_response_time_ms);
            }
            
            TMI_UnlockReport();
            
            // check if test is complete
            if(!tmi_data.test_active) {
                Serial.println("[TMI_MON] Test complete! Calculating verdict...");
                calculate_final_verdict();
                
                // signal reporter task
                uint32_t signal = 1;
                xQueueSend(report_ready_queue, &signal, 0);
            }
        }
    }
}

// ============================================================================
// TASK 2: REPORTER (sends protobuf to host)
// ============================================================================

static void tmi_reporter_task(void* pv)
{
    (void)pv;
    
    Serial.println("[TMI_REPORT] Reporter task started on Core " + String(xPortGetCoreID()));
    
    for(;;)
    {
        uint32_t signal;
        if(xQueueReceive(report_ready_queue, &signal, portMAX_DELAY) == pdTRUE)
        {
            Serial.println("[TMI_REPORT] Generating final report...");
            
            TMI_LockReport();
            
            // create envelope
            nxf1_v1_Envelope envelope = nxf1_v1_Envelope_init_zero;
            
            // fill in TmiReport inside envelope
            envelope.has_report = true;
            
            // correlation
            envelope.report.id = tmi_data.report.id;
            envelope.report.run_id = tmi_data.report.run_id;
            envelope.report.attempt_no = tmi_data.report.attempt_no;
            envelope.report.timestamp_ms = tmi_data.report.timestamp_ms;
            
            // test config
            envelope.report.injection_type = tmi_data.report.injection_type;
            envelope.report.transport_type = tmi_data.report.transport_type;
            envelope.report.injection_duration_ms = tmi_data.report.injection_duration_ms;
            envelope.report.crc_recalculated = tmi_data.report.crc_recalculated;
            
            // traffic counters
            envelope.report.bytes_transmitted = tmi_data.report.bytes_transmitted;
            envelope.report.bytes_received = tmi_data.report.bytes_received;
            envelope.report.bytes_dropped = tmi_data.report.bytes_dropped;
            envelope.report.bits_flipped = tmi_data.report.bits_flipped;
            envelope.report.phantom_bytes_added = tmi_data.report.phantom_bytes_added;
            
            // frame stats
            envelope.report.frames_sent = tmi_data.report.frames_sent;
            envelope.report.responses_ok = tmi_data.report.responses_ok;
            envelope.report.responses_error = tmi_data.report.responses_error;
            envelope.report.responses_timeout = tmi_data.report.responses_timeout;
            envelope.report.consecutive_timeout_streak = tmi_data.report.consecutive_timeout_streak;
            
            // crash detection
            envelope.report.uut_reset_suspected = tmi_data.report.uut_reset_suspected;
            envelope.report.crash_timestamp_ms = tmi_data.report.crash_timestamp_ms;
            
            // performance
            envelope.report.avg_response_time_ms = tmi_data.report.avg_response_time_ms;
            envelope.report.max_response_time_ms = tmi_data.report.max_response_time_ms;
            
            // health
            envelope.report.decode_error_count = tmi_data.report.decode_error_count;
            envelope.report.queue_drop_count = tmi_data.report.queue_drop_count;
            
            // outcome
            envelope.report.status = tmi_data.report.status;
            envelope.report.verdict = tmi_data.report.verdict;
            envelope.report.reason = tmi_data.report.reason;
            strncpy(envelope.report.verdict_message, tmi_data.report.verdict_message, 128);
            
            TMI_UnlockReport();
            
            // send envelope to host
            if(protobuf_send(&Serial, &envelope, nxf1_v1_Envelope_fields)) {
                Serial.println("[TMI_REPORT] Report sent to host successfully");
            } else {
                Serial.println("[TMI_REPORT] ERROR: Failed to send report");
            }
            
            // print summary to console
            Serial.println("--------------------------------------------------------------");
            Serial.println("|                    TMI FINAL REPORT                        |");
            Serial.println("--------------------------------------------------------------");
            Serial.printf("Test ID: %u\n", envelope.report.id);
            Serial.printf("Duration: %u ms\n", envelope.report.injection_duration_ms);
            Serial.printf("Verdict: %s\n", envelope.report.verdict_message);
            Serial.println("--------------------------------------------------------------");
            Serial.printf("Frames sent: %u\n", envelope.report.frames_sent);
            Serial.printf("Responses OK: %u (%.1f%%)\n", envelope.report.responses_ok, 
                         (float)envelope.report.responses_ok / envelope.report.frames_sent * 100);
            Serial.printf("Responses ERROR: %u (%.1f%%)\n", envelope.report.responses_error,
                         (float)envelope.report.responses_error / envelope.report.frames_sent * 100);
            Serial.printf("Timeouts: %u (%.1f%%)\n", envelope.report.responses_timeout,
                         (float)envelope.report.responses_timeout / envelope.report.frames_sent * 100);
            Serial.println("--------------------------------------------------------------");
            if(envelope.report.uut_reset_suspected) {
                Serial.printf("CRASH DETECTED at %u ms\n", envelope.report.crash_timestamp_ms);
                Serial.printf("Max timeout streak: %u frames\n", envelope.report.consecutive_timeout_streak);
            }
            Serial.printf("Avg response time: %u ms\n", envelope.report.avg_response_time_ms);
            Serial.printf("Max response time: %u ms\n", envelope.report.max_response_time_ms);
            Serial.println("--------------------------------------------------------------");
        }
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static void calculate_final_verdict()
{
    TMI_LockReport();
    
    if(tmi_data.report.uut_reset_suspected) {
        // UUT crashed
        tmi_data.report.verdict = nxf1_v1_TestVerdict_VERDICT_CRASH;
        tmi_data.report.reason = nxf1_v1_FailureReason_FAIL_UUT_RESET_SUSPECTED;
        snprintf(tmi_data.report.verdict_message, 128,
                "UUT crashed at %u ms", tmi_data.report.crash_timestamp_ms);
    }
    else if(tmi_data.report.frames_sent > 0) {
        float success_rate = (float)tmi_data.report.responses_ok / tmi_data.report.frames_sent;
        
        if(success_rate > 0.05f) {
            // UUT accepted >5% of corrupted frames (FAIL)
            tmi_data.report.verdict = nxf1_v1_TestVerdict_VERDICT_FAIL;
            tmi_data.report.reason = nxf1_v1_FailureReason_FAIL_NONE;
            snprintf(tmi_data.report.verdict_message, 128,
                    "UUT accepted %.1f%% of corrupted frames", success_rate * 100);
        } else {
            // UUT rejected most corrupted frames (PASS)
            tmi_data.report.verdict = nxf1_v1_TestVerdict_VERDICT_PASS;
            tmi_data.report.reason = nxf1_v1_FailureReason_FAIL_NONE;
            snprintf(tmi_data.report.verdict_message, 128,
                    "UUT properly rejected corrupted frames");
        }
    }
    
    tmi_data.report.status = nxf1_v1_ExecStatus_STATUS_DONE;
    
    TMI_UnlockReport();
}

static bool is_modbus_error_response(uint8_t* response, size_t len)
{
    if(!len >= 3) {
        return false; // too short to be valid
    }
    
    // check if function code has error bit set (0x80)
    uint8_t function_code = response[1];
    if(function_code & 0x80) {
        Serial.printf("[TMI_MON] Error response detected (func=0x%02X, exception=0x%02X)\n", 
                     function_code, response[2]);
        return true;
    }
    
    return false;
}

static size_t receive_modbus_response(uint8_t* buffer, size_t max_len, uint32_t timeout_ms)
{
    size_t bytes_read = 0;
    unsigned long start = millis();
    
    // wait for first byte
    while((millis() - start) < timeout_ms && bytes_read < max_len) {
        if(Serial1.available()) {
            buffer[bytes_read++] = Serial1.read();
            start = millis(); // reset timeout after each byte
        }
    }
    
    return bytes_read;
}

// ============================================================================
// PUBLIC START FUNCTION
// ============================================================================

void start_tmi_tasks()
{
    report_ready_queue = xQueueCreate(1, sizeof(uint32_t));
    if(report_ready_queue == NULL) {
        Serial.println("[TMI] ERROR: Failed to create report queue!");
        return;
    }
    
    xTaskCreate(tmi_monitoring_task, "TMI_MON", 4096, NULL, 3, NULL);
    xTaskCreate(tmi_reporter_task, "TMI_REPORT", 2048, NULL, 2, NULL);
    
    Serial.println("[TMI] Tasks started successfully");
}