#include "tasks/tmi_tasks.h"
#include "tmi/tmi_metrics.h"

void TMI_AddBytesDropped(uint32_t n){
    TMI_LockReport();
    tmi_data.report.bytes_dropped += n;
    TMI_UnlockReport();
}
void TMI_AddBitsFlipped(uint32_t n){
    TMI_LockReport();
    tmi_data.report.bits_flipped = n;
    TMI_UnlockReport();
}
void TMI_AddPhantomBytes(uint32_t n){
    TMI_LockReport();
    tmi_data.report.phantom_bytes_added = n;
    TMI_UnlockReport();
}

// modbus protocol frame and total bytes transmitted
void TMI_AddModbusFrame(char* final_frame, uint32_t bytes)
{
    TMI_LockReport();
    tmi_data.report.bytes_transmitted = bytes;
    strncpy(tmi_data.report.final_frame,
            final_frame,
            sizeof(tmi_data.report.final_frame) - 1);

    tmi_data.report.final_frame[sizeof(tmi_data.report.final_frame) - 1] = '\0';

    TMI_UnlockReport();
}