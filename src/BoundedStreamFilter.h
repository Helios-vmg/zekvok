/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "StreamProcessor.h"

namespace zstreams{

class ByteCounterOutputFilter : public OutputStream{
	streamsize_t &bytes_processed;
	void work() override;
	IGNORE_FLUSH_COMMAND
public:
	ByteCounterOutputFilter(OutputStream &wrapped, streamsize_t &dst): OutputStream(wrapped), bytes_processed(dst){
		this->bytes_processed = 0;
	}
};

class BoundedInputFilter : public InputStream{
	streamsize_t bytes_read,
		simulated_length;
	void work() override;
public:
	BoundedInputFilter(InputStream &stream, streamsize_t length):
		InputStream(stream),
		bytes_read(0),
		simulated_length(length){}
};

}
