#pragma once

#include <Arduino.h>
#include "pb_encode.h"
#include <memory>
#include "pb_decode.h"
#include "pb.h"
#include "injectors/Injector.h"

#define PROTOBUF_BUFFER_SIZE 128

// non callback protobuf messages (integers etc..)
bool protobuf_send(HardwareSerial* serial, const void* msg, const pb_msgdesc_t* fields);
bool protobuf_receive(HardwareSerial* serial, void* msg, const pb_msgdesc_t* fields);

// callback (strings)
bool protobuf_encode_string(pb_ostream_t* stream, const pb_field_t* field, void* const* arg);
bool protobuf_decode_string(pb_istream_t* stream, const pb_field_t* field, void** arg);
