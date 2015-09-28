/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "BoundedStreamFilter.h"

std::streamsize BoundedInputFilter::internal_read(read_callback_t cb, void *ud, void *output, std::streamsize length){
	length = std::min(length, this->simulated_length - this->bytes_read);
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
	this->bytes_read += ret;
	return !ret && eof ? -1 : ret;
}
