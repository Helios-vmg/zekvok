/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "StreamProcessor.h"

namespace zstreams{

class ByteCounterSink : public Sink{
	streamsize_t &bytes_processed;
	void work() override;
	IGNORE_FLUSH_COMMAND
public:
	ByteCounterSink(Sink &wrapped, streamsize_t &dst): Sink(wrapped), bytes_processed(dst){
		this->bytes_processed = 0;
	}
	const char *class_name() const override{
		return "ByteCounterSink";
	}
};

class BoundedSource : public Source{
	streamsize_t bytes_read,
		simulated_length;
	void work() override;
public:
	BoundedSource(Source &stream, streamsize_t length):
		Source(stream),
		bytes_read(0),
		simulated_length(length){}
	const char *class_name() const override{
		return "BoundedSource";
	}
};

}
