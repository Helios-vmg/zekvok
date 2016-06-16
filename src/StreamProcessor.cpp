/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "StreamProcessor.h"
#include "Utility.h"

struct StreamProcessorStoppingException{};

StreamProcessor::StreamProcessor(StreamPipeline &parent): parent(&parent), state(State::Uninitialized){
	this->parent->notify_thread_creation(this);
}

StreamProcessor::~StreamProcessor(){
	this->join();
	if (this->sink_queue)
		this->sink_queue->detach_sink();
	if (this->source_queue)
		this->source_queue->detach_source();
}

StreamSegment StreamProcessor::read(){
	auto &running = this->parent->busy_threads;
	ScopedAtomicReversibleSet<State> scope(this->state, State::Yielding);
	ScopedDecrement<decltype(running)> inc(running);
	while (true){
		StreamSegment ret;
		auto result = this->source_queue->try_pop(ret);
		switch (result){
			case StreamCommunicationQueue::Result::Ok:
				return ret;
			case StreamCommunicationQueue::Result::Timeout:
			case StreamCommunicationQueue::Result::Disconnected:
				break;
			default:
				zekvok_assert(false);
		}
		if (result == StreamCommunicationQueue::Result::Ok)
			return ret;
		if (result == StreamCommunicationQueue::Result::Disconnected)
		if (this->state == State::Stopping){
			scope.cancel();
			throw StreamProcessorStoppingException();
		}
	}
}

void StreamProcessor::write(StreamSegment &segment){
	auto &running = this->parent->busy_threads;
	ScopedAtomicReversibleSet<State> scope(this->state, State::Yielding);
	ScopedDecrement<decltype(running)> inc(running);
	while (true){
		auto result = this->sink_queue->try_push(segment);
		switch (result){
			case StreamCommunicationQueue::Result::Ok:
				return;
			case StreamCommunicationQueue::Result::Timeout:
			case StreamCommunicationQueue::Result::Disconnected:
				break;
			default:
				zekvok_assert(false);
		}
		if (this->state == State::Stopping){
			scope.cancel();
			throw StreamProcessorStoppingException();
		}
	}
}

void StreamProcessor::start(){
	this->state = State::Starting;
	this->thread.reset(new std::thread([this](){ this->thread_func(); }));
}

void StreamProcessor::thread_func(){
	this->state = State::Running;
	ScopedAtomicPostSet<State> scope(this->state, State::Completed);
	auto &running = this->parent->busy_threads;
	ScopedIncrement<decltype(running)> inc(running);
	try{
		this->work();
	}catch (StreamProcessorStoppingException &){
	}
	if (this->sink_queue){
		StreamSegment s(SegmentType::Eof);
		this->write(s);
	}
	this->parent->notify_thread_end(this);
}

void StreamProcessor::join(){
	if (this->thread){
		this->thread->join();
		this->thread.reset();
	}
	this->state = State::Completed;
}

void StreamProcessor::stop(){
	while (true){
		switch (this->state){
			case State::Uninitialized:
			case State::Starting:
				continue;
			case State::Running:
			case State::Yielding:
				this->state = State::Stopping;
			case State::Stopping:
			case State::Completed:
				this->join();
				return;
			default:
				zekvok_assert(false);
		}
	}
}

#define CONNECT_SOURCE_SINK(x, y)								\
	zekvok_assert(!this->x##_queue);							\
	if (p.y##_queue){											\
		p.y##_queue->attach_sink(*this);						\
		this->x##_queue = p.y##_queue;							\
	}else{														\
		this->x##_queue.reset(new StreamCommunicationQueue);	\
		this->x##_queue->connect(p, *this);						\
		this->x##_queue->attach_##x(*this);						\
		this->x##_queue->attach_##y(p);							\
		p.y##_queue = this->x##_queue;							\
	}

void StreamProcessor::connect_to_source(StreamProcessor &p){
	CONNECT_SOURCE_SINK(source, sink);
}

void StreamProcessor::connect_to_sink(StreamProcessor &p){
	CONNECT_SOURCE_SINK(sink, source);
}

void StreamPipeline::notify_thread_creation(StreamProcessor *p){
	this->processors.insert((uintptr_t)p);
}

void StreamPipeline::notify_thread_end(StreamProcessor *p){
	this->processors.erase((uintptr_t)p);
}

StreamPipeline::StreamPipeline(){
	this->busy_threads = 0;
}

StreamPipeline::~StreamPipeline(){
}

void StreamPipeline::start(){
	for (auto &p : this->processors)
		((StreamProcessor *)p)->start();
}

void StreamPipeline::sync(){
	//TODO: improve
	while (this->busy_threads)
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

ParallelFileSource::ParallelFileSource(const path_t &path, StreamPipeline &parent):
		ParallelSizedStreamSource(parent),
		stream(path, std::ios::binary){
	if (!this->stream)
		throw std::exception("File not found!");
	this->stream.seekg(0, std::ios::end);
	this->stream_size = this->stream.tellg();
	this->stream.seekg(0);
}

void ParallelFileSource::work(){
	while (this->stream){
		auto segment = StreamSegment::alloc();
		auto data = segment.get_data();
		this->stream.read(reinterpret_cast<char *>(&(*data)[0]), data->size());
		auto read = this->stream.gcount();
		if (!read)
			break;
		data->resize(read);
		this->write(segment);
	}
}

void ParallelSha256Filter::work(){
	while (true){
		auto segment = this->read();
		if (segment.get_type() == SegmentType::Eof)
			break;
		this->hash.Update(&(*segment.get_data())[0], segment.get_data()->size());
		this->write(segment);
	}
}

void ParallelSha256Filter::write_digest(byte *digest){
	this->hash.Final(digest);
}

void ParallelNullSink::work(){
	while (this->read().get_type() != SegmentType::Eof);
}
