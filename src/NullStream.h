/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Filters.h"
#include "StreamProcessor.h"

class NullOutputStream{
public:
	NullOutputStream(int){}
	typedef char char_type;
	struct category :
		boost::iostreams::sink_tag,
		boost::iostreams::flushable_tag
	{};
	std::streamsize write(const char *s, std::streamsize n){
		return n;
	}
	bool flush(){
		return true;
	}
};

class NullInputStream{
public:
	typedef char char_type;
	typedef boost::iostreams::source_tag category;
	std::streamsize read(char *s, std::streamsize n){
		memset(s, 0, n);
		return n;
	}
};

namespace zstreams{

class NullSource : public InputStream{
	void work() override;
public:
	NullSource(Pipeline &parent): InputStream(parent){}
};

class NullSink : public OutputStream{
	void work() override;
	IGNORE_FLUSH_COMMAND
public:
	NullSink(Pipeline &parent): OutputStream(parent){}
};

}