/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "StreamProcessor.h"
#include "Utility.h"

namespace zstreams{

const size_t default_buffer_size = 1 << 16;

const Segment &Segment::operator=(Segment &&old){
	this->release();
	this->allocator = old.allocator;
	old.allocator = nullptr;
	this->type = old.type;
	old.type = SegmentType::Undefined;
	this->data = std::move(old.data);
	this->default_subsegment = old.default_subsegment;
	old.default_subsegment = SubSegment();
	this->subsegment_override = old.subsegment_override;
	old.subsegment_override = SubSegment();
	this->flush_callback = std::move(old.flush_callback);
	return *this;
}

void Segment::skip_bytes(size_t n){
	auto addend = std::min(this->subsegment_override.size, n);
	this->subsegment_override.data += addend;
	this->subsegment_override.size -= addend;
}

void Segment::trim_to_size(size_t n){
	if (n < this->subsegment_override.size)
		this->subsegment_override.size = n;
}

Segment Segment::clone_and_trim(size_t max_size){
	if (this->type == SegmentType::Flush || this->type == SegmentType::FullFlush)
		throw std::exception("Attempt to clone a flush signal.");
	Segment ret;
	auto &old = *this;
	ret.allocator = old.allocator;
	ret.type = old.type;
	if (ret.allocator){
		max_size = std::min(max_size, old.subsegment_override.size);
		ret.data = ret.allocator->allocate_buffer();
		ret.default_subsegment = ret.construct_default_subsegment();
		ret.subsegment_override.data = ret.default_subsegment.data;
		ret.subsegment_override.size = max_size;
		memcpy(ret.default_subsegment.data, old.default_subsegment.data + old.get_offset(), max_size);
	}
	return ret;
}

Segment Segment::clone(){
	if (this->type == SegmentType::Flush || this->type == SegmentType::FullFlush)
		throw std::exception("Attempt to clone a flush signal.");
	Segment ret;
	auto &old = *this;
	ret.allocator = old.allocator;
	ret.type = old.type;
	if (ret.allocator){
		ret.data = ret.allocator->allocate_buffer();
		ret.default_subsegment = ret.construct_default_subsegment();
		memcpy(ret.default_subsegment.data, old.default_subsegment.data, ret.default_subsegment.size);
		ret.subsegment_override.data = ret.default_subsegment.data + old.get_offset();
		ret.subsegment_override.size = old.subsegment_override.size;
	}
	return ret;
}

void Segment::release(){
	if (this->data)
		this->allocator->release_buffer(this->data);
}

Segment::Segment(Pipeline &pipeline){
	this->allocator = &pipeline;
	this->data = pipeline.allocate_buffer();
	this->type = SegmentType::Data;
	this->subsegment_override = this->default_subsegment = this->construct_default_subsegment();
}

Segment Segment::construct_flush(flush_callback_ptr_t &callback){
	Segment ret(SegmentType::FullFlush);
	ret.flush_callback = std::move(callback);
	return ret;
}

Segment::~Segment(){
	this->release();
}

struct StreamProcessorStoppingException{};

Processor::Processor(Pipeline &parent): parent(&parent), state(State::Uninitialized){
	this->parent->notify_thread_creation(this);
}

Processor::~Processor(){
	this->stop();
	this->join();
	if (this->sink_queue)
		this->sink_queue->detach_source();
	if (this->source_queue)
		this->source_queue->detach_sink();
}

Segment Processor::read(){
	while (true){
		Segment ret;
		{
			auto &running = this->parent->busy_threads;
			ScopedAtomicReversibleSet<State> scope(this->state, State::Yielding, true);
			ScopedDecrement<decltype(running)> inc(running);
			while (!this->source_queue->try_pop(ret))
				this->check_termination();
		}
		if (ret.get_type() == SegmentType::Flush){
			this->flush_impl();
			ret.get_flush_callback()();
			continue;
		}
		if (ret.get_type() == SegmentType::FullFlush){
			this->flush_impl();
			if (this->pass_flush())
				this->sink_queue->push(ret);
			else
				ret.get_flush_callback()();
			continue;
		}
		return ret;
	}
}

void Processor::check_termination(){
	if (this->state == State::Stopping)
		throw StreamProcessorStoppingException();
}

void Processor::write(Segment &segment){
	if (!this->pass_eof && segment.get_type() == SegmentType::Eof)
		return;
	auto &running = this->parent->busy_threads;
	ScopedAtomicReversibleSet<State> scope(this->state, State::Yielding, true);
	ScopedDecrement<decltype(running)> inc(running);
	while (!this->sink_queue->try_push(segment))
		this->check_termination();
}

void Processor::start(){
	auto expected = State::Uninitialized;
	if (!this->state.compare_exchange_strong(expected, State::Starting))
		return;
	this->state = State::Starting;
	this->thread.reset(new std::thread([this](){ this->thread_func(); }));
}

void Processor::thread_func(){
	this->state = State::Running;
	ScopedAtomicPostSet<State> scope(this->state, State::Completed);
	auto &running = this->parent->busy_threads;
	ScopedIncrement<decltype(running)> inc(running);
	try{
		this->work();
	}catch (StreamProcessorStoppingException &){
	}catch (std::exception &e){
		this->parent->set_exception_message((std::string)this->class_name() + ": " + e.what());
	}
	this->parent->notify_thread_end(this);
}

void Processor::join(){
	if (this->thread){
		this->thread->join();
		this->thread.reset();
	}
	this->state = State::Completed;
}

void Processor::stop(){
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

#define USE_MACRO

#define CONNECT_SOURCE_SINK(x, y)		\
	zekvok_assert(!y.x##_queue);		\
	if (x.y##_queue){					\
		x.y##_queue->attach_##y(y);		\
		y.x##_queue = x.y##_queue;		\
	}else{								\
		y.x##_queue.reset(new Queue);	\
		y.x##_queue->attach_##x(x);		\
		y.x##_queue->attach_##y(y);		\
		x.y##_queue = y.x##_queue;		\
	}

void Processor::connect_to_source(Processor &p){
	Processor &source = p;
	Processor &sink = *this;
	CONNECT_SOURCE_SINK(source, sink);
}

void Processor::connect_to_sink(Processor &p){
	Processor &sink = p;
	Processor &source = *this;
	CONNECT_SOURCE_SINK(sink, source);
}

void Pipeline::notify_thread_creation(Processor *p){
	this->processors.insert((uintptr_t)p);
}

void Pipeline::notify_thread_end(Processor *p){
	this->processors.erase((uintptr_t)p);
}

Pipeline::Pipeline(){
	this->busy_threads = 0;
}

Pipeline::~Pipeline(){
}

void Pipeline::start(){
	this->check_exceptions();
	for (auto &p : this->processors)
		((Processor *)p)->start();
}

void Pipeline::check_exceptions(){
	decltype(this->exception_message) ex;
	{
		LOCK_MUTEX(this->exception_message_mutex);
		ex = std::move(this->exception_message);
	}
	if (ex.is_initialized()){
		auto message = ex.value();
		throw StdStringException(message);
	}
}

std::unique_ptr<buffer_t> Pipeline::allocate_buffer(){
	{
		LOCK_MUTEX(this->allocated_buffers_mutex);
		if (this->allocated_buffers.size()){
			auto ret = std::move(this->allocated_buffers.back());
			this->allocated_buffers.pop_back();
			return ret;
		}
	}
	return std::make_unique<buffer_t>(default_buffer_size);
}

void Pipeline::release_buffer(std::unique_ptr<buffer_t> &buffer){
	LOCK_MUTEX(this->allocated_buffers_mutex);
	this->allocated_buffers.emplace_back(std::move(buffer));
}

Segment Pipeline::allocate_segment(){
	return Segment(*this);
}

void Pipeline::set_exception_message(const std::string &s){
	LOCK_MUTEX(this->exception_message_mutex);
	this->exception_message = s;
}

//void Pipeline::sync(){
//	//TODO: improve
//	while (this->busy_threads)
//		std::this_thread::sleep_for(std::chrono::milliseconds(250));
//}

FileSource::FileSource(const path_t &path, Pipeline &parent):
		InputStream(parent),
		stream(path, std::ios::binary){
	if (!this->stream)
		throw std::exception("File not found!");
	this->stream.seekg(0, std::ios::end);
	this->stream_size = this->stream.tellg();
	this->stream.seekg(0);
}

void FileSource::work(){
	std::uint64_t bytes = 0;
	while (this->stream){
		auto segment = this->parent->allocate_segment();
		auto data = segment.get_data();
		this->stream.read(reinterpret_cast<char *>(data.data), data.size);
		auto read = this->stream.gcount();
		if (!read)
			break;
		segment.trim_to_size(read);
		bytes += read;
		this->write(segment);
	}
	Segment s(SegmentType::Eof);
	this->write(s);
}

OutputStream::~OutputStream(){
}

OutputStream::OutputStream(OutputStream &sink): Processor(sink.get_pipeline()){
	this->connect_to_sink(sink);
}

void OutputStream::flush(){
	if (!this->source_queue->get_source()){
		Segment eof(SegmentType::Eof);
		this->source_queue->push(eof);
		this->join();
		return;
	}
	std::mutex flush_mutex;
	std::condition_variable cv;
	bool ready = false;
	auto cb = std::make_unique<flush_callback_t>([&]{ ready = true; cv.notify_all(); });
	auto segment = Segment::construct_flush(cb);
	this->source_queue->push(segment);
	std::unique_lock<std::mutex> lock(flush_mutex);
	while (!ready && this->state != State::Completed)
		cv.wait_for(lock, std::chrono::milliseconds(250));
	if (!ready)
		this->flush_impl();
}

InputStream::InputStream(InputStream &source) : Processor(source.get_pipeline()){
	this->connect_to_source(source);
}

void InputStream::copy_to(OutputStream &sink){
	this->pass_eof = false;
	this->connect_to_sink(sink);
	this->parent->start();
	this->join();
}

SizedSource::~SizedSource(){}

FileSink::FileSink(const path_t &path, Pipeline &parent):
		OutputStream(parent),
		stream(path, std::ios::binary){
	if (!this->stream)
		throw std::exception("Error opening file!");
}

void FileSink::work(){
	while (true){
		auto segment = this->read();
		if (segment.get_type() == SegmentType::Eof)
			break;
		auto data = segment.get_data();
		this->stream.write(reinterpret_cast<const char *>(data.data), data.size);
	}
}

}
