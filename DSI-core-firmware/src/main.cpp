#include <Arduino.h>
#include <iostream>
#include <memory>
#include <stdio.h>
#include "../include/proto_codec/proto_communication.h"
#include "../include/proto_codec/proto_communication.c"
#include "../protobuf_msgs/proto_msgs/uart_data.pb.c"
#include "injectors/ByteDropInjector.h"
#include "injectors/BitFlipInjector.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// nizar esp pins
#define TX_PIN_NZ 17
#define RX_PIN_NZ 18

// prototype pins, for real product. TX = 6 and RX = 5
#define TX_PIN 6
#define RX_PIN 5

#define HOST_BAUD 9600
#define DEVICE_BAUD 115200

// to identify the esp32 for com port
#define DEVICE_ID "DSI"

// dsi commands (includes injectors types params inside of it [oneof params])
nxf1_v1_DsiCommand commands;

void performHandshake()
{
  const TickType_t retryDelay = pdMS_TO_TICKS(1000);
  String expectedAck = "<ACK:" DEVICE_ID ">";

  while(true)
  {
    Serial.printf("<HELLO_UI:%s>\n", DEVICE_ID);
    vTaskDelay(retryDelay);

    if(Serial.available())
    {
      String msg = Serial.readStringUntil('\n');
      msg.trim();
      if(msg == expectedAck)
      {
        Serial.printf("[HANDSHAKE OK] Device %s recognized by dashboard.\n", DEVICE_ID);
        break;
      }
    }
  }
}

// host included in both loops. the DSI receives from the host while TMI sends to the host.
void dsi_uut_loop()
{

  nxf1_v1_DsiCommand commands = nxf1_v1_DsiCommand_init_zero;
  //bytedropc = nxf1_v1_ByteDropParams_init_zero;

  vTaskDelay(pdMS_TO_TICKS(100));
  // protobuf decoding for DsiCommands
  if(!protobuf_receive(&Serial, &commands, nxf1_v1_DsiCommand_fields))
  {
    //Serial.println("failed to decode dsi commands");
    return;
  }
  Serial.println("*********************** DSI TRANSMITTER ***********************");
  Serial.println("[DSI] DSI Commands Received!");
  // default to 0, no byte dropping
  if(commands.inj_type == nxf1_v1_InjectionType_INJ_BYTE_DROP)
  {
    uint8_t buffer[PROTOBUF_BUFFER_SIZE];
    Serial.printf("[DEBUG] which_params=%d (expect %d)\n",
              commands.which_params, nxf1_v1_DsiCommand_byte_drop_tag);
    Serial.printf("[DEBUG] start_offset=%u length=%u payload='%s'\n",
              commands.params.byte_drop.start_offset,
              commands.params.byte_drop.length,
              commands.params.byte_drop.payload);
    Serial.println("[DSI] BytesDrop command received");
    Serial.print("Length (Num of Bytes to drop): ");
    Serial.println(commands.params.byte_drop.length);
    Serial.print("Offset (everyN): ");
    Serial.println(commands.params.byte_drop.start_offset);
    Serial.print("Original message: ");
    Serial.println(commands.params.byte_drop.payload);
    auto drop_byte = std::make_unique<ByteDropInjector>(commands.params.byte_drop.length, commands.params.byte_drop.start_offset);
    
    size_t len = snprintf((char*)buffer, sizeof(buffer), "%s", commands.params.byte_drop.payload);
    if(len == 0)
    {
      Serial.println("Payload empty...");
      vTaskDelay(pdMS_TO_TICKS(200));
      return;
    }

    Serial.print("[DSI] calling injector on ");
    Serial.print(len);
    Serial.println(" bytes...");
    size_t newlen = drop_byte->inject(buffer, len);

    if(newlen > sizeof(buffer))
    {
      Serial.println("[DSI] injector returned invalid length, clamping...");
      newlen = sizeof(buffer);
    }

    Serial2.write(buffer, newlen);
    Serial2.flush();
  } else if (commands.inj_type == nxf1_v1_InjectionType_INJ_BIT_FLIP)
  {
    uint8_t buffer[PROTOBUF_BUFFER_SIZE];
    Serial.println("[DSI] BitFlip command received");
    Serial.print("Mode: ");
    if(commands.params.bit_flip.mode == 0)
    {
      Serial.println("RANDOM");
    } else {
      Serial.println("PERIODIC");
    }
    Serial.print("every_n (PERIODIC ONLY): ");
    Serial.println(commands.params.bit_flip.every_n_p);
    Serial.print("Number of bits to drop (RANDOM ONLY): ");
    Serial.println(commands.params.bit_flip.bits_drop);
    Serial.print("Original message: ");
    Serial.println(commands.params.bit_flip.payload);
    
    std::unique_ptr<Injector> bit_flip;
    if(commands.params.bit_flip.mode == nxf1_v1_BitFlipMode_RANDOM)
    {
      bit_flip = std::make_unique<BitFlipInjector>(BitFlipMode::RANDOM, 0, commands.params.bit_flip.bits_drop);
    } else {
      bit_flip = std::make_unique<BitFlipInjector>(BitFlipMode::PERIODIC, commands.params.bit_flip.every_n_p, 0);
    }
    size_t len = snprintf((char*)buffer, sizeof(buffer), "%s", commands.params.bit_flip.payload);
    Serial.println();
    if(len == 0)
    {
      Serial.println("Payload empty...");
      vTaskDelay(pdMS_TO_TICKS(200));
      return;
    }

    Serial.print("[DSI] calling injector on ");
    Serial.print(len);
    Serial.println(" bytes...");
    size_t newlen = bit_flip->inject(buffer, len);

    if(newlen > sizeof(buffer))
    {
      Serial.println("[DSI] injector returned invalid length, clamping...");
      newlen = sizeof(buffer);
    }

    Serial2.write(buffer, newlen);
    Serial2.flush();
  }


  Serial.println("DSI to UUT Encoding...");
  vTaskDelay(pdMS_TO_TICKS(200));

}

