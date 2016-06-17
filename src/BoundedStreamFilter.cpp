/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "BoundedStreamFilter.h"

std::streamsize BoundedInputFilter::read(char *output, std::streamsize length){
	if (this->bytes_read == this->simulated_length)
		return -1;
	length = std::min(length, this->simulated_length - this->bytes_read);
	std::streamsize ret = 0;
	bool bad = false;
	while (length){
		auto temp = this->next_read(output, length);
		if (temp < 0){
			bad = true;
			break;
		}
		ret += temp;
		output += temp;
		length -= temp;
	}
	this->bytes_read += ret;
	return !ret && bad ? -1 : ret;
}

namespace zstreams{
void ByteCounterOutputFilter::work(){
	while (true){
		auto segment = this->read();
		if (segment.get_type() == SegmentType::Eof){
			this->write(segment);
			break;
		}
		this->bytes_processed += segment.get_data().size;
		this->write(segment);
	}
}

void BoundedInputFilter::work(){
	while (true){
		auto segment = this->read();
		if (segment.get_type() == SegmentType::Eof){
			this->write(segment);
			break;
		}
		auto data = segment.get_data();
		auto bytes_read = std::min(this->simulated_length - this->bytes_read, data.size);
		this->bytes_read += bytes_read;
		if (bytes_read == data.size){
			this->write(segment);
			continue;
		}
		Segment eof(SegmentType::Eof);
		if (this->bytes_read == this->simulated_length){
			this->write(eof);
			break;
		}
		auto copy = segment.clone_and_trim(bytes_read);
		segment.skip_bytes(bytes_read);
		this->source_queue->put_back(segment);
		this->write(copy);
		this->write(eof);
		break;
	}
}

}
