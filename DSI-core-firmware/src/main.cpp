#include <Arduino.h>
#include <iostream>
#include <memory>
#include <stdio.h>
#include "freertos/task.h"
#include "tasks/dsi_tasks.h"
#include "tasks/tmi_tasks.h"

// nizar esp pins
#define TX_PIN_NZ 17
#define RX_PIN_NZ 18

// prototype pins, for real product. TX = 45 and RX = 48
#define TX_PIN 45
#define RX_PIN 48

#define HOST_BAUD 9600
#define MODBUS_BAUD 19200
#define DEVICE_BAUD 115200

// modbus initialization
#define MODBUS_TX 45 // DI
#define MODBUS_RX 48 // RO
#define MODBUS_DE 47

// to identify the esp32 for com port
#define DEVICE_ID "DSI"

// dsi commands (includes injectors types params inside of it [oneof params])
//nxf1_v1_DsiCommand commands;

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

  vTaskDelay(pdMS_TO_TICKS(1500));
  //performHandshake();

  // successful?
  Serial.println("[DSI] HANDSHAKE COMPLETED. DSI: Ready.");

  // start the DSI specific tasks that deal with protobuf decoding and injecting for UART
  start_dsi_tasks();


  for (;;) {
    // Add your waveform generation logic here
    //dsi_uut_loop();
    // Task delay to prevent watchdog timeout
    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay
  }

}

// UUT_TMI_HOST task - runs on Core 1
void DSI_TMI_Task(void *pvParameters) {
  Serial.println("DSI_TMI task started on Core " + String(xPortGetCoreID()));
  
  TMI_Init();
  // start TMI specific taks that will report data
  start_tmi_tasks();

  for (;;) {
    // Add your TMI (Telemetry, Monitoring, Interface) logic here
    //Serial.println("DSI_TMI running on Core " + String(xPortGetCoreID()));
    //tmi_uut_loop();
    // Task delay to prevent watchdog timeout
    vTaskDelay(pdMS_TO_TICKS(1500)); // 1.5 second delay
  }
}

void setup() {

  // HOST COMMUNICATION (HOST<->DSI/TMI)
  Serial.begin(HOST_BAUD);

  // MODBUS RTU
  Serial1.begin(MODBUS_BAUD, SERIAL_8N1, MODBUS_RX, MODBUS_TX);

  // RAW UART
  Serial2.begin(DEVICE_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);

  // Wait for serial connection
  delay(500);

  // modbus rs485 direction control pin 
  pinMode(MODBUS_DE, OUTPUT);
  digitalWrite(MODBUS_DE, LOW);

  delay(5);

  // create tasks
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