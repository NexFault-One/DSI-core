#pragma once

#include <ModbusRTU.h>
#include "Protocol.h"
#include "tmi/tmi_metrics.h"

#define REG 0


class ModbusProtocol : public Protocol {
public:
    ModbusProtocol(uint8_t de_pin);
    void inject(Injector* injector, uint8_t* data, size_t data_len) override;
    int receive(uint8_t* data, size_t max_len, uint32_t timeout_ms) override;
    void setModbusConfig(uint8_t slave_id, uint8_t func_code, uint16_t address, uint16_t value_or_quantity, bool recalculate_crc);
    void burst_inject(Injector* injector);

private:
    bool init() override;
    uint16_t calculateCRC(uint8_t* buffer, size_t len);
    void buildModbusFrame(uint8_t* buffer, size_t& frame_len);
    
    uint8_t de_pin;

    struct {
        uint8_t slave_id;
        uint8_t func_code;
        uint16_t address;
        uint16_t value_or_quantity;
        bool recalculate_crc;

    } config;
};