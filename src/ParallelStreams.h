/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "System/Threads.h"
#include "SimpleTypes.h"

extern const size_t default_buffer_size;

enum class SegmentType{
	Undefined,
	Eof,
	Data,
	Flush,
	FullFlush,
};

class StreamSegment{
	SegmentType type;
	std::shared_ptr<buffer_t> data;
public:
	size_t offset = 0;
	StreamSegment(const std::shared_ptr<buffer_t> &data): type(SegmentType::Data), data(data){}
	StreamSegment(SegmentType type = SegmentType::Eof): type(type){}
	SegmentType get_type() const{
		return this->type;
	}
	const std::shared_ptr<buffer_t> &get_data() const{
		return this->data;
	}
	static StreamSegment alloc(){
		StreamSegment ret(SegmentType::Data);
		ret.data.reset(new buffer_t(default_buffer_size));
		return ret;
	}
};
