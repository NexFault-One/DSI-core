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


// multi-tasking functions
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
    uint32 duration = millis() - tmi_data.test_start_time;
    tmi_data.report.injection_duration_ms = duration;
    TMI_UnlockReport();

    Serial.printf("[TMI_SHARED] Test stopped! Duration: %u ms \n", duration);
}

// tmi reporting functions
static tmi_reporter_task(void* pv)
{
    (void)pv;

    for(;;)
    {
        nxf1_v1_TmiReport TMI_REPORTS = nxf1_v1_TmiReport_init_zero;
        
    }
}