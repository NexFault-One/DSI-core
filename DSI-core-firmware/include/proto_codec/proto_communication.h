#ifndef PROTO_COMMUNICATION_H
#define PROTO_COMMUNICATION_H

#include "Nanopb/pb_encode.h"
#include "Nanopb/pb_decode.h"
#include "Nanopb/pb.h"

#define PROTOBUF_BUFFER_SIZE 128

// non callback protobuf messages (integers etc..)
bool protobuf_send(HardwareSerial& serial, const void* msg, const pb_msgdesc_t* fields);
bool protobuf_receive(HardwareSerial& serial, void* msg, const pb_msgdesc_t* fields);

// callback (strings)
bool protobuf_encode_string(pb_ostream_t* stream, const pb_field_t* field, void* const* arg);
bool protobuf_decode_string(pb_istream_t* stream, const pb_field_t* field, void** arg);

#endif