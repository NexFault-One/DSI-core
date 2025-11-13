#include "proto_communication.h"


bool protobuf_send(HardwareSerial* serial, const void* msg, const pb_msgdesc_t* fields)
{
    uint8_t buffer[PROTOBUF_BUFFER_SIZE];

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if(!pb_encode(&stream, fields, msg))
    {
        return false;
    }
    serial->write(buffer, stream.bytes_written);

    serial->flush();
    return true;

}

bool protobuf_receive(HardwareSerial* serial, void* msg, const pb_msgdesc_t* fields) 
{
    uint8_t buffer[PROTOBUF_BUFFER_SIZE];
    size_t bytes_read = 0;

    while (serial->available() && bytes_read < PROTOBUF_BUFFER_SIZE) {
        buffer[bytes_read++] = serial->read();
    }

    if(bytes_read == 0)
    {
        return false;
    }

    pb_istream_t stream = pb_istream_from_buffer(buffer, bytes_read);
    return pb_decode(&stream, fields, msg);
}

bool protobuf_decode_string(pb_istream_t* stream, const pb_field_t* field, void** arg)
{
    char* out_str = (char*)(*arg);
    if (!out_str){
        return false;
    }

    size_t len = stream->bytes_left;
    if(len > PROTOBUF_BUFFER_SIZE - 2){
        len = PROTOBUF_BUFFER_SIZE - 2;
    }

    if(!pb_read(stream, (pb_byte_t*)out_str, len))
    {
        return false;
    }
    out_str[len] = '\0';
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