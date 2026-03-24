#include "proto_communication.h"


bool protobuf_send(HardwareSerial* serial, const void* msg, const pb_msgdesc_t* fields)
{
    uint8_t buffer[PROTOBUF_BUFFER_SIZE];

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if(!pb_encode(&stream, fields, msg))
    {
        return false;
    }

    uint16_t len = (uint16_t)stream.bytes_written;
    if (len == 0 || len > PROTOBUF_BUFFER_SIZE) return false;

    // length prefix (little-endian)
    uint8_t prefix[2];
    prefix[0] = (uint8_t)(len & 0xFF);
    prefix[1] = (uint8_t)((len >> 8) & 0xFF);

    serial->write(prefix, 2);
    serial->write(buffer, len);
    serial->flush();
    return true;
}

bool protobuf_receive(HardwareSerial* serial, void* msg, const pb_msgdesc_t* fields)
{
    uint8_t buffer[PROTOBUF_BUFFER_SIZE];

    // Wait for at least 2 bytes for the length prefix
    unsigned long start = millis();
    while (serial->available() < 2) {
        if (millis() - start > 200) {
            //Serial.println("[protobuf_receive] Timeout waiting for length prefix");
            return false;
        }
    }

    // Read the 2-byte length prefix (little-endian)
    uint16_t msg_len = 0;
    msg_len  = serial->read();
    msg_len |= (serial->read() << 8);

    if (msg_len == 0 || msg_len > PROTOBUF_BUFFER_SIZE) {
        Serial.printf("[protobuf_receive] Invalid msg_len=%u\n", msg_len);
        return false;
    }

    // Read exactly msg_len bytes with a moving timeout
    size_t bytes_read = 0;
    start = millis();
    while (bytes_read < msg_len) {
        if (serial->available()) {
            buffer[bytes_read++] = serial->read();
            start = millis();  // reset timeout after each byte
        }
        if (millis() - start > 200) {
            Serial.printf("[protobuf_receive] Timeout after %u bytes (expected %u)\n",
                          bytes_read, msg_len);
            return false;
        }
    }

    Serial.printf("[protobuf_receive] Received %u bytes to decode:\n", msg_len);
    // DUMP THE RAW HEX
    //for(size_t i = 0; i < msg_len; i++) {
    //    Serial.printf("%02X ", buffer[i]);
    //    if((i+1) % 16 == 0) Serial.println();
    //}
    Serial.println();
    
    pb_istream_t stream = pb_istream_from_buffer(buffer, msg_len);
    bool status = pb_decode(&stream, fields, msg);
    if (!status) {
        Serial.printf("[protobuf_receive] pb_decode failed: %s\n", PB_GET_ERROR(&stream));
    }
    return status;
}

bool protobuf_decode_string(pb_istream_t* stream, const pb_field_t* field, void** arg)
{
    Serial.println("[decode_string] callback called!");
    char* out_str = (char*)(*arg);
    if (!out_str){
        Serial.println("[decode_string] ERROR: out_str is NULL!");
        return false;
    }
    
    size_t len = stream->bytes_left;
    Serial.printf("[decode_string] bytes_left=%u\n", len);
    
    if(len == 0) {
        Serial.println("[decode_string] WARNING: zero bytes to read!");
        out_str[0] = '\0';
        return true;  // Not necessarily an error
    }
    
    if(len > PROTOBUF_BUFFER_SIZE - 1){
        Serial.printf("[decode_string] Truncating from %u to %u\n", 
                     len, PROTOBUF_BUFFER_SIZE - 1);
        len = PROTOBUF_BUFFER_SIZE - 1;
    }
    
    if(!pb_read(stream, (pb_byte_t*)out_str, len))
    {
        Serial.println("[decode_string] pb_read FAILED!");
        return false;
    }
    
    out_str[len] = '\0';
    Serial.printf("[decode_string] Successfully decoded: '%s' (%u bytes)\n", 
                  out_str, len);
    return true;
}

bool protobuf_encode_string(pb_ostream_t* stream, const pb_field_t* field, void* const* arg)
{
    const char* str = (const char*)(*arg);
    if (!str)
    {
        return false;
    }

    if(!pb_encode_tag_for_field(stream, field))
    {
        return false;
    }

    return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}