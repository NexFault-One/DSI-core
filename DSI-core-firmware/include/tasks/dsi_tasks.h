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

static void dsi_cmd_task(void *pv);
static void dsi_injector_task(void *pv);

void start_dsi_tasks(QueueHandle_t queue);