/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "BoundedStreamFilter.h"

std::streamsize BoundedInputFilter::read(char *output, std::streamsize length){
	length = std::min(length, this->simulated_length - this->bytes_read);
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
	this->bytes_read += ret;
	return !ret && bad ? -1 : ret;
}
