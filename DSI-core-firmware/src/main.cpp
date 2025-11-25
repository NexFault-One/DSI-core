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

#define TX_PIN 17
#define RX_PIN 18

#define HOST_BAUD 9600
#define DEVICE_BAUD 115200
#define BIT_FLIP 1

// dsi commands (includes injectors types params inside of it [oneof params])
nxf1_v1_DsiCommand commands;

// host included in both loops. the DSI receives from the host while TMI sends to the host.
void dsi_uut_loop()
{

  nxf1_v1_DsiCommand commands = nxf1_v1_DsiCommand_init_zero;
  char payload_str[PROTOBUF_BUFFER_SIZE];
  memset(payload_str, 0, sizeof(payload_str));

  char bit_flip_str[PROTOBUF_BUFFER_SIZE] = "Hello world";

  //commands.which_params = nxf1_v1_DsiCommand_byte_drop_tag;
  commands.params.byte_drop.payload.arg = payload_str;
  commands.params.byte_drop.payload.funcs.decode = &protobuf_decode_string;

  vTaskDelay(pdMS_TO_TICKS(100));
  Serial.printf("[DEBUG] payload_str len=%u content='%s'\n", strlen(payload_str), payload_str);

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
    Serial.println("[DSI] BytesDrop command received");
    Serial.print("Length (Num of Bytes to drop): ");
    Serial.println(commands.params.byte_drop.length);
    Serial.print("Offset (everyN): ");
    Serial.println(commands.params.byte_drop.start_offset);
    Serial.print("Original message: ");
    //Serial.println(payload_str);
    auto drop_byte = std::make_unique<ByteDropInjector>(commands.params.byte_drop.length, commands.params.byte_drop.start_offset);
    
    size_t len = snprintf((char*)buffer, sizeof(buffer), "%s", payload_str);
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
    Serial.println("Random");
    Serial.print("every_n: ");
    Serial.println("none because it is random!");
    Serial.print("Number of bits to drop: ");
    Serial.println("5");
    Serial.print("Original message: ");
    Serial.println(bit_flip_str);
    auto bit_flip = std::make_unique<BitFlipInjector>(BitFlipMode::RANDOM, 0, 5);
    
    size_t len = snprintf((char*)buffer, sizeof(buffer), "%s", bit_flip_str);
    Serial.print("Original hex: ");
    for(size_t i = 0;i<len;++i)
    {
      Serial.printf("0x%02X ", bit_flip_str[i]);
    }
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