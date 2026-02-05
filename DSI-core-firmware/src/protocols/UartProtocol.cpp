#include "protocols/UartProtocol.h"

// ERROR INJECTIONS ARE HANDLED HERE!

UartProtocol::UartProtocol()
{
    //if(!init())
    //{
    //    Serial.print("[UARTProtocol] failed to initialize");
    //}
}

bool UartProtocol::init()
{
    Serial2.begin(115200, SERIAL_8N1, 18, 17);
    Serial.println("[UARTProtocol] initialized");
    return true;
}


void UartProtocol::inject(Injector* injector, uint8_t* data, size_t data_len)
{
    
    Serial.println();
    if(data_len == 0)
    {
      Serial.println("Payload is empty...");
      vTaskDelay(pdMS_TO_TICKS(200));
      return;
    }
    // start injection
    Serial.print("[DSI] calling injector on ");
    Serial.print(data_len);
    Serial.println(" bytes...");
    
    size_t newlen = injector->inject(data, data_len);

    if(newlen > data_len)
    {
      Serial.println("[DSI] injector returned invalid length, clamping...");
      newlen = data_len;
    }

    Serial2.write(data, newlen);
    Serial2.flush();
    
}

int UartProtocol::receive(uint8_t* data, size_t max_len, uint32_t timeout_ms)
{
    uint8_t buffer[512];
    size_t bytes_read = 0;
    unsigned long last_byte_time = millis();
    // keep reading until no new bytes arrive for 50 ms or buffer full
    while ((millis() - last_byte_time) < timeout_ms && bytes_read < 512)
    {
        if (Serial2.available())
        {
            buffer[bytes_read++] = Serial2.read();
            last_byte_time = millis();  // reset timer after each byte
        }
    }
    if (bytes_read == 0)
    {
        return 0;
    }
    Serial.printf("RX bytes (%u):\n", bytes_read);
    for (size_t i = 0; i < bytes_read; ++i)
    {
        Serial.printf("%02X ", buffer[i]);
    }
    Serial.println();
    Serial.print("ASCII message: ");
    for (size_t i = 0; i < bytes_read; ++i)
    {
        Serial.write(buffer[i]);
    }
    Serial.println();

    return bytes_read;
}