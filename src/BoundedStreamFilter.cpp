/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "BoundedStreamFilter.h"

namespace zstreams{
void ByteCounterSink::work(){
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

void BoundedSource::work(){
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
		if (data.size == bytes_read){
			this->write(segment);
		}else{
			auto copy = segment.clone_and_trim(bytes_read);
			segment.skip_bytes(bytes_read);
			this->source_queue->put_back(segment);
			this->write(copy);
		}
		this->write(eof);
		break;
	}
}

}
