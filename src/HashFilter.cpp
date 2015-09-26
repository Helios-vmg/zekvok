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

std::streamsize HashOutputFilter::write(write_callback_t cb, void *ud, const void *input, std::streamsize length){
	this->update(input, length);
	std::streamsize ret = 0;
	while (length){
		auto temp = cb(ud, input, length);
		ret += temp;
		input = (const char *)input + temp;
		length -= temp;
	}
	return ret;
}

bool HashOutputFilter::flush(write_callback_t cb, void *ud){
	return true;
}

std::streamsize HashInputFilter::read(read_callback_t cb, void *ud, void *output, std::streamsize length){
	auto head = output;
	std::streamsize ret = 0;
	bool eof = false;
	while (length){
		auto temp = cb(ud, output, length);
		if (temp < 0){
			eof = true;
			break;
		}
		ret += temp;
		output = (char *)output + temp;
		length -= temp;
	}
	this->update(output, ret);
	return !ret && eof ? -1 : ret;
}
