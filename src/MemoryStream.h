/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

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
