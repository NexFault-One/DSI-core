#include "tasks/dsi_tasks.h"

extern "C" {
    #include "proto_msgs/uart_data.pb.h"
    #include "pb_decode.h"
    #include "pb_encode.h"
}

QueueHandle_t uartQueue;
QueueHandle_t modbusQueue;

// use dsi_message.h later when multiple transport protocols are used instead of passing raw buffers of decoded protobuf msgs

// create the Injector pointer for the corresponding injection type selected
static std::unique_ptr<Injector> createInjector(const nxf1_v1_DsiCommand &command)
{
  switch (command.inj_type)
  {
    case nxf1_v1_InjectionType_INJ_BYTE_DROP:
      Serial.println("[DSI] Creating ByteDrop Injector");
      Serial.printf("[DEBUG] which_params=%d (expect %d)\n",
              command.which_params, nxf1_v1_DsiCommand_byte_drop_tag);
      Serial.printf("[DEBUG] start_offset=%u length=%u payload='%s'\n",
              command.params.byte_drop.start_offset,
              command.params.byte_drop.length,
              command.params.byte_drop.payload);
      Serial.println("[DSI] BytesDrop command received");
      Serial.print("Length (Num of Bytes to drop): ");
      Serial.println(command.params.byte_drop.length);
      Serial.print("Offset (everyN): ");
      Serial.println(command.params.byte_drop.start_offset);
      Serial.print("Original message: ");
      Serial.println(command.params.byte_drop.payload);
      return std::make_unique<ByteDropInjector>(command.params.byte_drop.length, command.params.byte_drop.start_offset);
    case nxf1_v1_InjectionType_INJ_BIT_FLIP:
      Serial.println("[DSI] Creating BitFlip Injector");
      if(command.params.bit_flip.mode == nxf1_v1_BitFlipMode_BITFLIP_RANDOM) 
      {
        Serial.println("Mode: RANDOM");
        Serial.print("Number of bits to drop (RANDOM ONLY): ");
        Serial.println(command.params.bit_flip.bits_drop); 
        Serial.print("Original message: ");
        Serial.println(command.params.bit_flip.payload);
        return std::make_unique<BitFlipInjector>(BitFlipMode::RANDOM, 0, command.params.bit_flip.bits_drop); 
      }
      else 
      { 
        Serial.println("Mode: PERIODIC");
        Serial.print("every_n (PERIODIC ONLY): ");
        Serial.println(command.params.bit_flip.every_n_p);
        Serial.print("Original message: ");
        Serial.println(command.params.bit_flip.payload);
        return std::make_unique<BitFlipInjector>(BitFlipMode::PERIODIC, command.params.bit_flip.every_n_p, 0); 
      }
    case nxf1_v1_InjectionType_INJ_PHANTOM_BYTE:
      Serial.println("[DSI] Creating PhantomByte Injector");
      if(command.params.phantom_byte.mode == nxf1_v1_PhantomByteMode_PHANTOM_RANDOM)
      {
        Serial.println("Mode: RANDOM");
        Serial.printf("Phantom Byte to be introduced: 0x%02X \n", command.params.phantom_byte.byte_value);
        return std::make_unique<PhantomByteInjector>(PhantomByteMode::RANDOM, command.params.phantom_byte.byte_value, 0);
      } else 
      {
        Serial.println("Mode: MANUAL");
        Serial.printf("Phantom Byte to be introduced: 0x%02X \n", command.params.phantom_byte.byte_value);
        Serial.print("Offset: ");
        Serial.println(command.params.phantom_byte.offset);
        Serial.print("Original message: ");
        Serial.println(command.params.phantom_byte.payload);
        return std::make_unique<PhantomByteInjector>(PhantomByteMode::MANUAL, command.params.phantom_byte.byte_value, command.params.phantom_byte.offset);
      }
    default:
        Serial.println("[DSI] Unknown injection type");
        return nullptr;
  }
}
static void dsi_cmd_task(void *pv)
{
    (void)pv;
    for(;;)
    {
        nxf1_v1_DsiCommand commands = nxf1_v1_DsiCommand_init_zero;
        if(!protobuf_receive(&Serial, &commands, nxf1_v1_DsiCommand_fields))
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        switch(commands.transport)
        {
          case nxf1_v1_TransportType_TRANSPORT_UART:
            xQueueSend(uartQueue, &commands, portMAX_DELAY);
            break;
          case nxf1_v1_TransportType_TRANSPORT_MODBUS:
            xQueueSend(modbusQueue, &commands, portMAX_DELAY);
            break;
          default:
            Serial.println("[DSI_CMD_TASK] Unknown transport...");
            break;
        }
    }
    
}

