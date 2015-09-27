/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once
#include "Filters.h"

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
	lzma_stream lstream;
	lzma_action action;
	std::vector<uint8_t> output_buffer;
	uint64_t bytes_read,
		bytes_written;

	bool initialize_single_threaded(int, size_t, bool);
	bool initialize_multithreaded(int, size_t, bool);
	bool pass_data_to_stream(lzma_ret ret, write_callback_t, void *);
protected:
	std::streamsize write(write_callback_t cb, void *ud, const void *input, std::streamsize length) override;
	bool flush(write_callback_t cb, void *ud) override;
public:
	LzmaOutputFilter(bool &multithreaded, int compression_level = 7, size_t buffer_size = default_buffer_size, bool extreme_mode = false);
	~LzmaOutputFilter();
};

class LzmaInputFilter : public InputFilter{
	lzma_stream lstream;
	lzma_action action;
	std::vector<uint8_t> input_buffer;
	const uint8_t *queued_buffer;
	size_t queued_bytes;
	uint64_t bytes_read,
		bytes_written;
	bool at_eof;

protected:
	std::streamsize read(read_callback_t cb, void *ud, void *output, std::streamsize length) override;
public:
	LzmaInputFilter(size_t buffer_size = default_buffer_size);
	~LzmaInputFilter();
};
