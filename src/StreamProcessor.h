/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "ParallelStreams.h"

class StreamProcessor;
class StreamPipeline;

class StreamCommunicationQueue{
	StreamProcessor *source = nullptr,
		*sink = nullptr;
	CircularQueue<StreamSegment> queue;
public:
	StreamCommunicationQueue(): queue(16){}
	StreamProcessor *get_source() const{
		return this->source;
	}
	StreamProcessor *get_sink() const{
		return this->sink;
	}
	enum class Result{
		Ok,
		Timeout,
		Disconnected,
	};
	Result try_push(StreamSegment &src){
		//if (!this->sink)
		//	return Result::Disconnected;
		return this->queue.try_push(src) ? Result::Ok : Result::Timeout;
	}
	Result try_pop(StreamSegment &dst){
		//if (!this->source)
		//	return Result::Disconnected;
		return this->queue.try_pop(dst) ? Result::Ok : Result::Timeout;
	}
	void connect(StreamProcessor &source, StreamProcessor &sink){
		this->source = &source;
		this->sink = &sink;
	}
	void attach_source(StreamProcessor &source){
		this->source = &source;
	}
	void attach_sink(StreamProcessor &sink){
		this->sink = &sink;
	}
	void detach_source(){
		this->source = nullptr;
	}
	void detach_sink(){
		this->sink = nullptr;
	}
};

class StreamProcessor{
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
	StreamPipeline *parent;
	std::atomic<State> state;
	std::unique_ptr<std::thread> thread;
	std::shared_ptr<StreamCommunicationQueue> sink_queue, source_queue;

	void thread_func();
	//virtual bool ready() = 0;
	virtual void work() = 0;
	StreamSegment read();
	void write(StreamSegment &);
public:
	StreamProcessor(StreamPipeline &parent);
	virtual ~StreamProcessor();
	void start();
	void join();
	void stop();
	State get_state() const{
		return this->state;
	}
	void connect_to_source(StreamProcessor &);
	void connect_to_sink(StreamProcessor &p);
};

class StreamPipeline{
	friend class StreamProcessor;
	std::atomic<int> busy_threads;
	std::unordered_set<uintptr_t> processors;
	
	void notify_thread_creation(StreamProcessor *);
	void notify_thread_end(StreamProcessor *);
public:
	StreamPipeline();
	~StreamPipeline();
	void start();
	//void sync();
};

class ParallelSizedStreamSource : public StreamProcessor{
protected:
	std::uint64_t stream_size = 0;
public:
	ParallelSizedStreamSource(StreamPipeline &parent): StreamProcessor(parent){}
	const decltype(stream_size) &get_stream_size() const{
		return this->stream_size;
	}
};

class ParallelFileSource : public ParallelSizedStreamSource{
	boost::filesystem::ifstream stream;

	void work() override;
public:
	ParallelFileSource(const path_t &path, StreamPipeline &parent);
};

class ParallelSha256Filter : public StreamProcessor{
	CryptoPP::SHA256 hash;

	void work() override;
public:
	ParallelSha256Filter(StreamPipeline &parent): StreamProcessor(parent){}
	void write_digest(byte *);
};

class ParallelNullSink : public StreamProcessor{
	void work() override;
public:
	ParallelNullSink(StreamPipeline &parent): StreamProcessor(parent){}
};