// host included in both loops. the DSI receives from the host while TMI sends to the host.
void tmi_uut_loop()
{
  Serial.println("*********************** DSI RECEIVER ***********************");
  if(Serial2.available())
  {

  } else {
    Serial.println("Serial2 not available...");
  }

  vTaskDelay(pdMS_TO_TICKS(200));
}

// Task handles
TaskHandle_t DSI_Waveform_Handle = NULL;
TaskHandle_t DSI_TMI_Handle = NULL;

// HOST_DSI_UUT task - runs on Core 0
void DSI_Waveform_Task(void *pvParameters) {
  Serial.println("DSI_Waveform task started on Core " + String(xPortGetCoreID()));
  Serial.println("DSI_Waveform running on Core " + String(xPortGetCoreID()));

  performHandshake();

  // successful?
  Serial.println("[DSI] HANDSHAKE COMPLETED. DSI: Ready.");

  for (;;) {
    // Add your waveform generation logic here
    dsi_uut_loop();
    // Task delay to prevent watchdog timeout
    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay
  }

}

// UUT_TMI_HOST task - runs on Core 1
void DSI_TMI_Task(void *pvParameters) {
  Serial.println("DSI_TMI task started on Core " + String(xPortGetCoreID()));
  
  for (;;) {
    // Add your TMI (Telemetry, Monitoring, Interface) logic here
    //Serial.println("DSI_TMI running on Core " + String(xPortGetCoreID()));
    //tmi_uut_loop();
    // Task delay to prevent watchdog timeout
    vTaskDelay(pdMS_TO_TICKS(1500)); // 1.5 second delay
  }
}

void setup() {
  Serial.begin(HOST_BAUD);
  Serial2.begin(DEVICE_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
  
  // Wait for serial connection
  while (!Serial) {
    delay(10);
  }

  while (!Serial2) {
    delay(10);
  }
  
  Serial.println("Starting DSI Core Firmware...");
  Serial.println("Creating FreeRTOS tasks on dual cores");
  
  // Create DSI_Waveform task on Core 0
  xTaskCreatePinnedToCore(
    DSI_Waveform_Task,     // Task function
    "DSI_Waveform",        // Task name
    4096,                  // Stack size (bytes)
    NULL,                  // Task parameters
    2,                     // Task priority (0-25, higher number = higher priority)
    &DSI_Waveform_Handle,  // Task handle
    0                      // Core ID (0 = Core 0)
  );
  
  // Create DSI_TMI task on Core 1
  xTaskCreatePinnedToCore(
    DSI_TMI_Task,          // Task function
    "DSI_TMI",             // Task name
    4096,                  // Stack size (bytes)
    NULL,                  // Task parameters
    2,                     // Task priority (0-25, higher number = higher priority)
    &DSI_TMI_Handle,       // Task handle
    1                      // Core ID (1 = Core 1)
  );
  
  // Check if tasks were created successfully
  if (DSI_Waveform_Handle != NULL) {
    Serial.println("DSI_Waveform task created successfully on Core 0");
  } else {
    Serial.println("Failed to create DSI_Waveform task");
  }
  
  if (DSI_TMI_Handle != NULL) {
    Serial.println("DSI_TMI task created successfully on Core 1");
  } else {
    Serial.println("Failed to create DSI_TMI task");
  }
}

void loop() {
  // The loop() function runs on Core 1 by default
  // Since we're using FreeRTOS tasks, we can keep this minimal
  // or use it for additional background processing
  
  // Optional: Add any main loop logic here
  delay(5000); // 5 second delay to keep the loop light
}