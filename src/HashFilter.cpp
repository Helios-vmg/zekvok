/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "HashFilter.h"
#include "Utility.h"

size_t HashCalculator::get_hash_length() const{
	return this->hash->DigestSize();
}

void HashCalculator::get_result(void *buffer, size_t max_length){
	if (max_length >= this->get_hash_length()){
		this->hash->Final((byte *)buffer);
		return;
	}
	quick_buffer b(this->get_hash_length());
	this->hash->Final((byte *)b.data);
	memcpy(buffer, b.data, max_length);
}

std::streamsize HashOutputFilter::write(const char *input, std::streamsize length){
	this->update(input, length);
	return this->next_write(input, length);
}

std::streamsize HashInputFilter::read(char *output, std::streamsize length){
	auto head = output;
	std::streamsize ret = 0;
	bool bad = false;
	while (length){
		auto temp = this->next_read(output, length);
		if (temp < 0){
			bad = true;
			break;
		}
		ret += temp;
		output = (char *)output + temp;
		length -= temp;
	}
	this->update(head, ret);
	return !ret && bad ? -1 : ret;
}
