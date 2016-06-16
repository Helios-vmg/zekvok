/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "StreamProcessor.h"
#include "Utility.h"

struct StreamProcessorStoppingException{};

StreamProcessor::StreamProcessor(StreamPipeline &parent): parent(&parent), state(State::Uninitialized){}

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
		if (this->sink_queue->try_pop(ret))
			return ret;
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
		if (this->sink_queue->try_push(segment))
			return;
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
	StreamSegment s(SegmentType::Eof);
	this->write(s);
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
