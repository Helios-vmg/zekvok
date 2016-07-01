/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "NullStream.h"

namespace zstreams{

void NullSource::work(){
	while (true){
		auto segment = this->pipeline->allocate_segment();
		auto data = segment.get_data();
		memset(data.data, 0, data.size);
		this->write(segment);
	}
}

void NullSink::work(){
	while (this->read().get_type() != SegmentType::Eof);
}

}
