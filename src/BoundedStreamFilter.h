/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once
#include "Filters.h"

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
