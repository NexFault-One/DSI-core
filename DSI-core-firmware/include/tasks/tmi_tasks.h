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

#define MODBUS_DE 42

static void tmi_cmd_task(void *pv);
static void traffic_counter_task(void *pv);
static void frame_stats_task(void* pv);
static void crash_detection_task(void* pv);
static void performance_health_detection_task(void* pv);
static void final_verdict_task(void* pv);
void start_tmi_tasks();