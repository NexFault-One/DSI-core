#pragma once

#include <iostream>
#include <stdio.h>
#include <Arduino.h>
#include "pb_decode.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "proto_codec/proto_communication.h"
#include "proto_msgs/uart_data.pb.h"
#include "injectors/ByteDropInjector.h"
#include "injectors/BitFlipInjector.h"
#include "injectors/PhantomByteInjector.h"
#include "protocols/UartProtocol.h"
#include "protocols/ModbusProtocol.h"

#define MODBUS_DE 47
// data sent from dsi (core 0) to tmi (core 1)
typedef struct {
    nxf1_v1_TransportType transport;
    nxf1_v1_InjectionType inj_type;
    uint32_t bytes_sent;
    uint32_t frame_id;
    uint32_t run_id;
    uint32_t attempt_no;
} InjectionEvent;

// TMI report structure (matches protobuf TmiReport)
typedef struct {
    // correlation
    uint32_t id;
    uint32_t run_id;
    uint32_t attempt_no;
    uint32_t timestamp_ms;
    
    // test config
    nxf1_v1_InjectionType injection_type;
    nxf1_v1_TransportType transport_type;
    uint32_t injection_duration_ms;
    bool crc_recalculated;
    
    // traffic counters
    uint32_t bytes_transmitted;
    uint32_t bytes_received;
    uint32_t bytes_dropped;
    uint32_t bits_flipped;
    uint32_t phantom_bytes_added;
    
    // frame stats
    uint32_t frames_sent;
    char final_frame[512];
    uint32_t responses_ok;
    uint32_t responses_error;
    uint32_t responses_timeout;
    uint32_t consecutive_timeout_streak;
    
    // crash detection
    bool uut_reset_suspected;
    uint32_t crash_timestamp_ms;
    
    // performance
    uint32_t avg_response_time_ms;
    uint32_t max_response_time_ms;
    
    // DSI/TMI health
    uint32_t decode_error_count;
    uint32_t queue_drop_count;
    
    // outcome
    nxf1_v1_ExecStatus status;
    nxf1_v1_TestVerdict verdict;
    nxf1_v1_FailureReason reason;
    char verdict_message[128];
    
} TMI_Report;

// shared data between TMI tasks
typedef struct {
    TMI_Report report;
    SemaphoreHandle_t mutex;
    bool test_active;
    bool final_report_sent;
    uint32_t test_start_time;
    uint32_t next_frame_id;
} TMI_SharedData;

// globals (defined in tmi_shared.cpp)
extern TMI_SharedData tmi_data;
extern QueueHandle_t injection_event_queue;

// create envelope
static nxf1_v1_Envelope envelope = nxf1_v1_Envelope_init_zero;

// multitasking
// public functions
void TMI_Init();
void TMI_LockReport();
void TMI_UnlockReport();
void TMI_ResetReport(uint32_t command_id);
void TMI_StartTest();
void TMI_StopTest();

// report tasks
static void tmi_reporter_task(void* pv);
static void tmi_monitoring_task(void* pv);
static void tmi_verdict_task(void* pv);
void start_tmi_tasks();