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

// modbus protocol frame and total bytes transmitted (accumulated across all injections)
void TMI_AddModbusFinalFrame(char* final_frame, uint32_t bytes)
{
    TMI_LockReport();
    tmi_data.report.bytes_transmitted = bytes;  

    // append frame to final_frame buffer separated by " | "
    size_t current_len = strlen(tmi_data.report.final_frame);
    size_t new_frame_len = strlen(final_frame);
    size_t max_buf = sizeof(tmi_data.report.final_frame) - 1;

    if(current_len == 0) {
        // first frame — just copy
        strncpy(tmi_data.report.final_frame, final_frame, max_buf);
        tmi_data.report.final_frame[max_buf] = '\0';
    } else {
        // append with separator if there is room
        size_t needed = current_len + 3 + new_frame_len; // " | " = 3 chars
        if(needed <= max_buf) {
            strcat(tmi_data.report.final_frame, " | ");
            strcat(tmi_data.report.final_frame, final_frame);
        }
        // if buffer is full, oldest frames are preserved; newest are dropped
    }

    TMI_UnlockReport();
}

void TMI_AddModbusOriginalFrame(char* original_frame)
{
    TMI_LockReport();

    // append frame to final_frame buffer separated by " | "
    size_t current_len = strlen(tmi_data.report.original_frame);
    size_t new_frame_len = strlen(original_frame);
    size_t max_buf = sizeof(tmi_data.report.final_frame) - 1;

    if(current_len == 0) {
        // first frame — just copy
        strncpy(tmi_data.report.original_frame, original_frame, max_buf);
        tmi_data.report.original_frame[max_buf] = '\0';
    } else {
        // append with separator if there is room
        size_t needed = current_len + 3 + new_frame_len; // " | " = 3 chars
        if(needed <= max_buf) {
            strcat(tmi_data.report.original_frame, " | ");
            strcat(tmi_data.report.original_frame, original_frame);
        }
        // if buffer is full, oldest frames are preserved; newest are dropped
    }

    TMI_UnlockReport();
}