/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "MemoryStream.h"

std::streamsize MemorySource::read(char *s, std::streamsize n){
	if (this->offset >= this->mem->size())
		return -1;
	n = std::min<size_t>(n, this->mem->size() - this->offset);
	memcpy(s, &(*this->mem)[this->offset], n);
	this->offset += n;
	return n;
}

std::streamsize MemorySink::write(const char *s, std::streamsize n){
	auto size = this->mem->size();
	this->mem->resize(size + n);
	memcpy(&(*this->mem)[size], s, n);
	return n;
}
