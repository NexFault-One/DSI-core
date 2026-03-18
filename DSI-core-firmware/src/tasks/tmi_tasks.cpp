#include "tasks/tmi_tasks.h"

extern "C" {
    #include "proto_msgs/uart_data.pb.h"
    #include "pb_decode.h"
    #include "pb_encode.h"
}

QueueHandle_t traffic_counter_queue;
QueueHandle_t frame_stats_queue;
QueueHandle_t crash_detection_queue;
QueueHandle_t performance_health_detection_queue;
QueueHandle_t final_verdict_queue;

static tmi_cmd_task(void* pv)
{
    (void)pv;

    for(;;)
    {
        nxf1_v1_TmiReport TMI_REPORTS = nxf1_v1_TmiReport_init_zero;
        
    }
}