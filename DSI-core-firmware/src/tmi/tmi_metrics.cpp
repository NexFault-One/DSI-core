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