static void uart_injector_task(void *pv)
{
    QueueHandle_t queue = (QueueHandle_t)pv;   
    UartProtocol uart;

    for(;;)
    {
        nxf1_v1_DsiCommand commands = nxf1_v1_DsiCommand_init_zero;
        if(xQueueReceive(queue, &commands, portMAX_DELAY) == pdTRUE)
        {
            Serial.println("--------------------------------------------------------------");
            Serial.println("|                   DIGITAL SIGNAL INJECTOR                  |");
            Serial.println("--------------------------------------------------------------");
            Serial.println("[DSI_CMD_TASK] DSI Commands Received!");
            Serial.println("[DSI_CMD_TASK] PROTOCOL: UART");
            auto injector = createInjector(commands);

            uint8_t buffer[PROTOBUF_BUFFER_SIZE];
            size_t len = snprintf((char*)buffer, sizeof(buffer), "%s", getPayload(commands));
            uart.inject(injector.get(), buffer, len);

            Serial.println("[DSI_CMD_TASK] Injection complete!");
            vTaskDelay(pdMS_TO_TICKS(10));
            Serial.println("--------------------------------------------------------------");
        }
    }

}
static void modbus_injector_task(void* pv)
{
  QueueHandle_t queue = (QueueHandle_t)pv;
  ModbusProtocol modbus(MODBUS_DE);

  for(;;)
  {
    nxf1_v1_DsiCommand commands = nxf1_v1_DsiCommand_init_zero;
    if(xQueueReceive(queue, &commands, portMAX_DELAY) == pdTRUE)
    {
        Serial.println("--------------------------------------------------------------");
        Serial.println("|                   DIGITAL SIGNAL INJECTOR                  |");
        Serial.println("--------------------------------------------------------------");
        Serial.println("[DSI_CMD_TASK] DSI Commands Received!");
        Serial.println("[DSI_CMD_TASK] PROTOCOL: MODBUS RTU");

        uint32_t duration_ms = commands.duration_ms;
        auto injector = createInjector(commands);

        modbus.setModbusConfig(commands.modbus_config.slave_id, commands.modbus_config.func_code,
                              commands.modbus_config.address, commands.modbus_config.value_or_quantity, 
                              commands.modbus_config.recalculate_crc);

        //modbus.setModbusConfig(1, 0x06, 100, (uint16_t)commands.sensor_value, true);
        uint32_t run_id = 0;
        if(duration_ms == 0)
        {
          while(!commands.stop)
          {
            modbus.inject(injector.get(), nullptr, 0);
            run_id++;
          }
          // for string buffers/payload
          //uint8_t buffer[PROTOBUF_BUFFER_SIZE];
          //size_t len = snprintf((char*)buffer, sizeof(buffer), "%s", getPayload(commands));
          //modbus.inject(injector.get(), buffer, len);
        //// duration injection
        } else {
          Serial.printf("[DSI_CMD_TASK] starting injection for %d ms...\n", duration_ms);
          unsigned long start_time = millis();
          uint32_t injection_count = 0;
          while((millis()-start_time) < duration_ms)
          {
            // for string payload only
            //uint8_t buffer[PROTOBUF_BUFFER_SIZE];
            //size_t len = snprintf((char*)buffer, sizeof(buffer), "%s", getPayload(commands));
          //
            //modbus.inject(injector.get(), buffer, len);
            //injection_count++;

            auto burst_injector = createInjector(commands);
            modbus.burst_inject(burst_injector.get());
            run_id++;
            vTaskDelay(pdMS_TO_TICKS(2));
          }
        }
        Serial.println("[DSI_CMD_TASK] Injection complete!");
        vTaskDelay(pdMS_TO_TICKS(10));
        Serial.println("--------------------------------------------------------------");
    }
  }
}

void start_dsi_tasks()
{
    uartQueue = xQueueCreate(5, sizeof(nxf1_v1_DsiCommand));
    modbusQueue = xQueueCreate(5, sizeof(nxf1_v1_DsiCommand));
    if(!uartQueue || !modbusQueue)
    {
      Serial.println("[DSI] Failed to create queues!");
      return;
    }

    xTaskCreate(dsi_cmd_task, "DSI_CMD", 4096, NULL, 3, NULL);
    xTaskCreate(uart_injector_task, "UART_INJ", 4096, uartQueue, 2, NULL);
    xTaskCreate(modbus_injector_task, "MODBUS_INJ", 4096, modbusQueue, 2, NULL);
}