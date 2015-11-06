/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class CircularBuffer;
typedef std::uint32_t rolling_checksum_t;

rolling_checksum_t compute_rsync_rolling_checksum(const CircularBuffer &buffer);
rolling_checksum_t compute_rsync_rolling_checksum(const CircularBuffer &buffer, size_t offset, size_t logical_size, rolling_checksum_t previous = 0);
rolling_checksum_t subtract_rsync_rolling_checksum(rolling_checksum_t previous, byte_t byte_to_subtract, size_t size);
rolling_checksum_t add_rsync_rolling_checksum(rolling_checksum_t previous, byte_t byte_to_add, size_t size);
