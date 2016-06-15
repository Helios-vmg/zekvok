/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "ParallelStreams.h"

const size_t default_buffer_size = 1 << 16;

ParallelFilter::ParallelFilter(){
}

bool ParallelFilter::ready(){
	return !this->input.empty() && !this->output->full();
}

bool ParallelStreamSource::ready(){
	return !this->output->full();
}

bool ParallelStreamSink::ready(){
	return !this->input.empty();
}

void ParallelSource::write(const StreamSegment &segment){
	while (this->output->full())
		this->yield();
	this->output->push(segment);
}

StreamSegment ParallelSink::read(){
	auto ret = this->take_input();
	if (ret.get_type() == SegmentType::Eof)
		this->input.put_back(ret);
	return ret;
}

StreamSegment ParallelSink::take_input(){
	StreamSegment ret;
	while (!this->input.pop_single_try(ret))
		this->yield();
	return ret;
}

void ParallelFilter::fiber_func(){
	this->process();
	this->output->push(StreamSegment(SegmentType::Eof));
	this->done = true;
}

void ParallelFilter::set_pass_eof(bool pass){
	this->pass_eof = pass;
}

void ParallelSource::set_output(ParallelSink &sink){
	this->output = sink.get_input();
}

void ParallelSink::set_input(ParallelSource &source){
	source.set_output(*this);
}

void ParallelStreamSource::fiber_func(){
	this->process();
	this->output->push(StreamSegment(SegmentType::Eof));
	this->done = true;
}

void ParallelStreamSink::fiber_func(){
	this->process();
	this->done = true;
}

ParallelFileSource::ParallelFileSource(const path_t &path): stream(path, std::ios::binary){
	if (!this->stream)
		throw std::exception("File not found!");
	this->stream.seekg(0, std::ios::end);
	this->stream_size = this->stream.tellg();
	this->stream.seekg(0);
}

void ParallelFileSource::process(){
	while (true){
		auto segment = StreamSegment::alloc();
		auto data = segment.get_data();
		this->stream.read((char *)&(*data)[0], data->size());
		if (!this->stream)
			break;
		auto read = this->stream.gcount();
		data->resize(read);
		this->write(segment);
	}
}

void ParallelSha256Filter::process(){
	while (true){
		auto segment = this->read();
		if (segment.get_type() == SegmentType::Eof)
			break;
		this->hash.Update(&(*segment.get_data())[0], segment.get_data()->size());
		this->write(segment);
	}
}

void ParallelNullSink::process(){
	while (this->read().get_type() != SegmentType::Eof);
}
