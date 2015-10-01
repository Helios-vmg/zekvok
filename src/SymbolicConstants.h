#pragma once

#include "SimpleTypes.h"

const stream_id_t invalid_stream_id = 0;
const stream_id_t first_valid_stream_id = invalid_stream_id + 1;
const differential_chain_id_t invalid_differential_chain_id = 0;
const differential_chain_id_t first_valid_differential_chain_id = invalid_differential_chain_id + 1;
const version_number_t invalid_version_number = -1;

namespace config{
const bool include_typehashes = true;
}
