/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "RollingChecksum.h"
#include <cstdint>
#include <cstring>

struct rsync_table_item{
	rolling_checksum_t rolling_checksum;
	std::uint8_t complex_hash[20];
	std::uint64_t file_offset;

	bool operator<(const rsync_table_item &b) const{
		if (this->rolling_checksum < b.rolling_checksum)
			return true;
		if (this->rolling_checksum > b.rolling_checksum)
			return false;
		int cmp = memcmp(this->complex_hash, b.complex_hash, sizeof(this->complex_hash));
		if (cmp < 0)
			return true;
		if (cmp > 0)
			return false;
		return this->file_offset < b.file_offset;
	}
};

struct rsync_command{
	std::uint64_t file_offset,
		length;
	bool copy_from_old() const{
		return !!(this->length >> 63);
	}
	void set_copy_from_old(bool x){
		if (x)
			this->length |= mask;
		else
			this->length &= ~mask;
	}
	std::uint64_t get_length() const{
		return this->length & ~mask;
	}
private:
	static const std::uint64_t mask = (std::uint64_t)1 << 63;
};
