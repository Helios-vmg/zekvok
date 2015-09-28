/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "LzmaFilter.h"
#include "Utility.h"

LzmaOutputFilter::LzmaOutputFilter(bool &multithreaded, int compression_level, size_t buffer_size, bool extreme_mode){
	this->lstream = LZMA_STREAM_INIT;

	auto f = !multithreaded ? &LzmaOutputFilter::initialize_single_threaded : &LzmaOutputFilter::initialize_multithreaded;
	multithreaded = (this->*f)(compression_level, buffer_size, extreme_mode);
	
	this->action = LZMA_RUN;
	this->output_buffer.resize(buffer_size);
	this->lstream.next_out = &this->output_buffer[0];
	this->lstream.avail_out = this->output_buffer.size();
	this->bytes_read = 0;
	this->bytes_written = 0;
}

LzmaOutputFilter::~LzmaOutputFilter(){
	lzma_end(&this->lstream);
}

bool LzmaOutputFilter::initialize_single_threaded(int compression_level, size_t buffer_size, bool extreme_mode){
	uint32_t preset = compression_level;
	if (extreme_mode)
		preset |= LZMA_PRESET_EXTREME;
	lzma_ret ret = lzma_easy_encoder(&this->lstream, preset, LZMA_CHECK_NONE);
	if (ret != LZMA_OK){
		const char *msg;
		switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed.";
				break;
			case LZMA_OPTIONS_ERROR:
				msg = "Specified compression level is not supported.";
				break;
			case LZMA_UNSUPPORTED_CHECK:
				msg = "Specified integrity check is not supported.";
				break;
			default:
				msg = "Unknown error.";
				break;
		}
		throw LzmaInitializationException(msg);
	}
	return false;
}

bool LzmaOutputFilter::initialize_multithreaded(int compression_level, size_t buffer_size, bool extreme_mode){
	lzma_mt mt;
	zero_struct(mt);
	mt.flags = 0;
	mt.block_size = 0;
	mt.timeout = 0;
	mt.preset = compression_level;
	if (extreme_mode)
		mt.preset |= LZMA_PRESET_EXTREME;
	mt.filters = 0;
	mt.check = LZMA_CHECK_NONE;
	mt.threads = lzma_cputhreads();
	if (!mt.threads){
		this->initialize_single_threaded(compression_level, buffer_size, extreme_mode);
		return false;
	}
	
	mt.threads = std::max(mt.threads, 4U);

	lzma_ret ret = lzma_stream_encoder_mt(&this->lstream, &mt);

	if (ret != LZMA_OK){
		const char *msg;
		switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed.";
				break;
			case LZMA_OPTIONS_ERROR:
				msg = "Specified filter chain or compression level is not supported.";
				break;
			case LZMA_UNSUPPORTED_CHECK:
				msg = "Specified integrity check is not supported.";
				break;
			default:
				msg = "Unknown error.";
				break;
		}
		throw LzmaInitializationException(msg);
	}
	return true;
}

bool LzmaOutputFilter::pass_data_to_stream(lzma_ret ret, write_callback_t cb, void *ud){
	if (!this->lstream.avail_out || ret == LZMA_STREAM_END) {
		size_t write_size = this->output_buffer.size() - this->lstream.avail_out;

		auto temp = &this->output_buffer[0];
		while (write_size){
			auto w = cb(ud, temp, write_size);
			this->bytes_written += w;
			temp += w;
			write_size -= w;
		}

		this->lstream.next_out = &this->output_buffer[0];
		this->lstream.avail_out = this->output_buffer.size();
	}

	if (ret != LZMA_OK){
		if (ret != LZMA_STREAM_END){
			const char *msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed.";
				break;
			case LZMA_DATA_ERROR:
				msg = "File size limits exceeded.";
				break;
			default:
				msg = "Unknown error.";
				break;
			}
			throw LzmaOperationException(msg);
		}
		return false;
	}
	return true;
}

std::streamsize LzmaOutputFilter::internal_write(write_callback_t cb, void *ud, const void *buffer, std::streamsize length){
	auto x0 = this->bytes_written;
	lzma_ret lret;
	do{
		if (this->lstream.avail_in == 0){
			if (!length)
				break;
			this->bytes_read += length;
			this->lstream.next_in = (const uint8_t *)buffer;
			this->lstream.avail_in = length;
			length -= this->lstream.avail_in;
		}
		lret = lzma_code(&this->lstream, this->action);
	}while (this->pass_data_to_stream(lret, cb, ud));
	return this->bytes_written - x0;
}

bool LzmaOutputFilter::internal_flush(write_callback_t cb, void *ud){
	if (this->action != LZMA_RUN)
		return false;
	this->action = LZMA_FINISH;
	while (this->pass_data_to_stream(lzma_code(&this->lstream, this->action), cb, ud));
	return true;
}

LzmaInputFilter::LzmaInputFilter(size_t buffer_size){
	this->lstream = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_stream_decoder(&this->lstream, UINT64_MAX, LZMA_IGNORE_CHECK);
	if (ret != LZMA_OK){
		const char *msg;
		switch (ret) {
		case LZMA_MEM_ERROR:
			msg = "Memory allocation failed.\n";
			break;
		case LZMA_OPTIONS_ERROR:
			msg = "Unsupported decompressor flags.\n";
			break;
		default:
			msg = "Unknown error.\n";
			break;
		}
		throw LzmaInitializationException(msg);
	}
	this->action = LZMA_RUN;
	this->input_buffer.resize(buffer_size);
	this->bytes_read = 0;
	this->bytes_written = 0;
	this->queued_buffer = &this->input_buffer[0];
	this->queued_bytes = 0;
}

LzmaInputFilter::~LzmaInputFilter(){
	lzma_end(&this->lstream);
}

std::streamsize LzmaInputFilter::internal_read(read_callback_t cb, void *ud, void *buffer, std::streamsize size){
	if (this->at_eof)
		return -1;
	size_t ret = 0;
	this->lstream.next_out = (uint8_t *)buffer;
	this->lstream.avail_out = size;
	while (this->lstream.avail_out){
		if (this->lstream.avail_in == 0){
			this->lstream.next_in = &this->input_buffer[0];
			this->lstream.avail_in = cb(ud, &this->input_buffer[0], this->input_buffer.size());
			this->bytes_read += this->lstream.avail_in;
		}
		if (this->lstream.avail_in == 0)
			this->action = LZMA_FINISH;
		lzma_ret ret_code = lzma_code(&this->lstream, action);
		if (ret_code != LZMA_OK) {
			if (ret_code == LZMA_STREAM_END)
				break;
			const char *msg;
			switch (ret_code) {
				case LZMA_MEM_ERROR:
					msg = "Memory allocation failed.";
					break;
				case LZMA_FORMAT_ERROR:
					msg = "The input is not in the .xz format.";
					break;
				case LZMA_OPTIONS_ERROR:
					msg = "Unsupported compression options.";
					break;
				case LZMA_DATA_ERROR:
					msg = "Compressed file is corrupt.";
					break;
				case LZMA_BUF_ERROR:
					msg = "Compressed file is truncated or otherwise corrupt.";
					break;
				default:
					msg = "Unknown error.";
					break;
			}
			throw LzmaOperationException(msg);
		}
	}
	ret = size - this->lstream.avail_out;
	this->bytes_written += ret;
	this->at_eof = !ret && size;
	return ret;
}
