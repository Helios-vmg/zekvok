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
	return !this->input->empty() && !this->output->full();
}

bool ParallelStreamSource::ready(){
	return !this->output->full();
}

bool ParallelStreamSink::ready(){
	return !this->input->empty();
}

void ParallelSource::write(const StreamSegment &segment){
	while (this->output->full())
		this->yield();
	this->output->push(segment);
}

StreamSegment ParallelSink::read(){
	auto ret = this->take_input();
	if (ret.get_type() == SegmentType::Eof)
		this->input->put_back(ret);
	return ret;
}

StreamSegment ParallelSink::take_input(){
	StreamSegment ret;
	while (!this->input->pop_single_try(ret))
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
	sink.get_input()->prepend(*this->output);
	this->output = sink.get_input();
}

void ParallelSink::set_input(ParallelSource &source){
	source.get_output()->append(*this->input);
	this->input = source.get_output();
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
