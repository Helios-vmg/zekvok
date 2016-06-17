/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Filters.h"
#include "StreamProcessor.h"

class ByteCounterOutputFilter : public OutputFilter{
	std::uint64_t *bytes_processed;
public:
	ByteCounterOutputFilter(std::ostream &stream, std::uint64_t *dst):
		OutputFilter(stream),
		bytes_processed(dst){}
	std::streamsize write(const char *s, std::streamsize n) override{
		*this->bytes_processed += n;
		return this->next_write(s, n);
	}
	bool flush() override{
		return this->internal_flush();
	}
};

class BoundedInputFilter : public InputFilter{
	std::streamsize bytes_read,
		simulated_length;
public:
	BoundedInputFilter(std::istream &stream, std::streamsize length):
		InputFilter(stream),
		bytes_read(0),
		simulated_length(length){}
	std::streamsize read(char *s, std::streamsize n) override;
};

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
