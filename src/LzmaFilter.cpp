/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "LzmaFilter.h"

namespace zstreams{

LzmaOutputStream::LzmaOutputStream(OutputStream &stream, bool *multithreaded, int compression_level, bool extreme_mode):
	OutputStream(stream){
	zero_struct(this->lstream);
	this->lstream = LZMA_STREAM_INIT;

	auto f = !*multithreaded ? &LzmaOutputStream::initialize_single_threaded : &LzmaOutputStream::initialize_multithreaded;
	*multithreaded = (this->*f)(compression_level, extreme_mode);

	this->action = LZMA_RUN;
	this->reset_segment();
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

bool LzmaOutputStream::initialize_single_threaded(int compression_level, bool extreme_mode){
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

bool LzmaOutputStream::initialize_multithreaded(int compression_level, bool extreme_mode){
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
		this->initialize_single_threaded(compression_level, extreme_mode);
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
			this->lstream.next_in = static_cast<const uint8_t *>(data.data);
			this->lstream.avail_in = data.size;
		}
		lret = lzma_code(&this->lstream, this->action);

	}while (this->pass_data_to_stream(lret));
	this->flush_impl();
}

LzmaInputStream::LzmaInputStream(InputStream &stream):
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
