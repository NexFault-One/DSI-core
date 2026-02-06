#pragma once

#include <iostream>
#include <stdio.h>
#include <Arduino.h>
#include "pb_decode.h"
#include "dsi_message.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "proto_msgs/uart_data.pb.h"
#include "injectors/ByteDropInjector.h"
#include "injectors/BitFlipInjector.h"
#include "injectors/PhantomByteInjector.h"
#include "protocols/UartProtocol.h"
#include "protocols/ModbusProtocol.h"

#define MODBUS_DE 4

inline const char* getPayload(const nxf1_v1_DsiCommand& cmd)
{
    switch (cmd.inj_type)
    {
        case nxf1_v1_InjectionType_INJ_BYTE_DROP:
            return cmd.params.byte_drop.payload;
        case nxf1_v1_InjectionType_INJ_BIT_FLIP:
            return cmd.params.bit_flip.payload;
        case nxf1_v1_InjectionType_INJ_PHANTOM_BYTE:
            return cmd.params.phantom_byte.payload;
        default:
            return "";
    }
}
static std::unique_ptr<Injector> createInjector(const nxf1_v1_DsiCommand &command);
static void dsi_cmd_task(void *pv);
static void uart_injector_task(void *pv);
static void modbus_injector_task(void* pv);
void start_dsi_tasks();