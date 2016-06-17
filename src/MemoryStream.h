/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "StreamProcessor.h"

class MemorySource : public boost::iostreams::source{
	const buffer_t *mem;
	size_t offset;
public:
	MemorySource(const buffer_t *mem): mem(mem), offset(0){}
	std::streamsize read(char *s, std::streamsize n);
};

class MemorySink : public boost::iostreams::sink{
	buffer_t *mem;
public:
	MemorySink(buffer_t *mem): mem(mem){}
	std::streamsize write(const char *s, std::streamsize n);
};

namespace zstreams{

class MemorySource : public InputStream{
	const std::uint8_t *buffer;
	size_t size;
	void work() override;
public:
	MemorySource(const void *buffer, size_t size, Pipeline &parent):
		InputStream(parent),
		buffer(static_cast<const std::uint8_t *>(buffer)),
		size(size){}
};

class MemorySink : public OutputStream{
	std::vector<std::uint8_t> buffer;
	void work() override;
	IGNORE_FLUSH_COMMAND
public:
	MemorySink(Pipeline &parent): OutputStream(parent){}
};


}
