/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once
#include "Filters.h"
#include "Utility.h"

const size_t default_buffer_size = 1 << 12;

class LzmaInitializationException : public std::exception{
	std::string message;
public:
	LzmaInitializationException(const char *msg) : message(msg){}
	const char *what() const override{
		return this->message.c_str();
	}
};

class LzmaOperationException : public std::exception{
	std::string message;
public:
	LzmaOperationException(const char *msg) : message(msg){}
	const char *what() const override{
		return this->message.c_str();
	}
};

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
public:
	LzmaOutputFilter(std::ostream &stream, bool *multithreaded, int compression_level = 7, size_t buffer_size = default_buffer_size, bool extreme_mode = false);
	std::streamsize write(const char *s, std::streamsize n) override;
	bool flush() override;
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
		uint64_t bytes_read,
			bytes_written;
		bool at_eof;
		impl()
			: action(LZMA_RUN)
			, queued_buffer(nullptr)
			, queued_bytes(0)
			, bytes_read(0)
			, bytes_written(0)
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
