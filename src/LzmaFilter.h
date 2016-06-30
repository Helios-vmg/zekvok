/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Filters.h"
#include "StreamProcessor.h"

namespace zstreams{
	
class LzmaOutputStream : public OutputStream{
	lzma_stream lstream;
	lzma_action action = LZMA_RUN;
	Segment output_segment;

	void reset_segment();
	bool initialize_single_threaded(int, bool);
	bool initialize_multithreaded(int, bool);
	bool pass_data_to_stream(lzma_ret ret);
	void flush_impl() override;
	void work() override;
public:
	LzmaOutputStream(OutputStream &stream, bool *multithreaded, int compression_level = 7, bool extreme_mode = false);
	~LzmaOutputStream();
	const char *class_name() const override{
		return "LzmaOutputStream";
	}
};

class LzmaInputStream : public InputStream{
	lzma_stream lstream;
	lzma_action action = LZMA_RUN;
	bool at_eof = false;

	void work() override;
public:
	LzmaInputStream(InputStream &wrapped_stream);
	~LzmaInputStream();
	const char *class_name() const override{
		return "LzmaInputStream";
	}
};

}
