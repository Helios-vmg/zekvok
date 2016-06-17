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

namespace zstreams{

void MemorySource::work(){
	while (this->size){
		auto segment = this->parent->allocate_segment();
		auto data = segment.get_data();
		data.size = std::min(data.size, this->size);
		memcpy(data.data, this->buffer, data.size);
		segment.trim_to_size(data.size);
		this->buffer += data.size;
		this->size -= data.size;
		this->write(segment);
	}
	Segment s(SegmentType::Eof);
	this->write(s);
}

void MemorySink::work(){
	while (true){
		auto segment = this->read();
		auto data = segment.get_data();
		auto n = this->buffer.size();
		this->buffer.resize(n + data.size);
		memcpy(&this->buffer[n], data.data, data.size);
	}
}

}
