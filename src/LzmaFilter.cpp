/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "LzmaFilter.h"

LzmaOutputFilter::LzmaOutputFilter(
		std::ostream &stream,
		bool *multithreaded,
		int compression_level,
		size_t buffer_size,
		bool extreme_mode):
		OutputFilter(stream){
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

LzmaOutputFilter::~LzmaOutputFilter(){
	if (*this->copies == 1){
		this->enable_flush();
		// ReSharper disable once CppVirtualFunctionCallInsideCtor
		this->flush();
	}
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
		if (this->next_write(reinterpret_cast<const char *>(temp), write_size) < 0)
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
	auto initial_length = length;
	do{
		if (d->lstream.avail_in == 0){
			if (!length)
				break;
			d->bytes_read += length;
			d->lstream.next_in = reinterpret_cast<const uint8_t *>(buffer);
			d->lstream.avail_in = length;
			length -= d->lstream.avail_in;
		}
		lret = lzma_code(&d->lstream, d->action);
	}while (this->pass_data_to_stream(lret));
	return initial_length;
}

bool LzmaOutputFilter::internal_flush(){
	if (this->data->action != LZMA_RUN)
		return true;
	this->data->action = LZMA_FINISH;
	while (this->pass_data_to_stream(lzma_code(&this->data->lstream, this->data->action)));
	return OutputFilter::internal_flush();
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
	d->lstream.next_out = reinterpret_cast<uint8_t *>(buffer);
	d->lstream.avail_out = size;
	while (d->lstream.avail_out){
		if (d->lstream.avail_in == 0){
			d->lstream.next_in = &d->input_buffer[0];
			auto r = this->next_read(reinterpret_cast<char *>(&d->input_buffer[0]), d->input_buffer.size());
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
	size_t ret = size - d->lstream.avail_out;
	d->bytes_written += ret;
	d->at_eof = !ret && size;
	return ret;
}

namespace zstreams{

LzmaOutputStream::LzmaOutputStream(OutputStream &stream, bool *multithreaded, int compression_level, size_t buffer_size, bool extreme_mode):
	OutputStream(stream){
	zero_struct(this->lstream);
	this->lstream = LZMA_STREAM_INIT;

	auto f = !*multithreaded ? &LzmaOutputStream::initialize_single_threaded : &LzmaOutputStream::initialize_multithreaded;
	*multithreaded = (this->*f)(compression_level, buffer_size, extreme_mode);

	this->action = LZMA_RUN;
	this->reset_segment();
	this->bytes_read = 0;
	this->bytes_written = 0;
}

LzmaOutputStream::~LzmaOutputStream(){
	lzma_end(&this->lstream);
}

void LzmaOutputStream::reset_segment(){
	this->output_segment = this->parent->allocate_segment();
	auto data = this->output_segment.get_data();
	this->lstream.next_out = data.data;
	this->lstream.avail_out = data.size;
}

bool LzmaOutputStream::initialize_single_threaded(int compression_level, size_t buffer_size, bool extreme_mode){
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

bool LzmaOutputStream::initialize_multithreaded(int compression_level, size_t buffer_size, bool extreme_mode){
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

bool LzmaOutputStream::pass_data_to_stream(lzma_ret ret){
	if (!this->lstream.avail_out || ret == LZMA_STREAM_END) {
		size_t write_size = this->output_segment.get_data().size - this->lstream.avail_out;
		this->output_segment.trim_to_size(write_size);
		this->write(this->output_segment);
		this->reset_segment();
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

void LzmaOutputStream::flush_impl(){
	if (this->action != LZMA_RUN)
		return;
	this->action = LZMA_FINISH;
	while (this->pass_data_to_stream(lzma_code(&this->lstream, this->action)));
	Segment eof(SegmentType::Eof);
	this->write(eof);
}

void LzmaOutputStream::work(){
	lzma_ret lret = LZMA_OK;
	Segment segment;
	do{
		if (this->lstream.avail_in == 0){
			segment = this->read();
			if (segment.get_type() == SegmentType::Eof)
				break;
			auto data = segment.get_data();
			if (!data.size)
				continue;
			this->bytes_read += data.size;
			this->lstream.next_in = static_cast<const uint8_t *>(data.data);
			this->lstream.avail_in = data.size;
		}
		lret = lzma_code(&this->lstream, this->action);

	}while (this->pass_data_to_stream(lret));
	this->flush_impl();
}

LzmaInputStream::LzmaInputStream(InputStream &stream, size_t buffer_size):
		InputStream(stream){
	zero_struct(this->lstream);

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
	this->bytes_read = 0;
	this->bytes_written = 0;
}

LzmaInputStream::~LzmaInputStream(){
	lzma_end(&this->lstream);
}

void LzmaInputStream::work(){
	Segment in_segment;
	Segment out_segment;
	while (true){
		if (this->lstream.avail_in == 0 && !this->at_eof){
			in_segment = this->read();
			if (in_segment.get_type() == SegmentType::Eof){
				this->at_eof = true;
				this->action = LZMA_FINISH;
			}else{
				auto data = in_segment.get_data();
				if (!data.size)
					continue;
				this->bytes_read += data.size;
				this->lstream.next_in = static_cast<const uint8_t *>(data.data);
				this->lstream.avail_in = data.size;
			}
		}

		if (!this->lstream.avail_out){
			if (!!out_segment)
				this->write(out_segment);
			out_segment = this->parent->allocate_segment();
			auto data = out_segment.get_data();
			this->lstream.next_out = data.data;
			this->lstream.avail_out = data.size;
		}

		auto ret_code = lzma_code(&this->lstream, this->action);

		if (ret_code != LZMA_OK){
			if (ret_code == LZMA_STREAM_END)
				break;
			const char *msg;
			switch (ret_code){
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
	if (!!out_segment){
		auto data = out_segment.get_data();
		out_segment.trim_to_size(data.size - this->lstream.avail_out);
		this->write(out_segment);
	}
}

}
