#pragma once
#include <stdint.h>
#include "injectors/Injector.h"
#include "proto_codec/proto_communication.h"

typedef enum {
    DSI_INJ_UNKNOWN = 0,
    DSI_INJ_BYTE_DROP,
    DSI_INJ_BIT_FLIP,
    DSI_INJ_PHANTOM_BYTE
} DsiInjectionType;

typedef struct {
    DsiInjectionType inj_type;
    union {
        struct {
            uint32_t start_offset;
            uint32_t length;
            char payload[PROTOBUF_BUFFER_SIZE];
        } byte_drop;
        struct {
            uint32_t every_n_p;
            uint32_t bits_drop;
            char payload[PROTOBUF_BUFFER_SIZE];
            //nxf1_v1_BitFlipMode mode;
        } bit_flip;
    } params;
}DsiCommandMessage;