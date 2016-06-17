/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "System/Threads.h"
#include "SimpleTypes.h"

namespace zstreams{

class Pipeline;
class Processor;

enum class SegmentType{
	Undefined,
	Eof,
	Data,
	Flush,
	FullFlush,
};

struct SubSegment{
	std::uint8_t *data;
	size_t size;
};

typedef std::function<void()> flush_callback_t;
typedef std::unique_ptr<flush_callback_t> flush_callback_ptr_t;

class Segment{
	Pipeline *allocator = nullptr;
	SegmentType type = SegmentType::Undefined;
	std::unique_ptr<buffer_t> data;
	SubSegment default_subsegment = { nullptr, 0 };
	SubSegment subsegment_override = { nullptr, 0 };
	flush_callback_ptr_t flush_callback;

	//Segment(SegmentType type, const std::unique_ptr<buffer_t> &data): type(type), data(data){}
	//Segment(const std::unique_ptr<buffer_t> &data): type(SegmentType::Data), data(data){}
	SubSegment construct_default_subsegment() const{
		return SubSegment{ &(*this->data)[0], this->data->size() };
	}
	size_t get_offset() const{
		return this->subsegment_override.data - this->default_subsegment.data;
	}
	void release();
public:
	Segment(Pipeline &pipeline);
	Segment(SegmentType type = SegmentType::Undefined): type(type){}
	static Segment construct_flush(flush_callback_ptr_t &callback);
	Segment(Segment &&old){
		*this = std::move(old);
	}
	Segment(const Segment &) = delete;
	~Segment();
	void operator=(const Segment &) = delete;
	const Segment &operator=(Segment &&);
	Segment clone();
	SegmentType get_type() const{
		return this->type;
	}
	SubSegment get_data() const{
		return this->subsegment_override;
	}
	void skip_bytes(size_t);
	void trim_to_size(size_t);
};

class Queue{
	Processor *source = nullptr,
		*sink = nullptr;
	CircularQueue<Segment> queue;
	std::vector<Segment> putback;
	std::mutex putback_mutex;
public:
	Queue(): queue(16){}
	Processor *get_source() const{
		return this->source;
	}
	Processor *get_sink() const{
		return this->sink;
	}
	void push(Segment &src){
		while (!this->queue.try_push(src));
	}
	bool try_push(Segment &src){
		return this->queue.try_push(src);
	}
	bool try_pop(Segment &dst){
		{
			LOCK_MUTEX(this->putback_mutex);
			if (this->putback.size()){
				dst = std::move(this->putback.back());
				this->putback.pop_back();
				return true;
			}
		}
		return this->queue.try_pop(dst);
	}
	void put_back(Segment &s){
		LOCK_MUTEX(this->putback_mutex);
		this->putback.emplace_back(std::move(s));
	}
	void connect(Processor &source, Processor &sink){
		this->source = &source;
		this->sink = &sink;
	}
	void attach_source(Processor &source){
		this->source = &source;
	}
	void attach_sink(Processor &sink){
		this->sink = &sink;
	}
	void detach_source(){
		this->source = nullptr;
	}
	void detach_sink(){
		this->sink = nullptr;
	}
};

class Processor{
public:
	enum class State{
		Uninitialized,
		Starting,
		Running,
		Yielding,
		Stopping,
		Completed,
	};
protected:
	Pipeline *parent;
	std::atomic<State> state;
	std::unique_ptr<std::thread> thread;
	std::shared_ptr<Queue> sink_queue, source_queue;

	void thread_func();
	virtual void work() = 0;
	Segment read();
	void write(Segment &);
	void check_termination();
	virtual void flush_impl(){}
	virtual bool pass_flush(){
		return true;
	}
public:
	Processor(Pipeline &parent);
	virtual ~Processor();
	void start();
	void join();
	void stop();
	State get_state() const{
		return this->state;
	}
	void connect_to_source(Processor &);
	void connect_to_sink(Processor &p);
	Pipeline &get_pipeline() const{
		return *this->parent;
	}
};

class Pipeline{
	friend class Processor;
	std::atomic<int> busy_threads;
	std::unordered_set<uintptr_t> processors;
	std::vector<std::unique_ptr<buffer_t>> allocated_buffers;
	std::mutex allocated_buffers_mutex;

	void notify_thread_creation(Processor *);
	void notify_thread_end(Processor *);
public:
	Pipeline();
	~Pipeline();
	void start();
	//void sync();
	std::unique_ptr<buffer_t> allocate_buffer();
	void release_buffer(std::unique_ptr<buffer_t> &);
	Segment allocate_segment();
};

class SizedSource{
protected:
	std::uint64_t stream_size = 0;
public:
	virtual ~SizedSource() = 0;
	const std::uint64_t &get_stream_size() const{
		return this->stream_size;
	}
};

class OutputStream : public Processor{
public:
	OutputStream(Pipeline &parent): Processor(parent){}
	OutputStream(OutputStream &sink);
	virtual ~OutputStream() = 0;
	void flush();
};

#define IGNORE_FLUSH_COMMAND bool pass_flush() override{ return false; }

class InputStream : public Processor{
	IGNORE_FLUSH_COMMAND
public:
	InputStream(Pipeline &parent): Processor(parent){}
	InputStream(InputStream &source);
	void copy_to(OutputStream &);
};

class FileSource : public InputStream, public SizedSource{
	boost::filesystem::ifstream stream;

	void work() override;
public:
	FileSource(const path_t &path, Pipeline &parent);
};

template <typename HashT>
class HashFilter{
	HashT hash;
	std::array<byte, HashT::DIGESTSIZE> digest;

	virtual Segment read() = 0;
	virtual void write(Segment &) = 0;

protected:
	void HashFilter_work(){
		std::uint64_t bytes = 0;
		while (true){
			auto segment = this->read();
			if (segment.get_type() == SegmentType::Eof){
				this->hash.Final(this->digest.data());
				this->write(segment);
				break;
			}
			auto data = segment.get_data();
			bytes += data.size;
			this->hash.Update(data.data, data.size);
			this->write(segment);
		}
	}
public:
	virtual ~HashFilter(){}
	const std::array<byte, HashT::DIGESTSIZE> &get_digest(){
		return this->digest;
	}
};

template <typename HashT>
class HashInputFilter : public HashFilter<HashT>, public InputStream{
	void work() override{
		HashFilter<HashT>::HashFilter_work();
	}
	Segment read() override{
		return InputStream::read();
	}
	void write(Segment &s) override{
		InputStream::write(s);
	}
public:
	HashInputFilter(Pipeline &parent): InputStream(parent){}
	HashInputFilter(InputStream &source): InputStream(source){}
};

template <typename HashT>
class HashOutputFilter : public HashFilter<HashT>, public OutputStream{
	void work() override{
		HashFilter<HashT>::HashFilter_work();
	}
	Segment read() override{
		OutputStream::read();
	}
	void write(Segment &s) override{
		OutputStream::write(s);
	}
public:
	HashOutputFilter(Pipeline &parent): OutputStream(parent){}
	HashOutputFilter(OutputStream &sink): OutputStream(sink){}
};

class NullSink : public OutputStream{
	void work() override{
		while (this->read().get_type() != SegmentType::Eof)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	IGNORE_FLUSH_COMMAND
public:
	NullSink(Pipeline &parent): OutputStream(parent){}
};

}
