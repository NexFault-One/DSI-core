#include "protocols/ModbusProtocol.h"


ModbusProtocol::ModbusProtocol(uint8_t de_pin) : de_pin(de_pin)
{
    // default values
    config.slave_id = 1;
    config.func_code = 0x03;
    config.address = 100;
    config.value_or_quantity = 1;
    config.recalculate_crc = false;

    if(!init())
    {
        Serial.println("[Modbus] failed to initialize");
    }
}

uint16_t ModbusProtocol::calculateCRC(uint8_t* buffer, size_t len)
{
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < len ; i++)
    {
        crc^=(uint16_t)buffer[i];
        for(int j = 8; j!=0; j--)
        {
            if((crc&0x0001) != 0)
            {
                crc >>= 1;
                crc^=0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void ModbusProtocol::buildModbusFrame(uint8_t* buffer, size_t& frame_len)
{
    frame_len = 0;

    // byte 0: Slave_ID (address of the slave device)
    buffer[frame_len++] = config.slave_id;

    // byte 1: function code (which operation?)
    buffer[frame_len++] = config.func_code;

    // bytes 2-3: starting address (big-endian)
    buffer[frame_len++] = (config.address >> 8) & 0xFF;
    buffer[frame_len++] = config.address & 0xFF;

    // bytes 4-5 value or quantity (also big-endian)
    buffer[frame_len++] = (config.value_or_quantity >> 8) & 0xFF;
    buffer[frame_len++] = config.value_or_quantity & 0xFF;

    // crc in inject
}

void ModbusProtocol::setModbusConfig(uint8_t slave_id, uint8_t func_code, uint16_t address, uint16_t value_or_quantity, bool recalculate_crc)
{
    config.slave_id = slave_id;
    config.func_code = func_code;
    config.address = address;
    config.value_or_quantity = value_or_quantity;
    config.recalculate_crc = recalculate_crc;

    Serial.println("[ModbusProtocol] Configuration updated: ");
    Serial.printf(" Slave ID: %d\n", slave_id);
    Serial.printf(" Function code: 0x%02X ", func_code);

    // Print function code description
    switch(func_code) {
        case 0x01: Serial.println("(Read Coils)"); break;
        case 0x02: Serial.println("(Read Discrete Inputs)"); break;
        case 0x03: Serial.println("(Read Holding Registers)"); break;
        case 0x04: Serial.println("(Read Input Registers)"); break;
        case 0x05: Serial.println("(Write Single Coil)"); break;
        case 0x06: Serial.println("(Write Single Register)"); break;
        case 0x0F: Serial.println("(Write Multiple Coils)"); break;
        case 0x10: Serial.println("(Write Multiple Registers)"); break;
        default: Serial.println("(Unknown)"); break;
    }
    
    Serial.printf("  Address: %d (0x%04X)\n", address, address);
    Serial.printf("  Value/Quantity: %d (0x%04X)\n", value_or_quantity, value_or_quantity);
    Serial.printf("  CRC Mode: %s\n", recalculate_crc ? "RECALCULATE (Security Test)" : "CORRUPT (Fault Test)");
}

// data_len is not used 
// todo: for when the data is string
void ModbusProtocol::inject(Injector* injector, uint8_t* data, size_t data_len)
{
    //if(data == nullptr)
    //{
    //    Serial.println("[ModbusProtocol] NULL buffer provided!");
    //    return;
    //}

    // for string
    uint8_t buffer[512];
    size_t frame_len = 0;


    Serial.println();
    Serial.println("--------------------------------------------------------------");
    Serial.println("|                    MODBUS PROTOCOL                         |");
    Serial.println("--------------------------------------------------------------");
    Serial.println("-------------- RTU FRAME CONSTRUCTION --------------");
    // lets build the modbus frame
    buildModbusFrame(buffer, frame_len);
    Serial.printf("[ModbusProtocol] Built Modbus frame (no CRC yet):\n");
    Serial.printf("  Slave ID:       0x%02X (%d)\n", buffer[0], buffer[0]);
    Serial.printf("  Function Code:  0x%02X\n", buffer[1]);
    Serial.printf("  Start Address:  0x%04X (%d)\n", 
                  (buffer[2] << 8) | buffer[3], (buffer[2] << 8) | buffer[3]);
    Serial.printf("  Value/Quantity: 0x%04X (%d)\n", 
                  (buffer[4] << 8) | buffer[5], (buffer[4] << 8) | buffer[5]);
    Serial.printf("  Frame length:   %d bytes\n", frame_len);
    
    Serial.print("  Raw bytes(debug): ");
    for (size_t i = 0; i < frame_len; i++) {
        Serial.printf("%02X ", buffer[i]);
    }
    Serial.println();

    // add the CRC to the original message
    uint16_t original_crc = calculateCRC(buffer, frame_len);
    buffer[frame_len++] = original_crc & 0xFF;
    buffer[frame_len++] = (original_crc >> 8) & 0xFF;
    Serial.println();
    Serial.println("-------------- CRC CALCULATION --------------");
    Serial.printf("[ModbusProtocol] Calculated CRC: 0x%04X\n", original_crc);
    Serial.print("  Raw Bytes Frame WITH CRC (debug): ");
    for (size_t i = 0; i < frame_len; i++) {
        Serial.printf("%02X ", buffer[i]);
    }
    Serial.println();

    // apply injection step

    size_t injected_len = frame_len;

    // give injector entire frame or just the payload (data)
    bool payload_only = true;

    // if payload only true
    size_t new_injected_len = 2;

    if(injector != nullptr)
    {
        Serial.println("-------------- FAULT INJECTION ----------");
        Serial.println("[ModbusProtocol] Applying fault injection...");
        if(payload_only)
        {
            new_injected_len = injector->inject(&buffer[4], 2);
            Serial.print(" CORRUPTED Frame AFTER injection (debug):");
            for (size_t i = 0;i<new_injected_len;i++)
            {
                Serial.printf("%02X ", buffer[i]);
            }
            Serial.println();
        } else {
            injected_len = injector->inject(buffer, frame_len);
            Serial.print(" CORRUPTED Frame AFTER injection (debug):");
            for (size_t i = 0;i<injected_len;i++)
            {
                Serial.printf("%02X ", buffer[i]);
            }
            Serial.println();
        }
        if(injected_len > 512)
        {
            Serial.printf("[ModbusProtocol] ERROR: Injected length (%d) exceeds buffer size (512)!\n", 
                         injected_len);
            injected_len = frame_len;
        }
    }

    //only if payload is true
    size_t total_msg_len = 2 + 2 + new_injected_len;

    // check if we recalculate CRC mode or not
    if(config.recalculate_crc)
    {
        Serial.println("[ModbusProtocol] Mode: RECALCULATE CRC");
        
        if(injected_len >= 2)
        {
            injected_len -=2;
        } 

        // todo: update when payload is not true
        uint16_t new_crc = calculateCRC(buffer, total_msg_len);
        buffer[total_msg_len++] = new_crc & 0xFF; // crc low 
        buffer[total_msg_len++] = (new_crc >> 8) & 0xFF; // crc high

        Serial.printf("  New CRC: 0x%04X (recalculated on injected data)\n", new_crc);
    } else {
        Serial.println("[ModbusProtocol] Mode: Keep corrupted CRC");
    }

    // transmit using Modbus & storing for protobuf
    Serial.println();
    char final_frame[128];
    memset(final_frame, 0, sizeof(final_frame));
    size_t offset = 0;
    Serial.print("[ModbusProtocol] Final Frame: ");
    for (size_t i = 0; i<total_msg_len;i++)
    {
        Serial.printf("%02X ", buffer[i]);
        // storing into final_frame variable for protobuf
        int written = snprintf(&final_frame[offset], sizeof(final_frame) - offset, "%02X ", buffer[i]);
        if (written) < 0
            break;
        if((size_t)written >= (sizeof(final_frame) - offset))
        {
            offset = sizeof(final_frame) - 1;
            break;
        }
        offset += (size_t)written;
    }
    if(offset > 0 && final_frame[offset-1] == ' ')
    {
        final_frame[offset-1] = '\0';
    } else {
        final_frame[sizeof(final_frame)-1] = '\0';
    }
    
    // copying into protobuf
    TMI_LockReport();
    strncpy(tmi_data.report.final_frame, final_frame, sizeof(tmi_data.report.final_frame));
    tmi_data.report.final_frame[sizeof(tmi_data.report.final_frame)-1] = '\0';
    // total bytes sent
    tmi_data.report.bytes_transmitted = total_msg_len;
    TMI_UnlockReport();

    Serial.println();
    Serial.printf("[ModbusProtocol] Total: %d bytes\n", total_msg_len);

    // enable tx mode for rs-485
    digitalWrite(de_pin, HIGH);
    delay(1);

    Serial1.write(buffer, total_msg_len);
    Serial1.flush();

    delay(1);

    // rx mode
    digitalWrite(de_pin, LOW);

    Serial.println("[ModbusProtocol] Transmitted to UUT via Serial1");
    Serial.println("--------------------------------------------------------------");
    Serial.println();
}
// only removes the logs for now
// TODO: make it send a bunch of X injections (burst count) simultaneously
void ModbusProtocol::burst_inject(Injector* injector)
{
    uint8_t buffer[512];
    size_t frame_len = 0;

    buildModbusFrame(buffer, frame_len); 

    size_t new_injected_len = 2;
    if(injector != nullptr) {
        new_injected_len = injector->inject(&buffer[4], 2);
    }

    size_t total_msg_len = 4 + new_injected_len;

    if(config.recalculate_crc) {
        uint16_t new_crc = calculateCRC(buffer, total_msg_len);
        buffer[total_msg_len++] = new_crc & 0xFF;
        buffer[total_msg_len++] = (new_crc >> 8) & 0xFF;
    }

    digitalWrite(de_pin, HIGH);
    // delay(1); // Keep this for hardware stability, but only if needed
    Serial1.write(buffer, total_msg_len);
    Serial1.flush();
    digitalWrite(de_pin, LOW);
    Serial.print("[ModbusProtocol] Final Frame: ");
    for (size_t i = 0; i<total_msg_len;i++)
    {
        Serial.printf("%02X ", buffer[i]);
    }

    Serial.println();
    Serial.println("[ModbusProtocol] Transmitted to UUT via Serial1");
}

int ModbusProtocol::receive(uint8_t* data, size_t max_len, uint32_t timeout_ms)
{

    digitalWrite(de_pin, LOW);
    

    return 0;
}

bool ModbusProtocol::init()
{

    pinMode(de_pin, OUTPUT);
    digitalWrite(de_pin, LOW);
    return true;
}