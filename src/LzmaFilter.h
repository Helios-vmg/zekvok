/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Filters.h"
#include "StreamProcessor.h"

const size_t default_buffer_size = 1 << 12;

class LzmaOutputFilter : public OutputFilter{
	struct impl{
		lzma_stream lstream;
		lzma_action action;
		std::vector<uint8_t> output_buffer;
		uint64_t bytes_read,
			bytes_written;
		impl()
				: action(LZMA_RUN)
				, bytes_read(0)
				, bytes_written(0)
		{
			zero_struct(this->lstream);
		}
	};
	
	static void lzma_freer(impl *i){
		if (i){
			lzma_end(&i->lstream);
			delete i;
		}
	}

	std::shared_ptr<impl> data;

	bool initialize_single_threaded(int, size_t, bool);
	bool initialize_multithreaded(int, size_t, bool);
	bool pass_data_to_stream(lzma_ret ret);
	bool internal_flush() override;
public:
	LzmaOutputFilter(std::ostream &stream, bool *multithreaded, int compression_level = 7, size_t buffer_size = default_buffer_size, bool extreme_mode = false);
	~LzmaOutputFilter();
	std::streamsize write(const char *s, std::streamsize n) override;
	std::uint64_t get_bytes_written() const{
		return this->data->bytes_written;
	}
};

class LzmaInputFilter : public InputFilter{
	struct impl{
		lzma_stream lstream;
		lzma_action action;
		std::vector<uint8_t> input_buffer;
		const uint8_t *queued_buffer;
		size_t queued_bytes;
		bool at_eof;
		impl()
			: action(LZMA_RUN)
			, queued_buffer(nullptr)
			, queued_bytes(0)
			, at_eof(false)
		{
			zero_struct(this->lstream);
		}
	};
	static void lzma_freer(impl *i){
		if (i){
			lzma_end(&i->lstream);
			delete i;
		}
	}
	std::shared_ptr<impl> data;

public:
	LzmaInputFilter(std::istream &, size_t buffer_size = default_buffer_size);
	std::streamsize read(char *s, std::streamsize n) override;
};

namespace zstreams{
	
class LzmaOutputStream : public OutputStream{
	lzma_stream lstream;
	lzma_action action = LZMA_RUN;
	Segment output_segment;

	void reset_segment();
	bool initialize_single_threaded(int, size_t, bool);
	bool initialize_multithreaded(int, size_t, bool);
	bool pass_data_to_stream(lzma_ret ret);
	void flush_impl() override;
	void work() override;
public:
	LzmaOutputStream(OutputStream &stream, bool *multithreaded, int compression_level = 7, size_t buffer_size = default_buffer_size, bool extreme_mode = false);
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
	LzmaInputStream(InputStream &wrapped_stream, size_t buffer_size = default_buffer_size);
	~LzmaInputStream();
	const char *class_name() const override{
		return "LzmaInputStream";
	}
};

}
