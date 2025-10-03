#include <Arduino.h>
#include <stdio.h>
#include "../include/proto_codec/proto_communication.h"
#include "../include/proto_codec/proto_communication.c"
#include "../proto_msgs/uart_data.pb.c"

#define RX_PIN 18
#define TX_PIN 17


test_msgs_UART_Channels msg;
test_msgs_Error_Message errors;


void receiverSetup()
{
  Serial.begin(9600);
  Serial2.begin(11500, SERIAL_8N1, RX_PIN, TX_PIN); // RX, TX
  delay(1000);
  Serial.println("Receiver Started!");
}

void receiverLoop()
{
  if(Serial2.available())
  {
    test_msgs_Error_Message errors = test_msgs_Error_Message_init_zero;

    char payload_str[PROTOBUF_BUFFER_SIZE];

    errors.payload.arg = payload_str;
    errors.payload.funcs.decode = &protobuf_decode_string;
    Serial.println("Decoding...");
    if(protobuf_receive(&Serial2, &errors, test_msgs_Error_Message_fields))
    {
        Serial.println("****************** Receiver ******************");
        Serial.print("ID: ");
        Serial.println(errors.id);
        Serial.print("Data/Payload: ");
        Serial.println(payload_str);
        Serial.println("******************************************");
    } else {
        Serial.println("Decoding failed!");

    }
  } else {
      Serial.println("Serial2 not available...");
  }

  delay(5);
}

void transmitterSetup()
{
  Serial.begin(9600);
  Serial2.begin(11500, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(1000);
  Serial.println("Transmitter Started!");
}

void transmitterLoop()
{
    if(Serial2.available())
    {
      char str[PROTOBUF_BUFFER_SIZE] = "Hello from ESP32 (Transmitter)";
      errors = test_msgs_Error_Message_init_zero;
      errors.id = 42;
      errors.payload.arg = str;
      errors.payload.funcs.encode = &protobuf_encode_string;
      Serial.println("Encoding...");
      if(protobuf_send(&Serial2, &errors, test_msgs_Error_Message_fields))
      {
        Serial.println("****************** Transmitter ******************");
        Serial.println("Protobuf message transmitted !");
        Serial.println("***********************************************");
      } else {
        Serial.println("Encoding failed");
      }
    } else {
      Serial.println("Serial2 not available...");
    }
    delay(20);
}

void setup()
{
  receiverSetup();
  //transmitterSetup();
}


void loop()
{
  receiverLoop();
  //transmitterLoop();
}
