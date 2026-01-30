#include "tasks/dsi_tasks.h"

extern "C" {
    #include "proto_msgs/uart_data.pb.h"
    #include "pb_decode.h"
    #include "pb_encode.h"
}

// use dsi_message.h later when multiple transport protocols are used instead of passing raw buffers of decoded protobuf msgs

struct TransportQueues
{
  QueueHandle_t uartQueue;
  QueueHandle_t modbusQueue;
  QueueHandle_t i2cQueue;
};

static void dsi_cmd_task(void *pv)
{
    auto queues = (TransportQueues*)pv;  

    if(queues == NULL)
    {
        Serial.println("[DSI_CMD_TASK] No queue provided!");
        vTaskDelete(NULL);
        return;
    }
    
    for(;;)
    {
        nxf1_v1_DsiCommand commands = nxf1_v1_DsiCommand_init_zero;
        if(!protobuf_receive(&Serial, &commands, nxf1_v1_DsiCommand_fields))
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if(xQueueSend(queues->uartQueue, &commands, portMAX_DELAY) == pdTRUE)
        {
            Serial.println("[DSI_CMD_TASK] Command added to the InjectorQueue");
        }
    }
    
}

static void dsi_injector_task(void *pv)
{
    QueueHandle_t queue = (QueueHandle_t)pv;   
    
    if(queue == NULL)
    {
        Serial.println("[DSI_INJ_TASK] No queue provided!");
        vTaskDelete(NULL);
        return;
    }

    for(;;)
    {
        nxf1_v1_DsiCommand commands = nxf1_v1_DsiCommand_init_zero;
        if(xQueueReceive(queue, &commands, portMAX_DELAY) == pdTRUE)
        {
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

              size_t newlen;
              if(drop_byte != nullptr)
              {
                newlen = drop_byte->inject(buffer, len);
              } else {
                newlen = len;
              }
          
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
              if(commands.params.bit_flip.mode == nxf1_v1_BitFlipMode_BITFLIP_RANDOM)
              {
                Serial.println("RANDOM");
              } else {
                Serial.println("PERIODIC");
              }
              // create injection
              std::unique_ptr<Injector> bit_flip;
              if(commands.params.bit_flip.mode == nxf1_v1_BitFlipMode_BITFLIP_RANDOM)
              {
                bit_flip = std::make_unique<BitFlipInjector>(BitFlipMode::RANDOM, 0, commands.params.bit_flip.bits_drop);
                Serial.print("Number of bits to drop (RANDOM ONLY): ");
                Serial.println(commands.params.bit_flip.bits_drop);
              } else {
                bit_flip = std::make_unique<BitFlipInjector>(BitFlipMode::PERIODIC, commands.params.bit_flip.every_n_p, 0);
                Serial.print("every_n (PERIODIC ONLY): ");
                Serial.println(commands.params.bit_flip.every_n_p);
              }
              Serial.print("Original message: ");
              Serial.println(commands.params.bit_flip.payload);
              size_t len = snprintf((char*)buffer, sizeof(buffer), "%s", commands.params.bit_flip.payload);
              Serial.println();

              // check if payload is empty...
              if(len == 0)
              {
                Serial.println("Payload empty...");
                vTaskDelay(pdMS_TO_TICKS(200));
                return;
              }
              
              // start injection
              Serial.print("[DSI] calling injector on ");
              Serial.print(len);
              Serial.println(" bytes...");

              size_t newlen;
              if(bit_flip != nullptr)
              {
                newlen = bit_flip->inject(buffer, len);
              } else {
                newlen = len;
              }
          
              if(newlen > sizeof(buffer))
              {
                Serial.println("[DSI] injector returned invalid length, clamping...");
                newlen = sizeof(buffer);
              }
          
              Serial2.write(buffer, newlen);
              Serial2.flush();
            } else if (commands.inj_type == nxf1_v1_InjectionType_INJ_PHANTOM_BYTE)
            {
              uint8_t buffer[PROTOBUF_BUFFER_SIZE];
              Serial.println("[DSI] PhantomByte command received");
              Serial.print("Mode: ");
              // if statement
              std::unique_ptr<Injector> phantom_byte;
              if(commands.params.phantom_byte.mode == nxf1_v1_PhantomByteMode_PHANTOM_RANDOM)
              {
                phantom_byte = std::make_unique<PhantomByteInjector>(PhantomByteMode::RANDOM, commands.params.phantom_byte.byte_value, 0);
                Serial.print("Phantom Byte to be introduced: ");
                Serial.println(commands.params.phantom_byte.byte_value);
              } else {
                // 1= 0x01 -- 3 == offset
                phantom_byte = std::make_unique<PhantomByteInjector>(PhantomByteMode::MANUAL, commands.params.phantom_byte.byte_value, commands.params.phantom_byte.offset);
                Serial.printf("Phantom Byte to be introduced: 0x%02X \n", commands.params.phantom_byte.byte_value);
                Serial.print("Offset: ");
                Serial.println(commands.params.phantom_byte.offset);
              }
              Serial.print("Original message: ");
              Serial.println(commands.params.phantom_byte.payload);

              size_t len = snprintf((char*)buffer, sizeof(buffer), "%s", commands.params.phantom_byte.payload);
              
              std::unique_ptr<Protocol> uart_protocol = std::make_unique<UartProtocol>();

              uart_protocol->inject(phantom_byte.get(), buffer, len);
            
            }
            Serial.println("DSI to UUT...");
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

}

void start_dsi_tasks(QueueHandle_t queue)
{
    xTaskCreate(dsi_cmd_task, "DSI_CMD", 4096, queue, 3, NULL);
    xTaskCreate(dsi_injector_task, "DSI_INJ", 4096, queue, 2, NULL);
}