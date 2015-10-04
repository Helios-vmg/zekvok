/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "LzmaFilter.h"
#include "Utility.h"

LzmaOutputFilter::LzmaOutputFilter(
		std::ostream &stream,
		bool *multithreaded,
		int compression_level,
		size_t buffer_size,
		bool extreme_mode
		): OutputFilter(stream){
	this->data.reset(new impl, lzma_freer);
	auto d = this->data.get();
	d->lstream = LZMA_STREAM_INIT;

	auto f = !*multithreaded ? &LzmaOutputFilter::initialize_single_threaded : &LzmaOutputFilter::initialize_multithreaded;
	*multithreaded = (this->*f)(compression_level, buffer_size, extreme_mode);
	
	d->action = LZMA_RUN;
	d->output_buffer.resize(buffer_size);
	d->lstream.next_out = &d->output_buffer[0];
	d->lstream.avail_out = d->output_buffer.size();
	d->bytes_read = 0;
	d->bytes_written = 0;
}

bool LzmaOutputFilter::initialize_single_threaded(int compression_level, size_t buffer_size, bool extreme_mode){
	uint32_t preset = compression_level;
	if (extreme_mode)
		preset |= LZMA_PRESET_EXTREME;
	lzma_ret ret = lzma_easy_encoder(&this->data->lstream, preset, LZMA_CHECK_NONE);
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

	lzma_ret ret = lzma_stream_encoder_mt(&this->data->lstream, &mt);

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

bool LzmaOutputFilter::pass_data_to_stream(lzma_ret ret){
	auto d = this->data.get();
	if (!d->lstream.avail_out || ret == LZMA_STREAM_END) {
		size_t write_size = d->output_buffer.size() - d->lstream.avail_out;

		auto temp = &d->output_buffer[0];
		if (this->next_write((const char *)temp, write_size) < 0)
			return false;
		d->bytes_written += write_size;

		d->lstream.next_out = &d->output_buffer[0];
		d->lstream.avail_out = d->output_buffer.size();
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

std::streamsize LzmaOutputFilter::write(const char *buffer, std::streamsize length){
	auto d = this->data.get();
	lzma_ret lret;
	do{
		if (d->lstream.avail_in == 0){
			if (!length)
				break;
			d->bytes_read += length;
			d->lstream.next_in = (const uint8_t *)buffer;
			d->lstream.avail_in = length;
			length -= d->lstream.avail_in;
		}
		lret = lzma_code(&d->lstream, d->action);
	}while (this->pass_data_to_stream(lret));
	return length;
}

bool LzmaOutputFilter::flush(){
	if (this->data->action != LZMA_RUN)
		return true;
	this->data->action = LZMA_FINISH;
	while (this->pass_data_to_stream(lzma_code(&this->data->lstream, this->data->action)));
	this->stream->flush();
	return this->stream->good();
}

LzmaInputFilter::LzmaInputFilter(std::istream &stream, size_t buffer_size): InputFilter(stream){
	this->data.reset(new impl, lzma_freer);
	auto d = this->data.get();
	d->lstream = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_stream_decoder(&d->lstream, UINT64_MAX, LZMA_IGNORE_CHECK);
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
	d->action = LZMA_RUN;
	d->input_buffer.resize(buffer_size);
	d->bytes_read = 0;
	d->bytes_written = 0;
	d->queued_buffer = &d->input_buffer[0];
	d->queued_bytes = 0;
}

std::streamsize LzmaInputFilter::read(char *buffer, std::streamsize size){
	auto d = this->data.get();
	if (d->at_eof)
		return -1;
	size_t ret = 0;
	d->lstream.next_out = (uint8_t *)buffer;
	d->lstream.avail_out = size;
	while (d->lstream.avail_out){
		if (d->lstream.avail_in == 0){
			d->lstream.next_in = &d->input_buffer[0];
			auto r = this->next_read((char *)&d->input_buffer[0], d->input_buffer.size());
			d->lstream.avail_in = r < 0 ? 0 : r;
			d->bytes_read += d->lstream.avail_in;
		}
		if (d->lstream.avail_in == 0)
			d->action = LZMA_FINISH;
		lzma_ret ret_code = lzma_code(&d->lstream, d->action);
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
	ret = size - d->lstream.avail_out;
	d->bytes_written += ret;
	d->at_eof = !ret && size;
	return ret;
}
