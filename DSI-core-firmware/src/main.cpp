#include <Arduino.h>
#include <iostream>
#include <memory>
#include <stdio.h>
#include "../include/proto_codec/proto_communication.h"
#include "../include/proto_codec/proto_communication.c"
#include "../protobuf_msgs/proto_msgs/uart_data.pb.c"
#include "injectors/ByteDropInjector.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TX_PIN 17
#define RX_PIN 18

#define HOST_BAUD 9600
#define DEVICE_BAUD 115200

// byte drop injector testing

nxf1_v1_DsiCommand commands;
nxf1_v1_ByteDropParams bytesdrop;


void transmitterLoop()
{
  Serial.println("*********************** DSI TRANSMITTER ***********************");

  // protobuf decoding for DsiCommands
  if(!protobuf_receive(&Serial, &commands, nxf1_v1_DsiCommand_fields))
  {
    Serial.print("[DSI Protobuf] Failed to decode DSI Commands");
    return;
  }

  Serial.println("Received protobuf...");

  // default to 0, no byte dropping
  auto drop_byte = std::make_unique<ByteDropInjector>(commands.params.byte_drop.length, commands.params.byte_drop.start_offset);
  //if(commands.inj_type == nxf1_v1_InjectionType_INJ_BYTE_DROP)
  //{
  //  Serial.println("[DSI] BytesDrop command received");
  //  Serial.print("Length (Num of Bytes to drop): ");
  //  Serial.println(commands.params.byte_drop.length);
  //  Serial.print("Offset (everyN): ");
  //  Serial.println(commands.params.byte_drop.start_offset);
  //  auto drop_byte = std::make_unique<ByteDropInjector>(commands.params.byte_drop.length, commands.params.byte_drop.start_offset);
  //  
  //}

  // payload testing
  char str[PROTOBUF_BUFFER_SIZE] = "hello world";
  uint8_t buffer[PROTOBUF_BUFFER_SIZE];


  size_t len = snprintf((char*)buffer, sizeof(buffer), "%s", str);
  if(len == 0)
  {
    Serial.println("Payload empty...");
    vTaskDelay(pdMS_TO_TICKS(200));
    return;
  }

  Serial.print("[TX BDI] calling injector on ");
  Serial.print(len);
  Serial.println(" bytes...");
  size_t newlen = drop_byte->inject(buffer, len);

  if(newlen > sizeof(buffer))
  {
    Serial.println("[TX BDI] injector returned invalid length, clamping...");
    newlen = sizeof(buffer);
  }

  Serial2.write(buffer, newlen);
  Serial2.flush();

  Serial.println("DSI to UUT Encoding...");
  vTaskDelay(pdMS_TO_TICKS(200));

}

void receiverLoop()
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

// DSI_Waveform task - runs on Core 0
void DSI_Waveform_Task(void *pvParameters) {
  Serial.println("DSI_Waveform task started on Core " + String(xPortGetCoreID()));
  
  for (;;) {
    // Add your waveform generation logic here
    Serial.println("DSI_Waveform running on Core " + String(xPortGetCoreID()));
    transmitterLoop();
    // Task delay to prevent watchdog timeout
    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay
  }

}

// DSI_TMI task - runs on Core 1
void DSI_TMI_Task(void *pvParameters) {
  Serial.println("DSI_TMI task started on Core " + String(xPortGetCoreID()));
  
  for (;;) {
    // Add your TMI (Telemetry, Monitoring, Interface) logic here
    //Serial.println("DSI_TMI running on Core " + String(xPortGetCoreID()));
    
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