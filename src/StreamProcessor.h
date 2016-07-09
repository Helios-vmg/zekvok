/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "System/Threads.h"
#include "SimpleTypes.h"

namespace zstreams{

typedef std::uint64_t streamsize_t;

class StreamPipeline;
class StreamProcessor;

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
	StreamPipeline *allocator = nullptr;
	SegmentType type = SegmentType::Undefined;
	std::unique_ptr<buffer_t> data;
	SubSegment default_subsegment = { nullptr, 0 };
	SubSegment subsegment_override = { nullptr, 0 };
	flush_callback_ptr_t flush_callback;

	SubSegment construct_default_subsegment() const{
		return SubSegment{ &(*this->data)[0], this->data->size() };
	}
	size_t get_offset() const{
		return this->subsegment_override.data - this->default_subsegment.data;
	}
	void release();
public:
	Segment(StreamPipeline &pipeline);
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
	Segment clone_and_trim(size_t size = std::numeric_limits<size_t>::max());
	SegmentType get_type() const{
		return this->type;
	}
	SubSegment get_data() const{
		return this->subsegment_override;
	}
	void skip_bytes(size_t);
	void trim_to_size(size_t);
	flush_callback_t &get_flush_callback(){
		return *this->flush_callback;
	}
	bool operator!() const{
		return this->type == SegmentType::Undefined;
	}
};

class Queue{
	StreamProcessor *source = nullptr,
		*sink = nullptr;
	CircularQueue<Segment> queue;
	std::vector<Segment> putback;
public:
	Queue(): queue(16){}
	StreamProcessor *get_source() const{
		return this->source;
	}
	StreamProcessor *get_sink() const{
		return this->sink;
	}
	void push(Segment &src){
		while (!this->queue.try_push(src));
	}
	bool try_push(Segment &src){
		return this->queue.try_push(src);
	}
	bool try_pop(Segment &dst){
		if (this->putback.size()){
			dst = std::move(this->putback.back());
			this->putback.pop_back();
			return true;
		}
		return this->queue.try_pop(dst);
	}
	void put_back(Segment &s){
		this->putback.emplace_back(std::move(s));
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

#define USE_THREADPOOL

class StreamProcessor{
public:
	enum class State{
		Uninitialized,
		Starting,
		Running,
		Yielding,
		Completed,
		Ignore,
	};
private:
	std::uint64_t bytes_read = 0,
		bytes_written = 0;
protected:
	StreamPipeline *pipeline;
	std::atomic<State> state;
	std::atomic<bool> stop_requested;
#ifdef USE_THREADPOOL
	ThreadWrapper thread;
#else
	std::unique_ptr<std::thread> thread;
#endif
	std::shared_ptr<Queue> sink_queue,
		source_queue;
	bool pass_eof = true;
	std:: uint64_t *bytes_read_dst = nullptr,
		*bytes_written_dst = nullptr;

	void thread_func();
	virtual void work() = 0;
	Segment read();
	void throw_on_termination();
	virtual void flush_impl(){}
	virtual bool pass_flush(){
		return true;
	}
	void report_bytes_read(size_t n){
		this->bytes_read += n;
	}
	void report_bytes_written(size_t n){
		this->bytes_written += n;
	}
	void put_back(Segment &segment);
public:
	StreamProcessor(StreamPipeline &parent);
	virtual ~StreamProcessor();
	virtual void start();
	virtual void join();
	virtual void stop();
	State get_state() const{
		return this->state;
	}
	void connect_to_source(StreamProcessor &);
	void connect_to_sink(StreamProcessor &p);
	StreamPipeline &get_pipeline() const{
		return *this->pipeline;
	}
	virtual const char *class_name() const = 0;
	void set_bytes_written_dst(std::uint64_t &dst){
		this->bytes_written_dst = &dst;
	}
	void set_bytes_read_dst(std::uint64_t &dst){
		this->bytes_read_dst = &dst;
	}
	Segment allocate_segment() const;
	void write(Segment &);
	void notify_thread_creation();
	void notify_thread_end();
};

class StreamPipeline{
	friend class StreamProcessor;
	std::atomic<int> busy_threads;
	std::unordered_map<uintptr_t, std::string> processors;
	std::mutex processors_mutex;
	std::vector<std::unique_ptr<buffer_t>> allocated_buffers;
	std::mutex allocated_buffers_mutex;

	void notify_thread_creation(StreamProcessor *);
	void notify_thread_end(StreamProcessor *);
	boost::optional<std::string> exception_message;
	std::mutex exception_message_mutex;
	void set_exception_message(const std::string &);
public:
	StreamPipeline();
	~StreamPipeline();
	void start();
	//void sync();
	std::unique_ptr<buffer_t> allocate_buffer();
	void release_buffer(std::unique_ptr<buffer_t> &);
	Segment allocate_segment();
	void check_exceptions();
};

class SizedSource{
protected:
	streamsize_t stream_size = 0;
public:
	virtual ~SizedSource() = 0;
	const streamsize_t &get_stream_size() const{
		return this->stream_size;
	}
};

class Sink : public StreamProcessor{
public:
	Sink(StreamPipeline &parent): StreamProcessor(parent){}
	Sink(Sink &sink);
	virtual ~Sink() = 0;
	void flush();
};

// Warning: only use for input streams and FINAL output streams, NOT for output filters!
#define IGNORE_FLUSH_COMMAND bool pass_flush() override{ return false; }

class Source : public StreamProcessor{
	IGNORE_FLUSH_COMMAND
public:
	Source(StreamPipeline &parent): StreamProcessor(parent){}
	Source(Source &source);
	void copy_to(Sink &);
	void discard_rest();
};

class StdStreamSource : public Source{
	std::unique_ptr<std::istream> stream;

	void work() override;
	IGNORE_FLUSH_COMMAND
protected:
	void set_stream(std::unique_ptr<std::istream> &);
public:
	StdStreamSource(std::unique_ptr<std::istream> &stream, StreamPipeline &parent): Source(parent){
		this->set_stream(stream);
	}
	StdStreamSource(StreamPipeline &parent): Source(parent){}
	virtual ~StdStreamSource(){}
	virtual const char *class_name() const override{
		return "StdStreamSource";
	}
};

class FileSource : public StdStreamSource, public SizedSource{
public:
	FileSource(const path_t &path, StreamPipeline &parent);
	const char *class_name() const override{
		return "FileSource";
	}
};

class StdStreamSink : public Sink{
	std::unique_ptr<std::ostream> stream;

	void work() override;
	void flush_impl() override;
	IGNORE_FLUSH_COMMAND
protected:
	void set_stream(std::unique_ptr<std::ostream> &);
public:
	StdStreamSink(std::unique_ptr<std::ostream> &stream, StreamPipeline &parent): Sink(parent){
		this->set_stream(stream);
	}
	StdStreamSink(StreamPipeline &parent): Sink(parent){}
	virtual ~StdStreamSink(){}
	virtual const char *class_name() const override{
		return "StdStreamSink";
	}
};

class FileSink : public StdStreamSink{
public:
	FileSink(const path_t &path, StreamPipeline &parent);
	const char *class_name() const override{
		return "FileSink";
	}
};

class SynchronousSourceImpl : public Source{
	Segment current_segment;
	bool at_eof = false;
	void work() override{}
public:
	SynchronousSourceImpl(Source &);
	virtual ~SynchronousSourceImpl();
	void start() override{}
	void join() override{}
	void stop() override{}
	std::streamsize read(char *s, std::streamsize n);
	virtual const char *class_name() const override{
		return "SynchronousSourceImpl";
	}
};

class SynchronousSource : public boost::iostreams::source{
	std::shared_ptr<SynchronousSourceImpl> impl;
public:
	SynchronousSource(Source &source): impl(new SynchronousSourceImpl(source)){}
	std::streamsize read(char *s, std::streamsize n){
		return this->impl->read(s, n);
	}
};

class SynchronousSinkImpl : public Sink{
	Segment current_segment;
	size_t offset;
	IGNORE_FLUSH_COMMAND
	void ensure_valid_segment();
	void try_write(bool force = false);
	void work() override{}
public:
	SynchronousSinkImpl(Sink &);
	virtual ~SynchronousSinkImpl();
	void start() override{}
	void join() override{}
	void stop() override{}
	std::streamsize write(const char *s, std::streamsize n);
	virtual const char *class_name() const override{
		return "SynchronousSink";
	}
};

class SynchronousSink : public boost::iostreams::sink{
	std::shared_ptr<SynchronousSinkImpl> impl;
public:
	SynchronousSink(Sink &sink): impl(new SynchronousSinkImpl(sink)){}
	std::streamsize write(const char *s, std::streamsize n){
		return this->impl->write(s, n);
	}
};

template <typename T>
class Stream{
public:
	std::unique_ptr<T> stream;
	template <typename T1>
	typename std::enable_if<std::is_base_of<Sink, T1>::value, void>::type flush_stream(){
		if (this->stream)
			this->stream->flush();
	}
	template <typename T1>
	typename std::enable_if<!std::is_base_of<Sink, T1>::value, void>::type flush_stream(){
		if (this->stream)
			this->stream->stop();
	}
	Stream(){}
	template <typename ... Args>
	Stream(Args && ... args): stream(std::make_unique<T, Args...>(std::forward<Args>(args)...)){
		this->stream->notify_thread_creation();
	}
	template <typename T2>
	Stream(Stream<T2> &&old){
		this->stream = std::move(old.stream);
	}
	template <typename T2>
	const Stream &operator=(Stream<T2> &&old){
		this->stream = std::move(old.stream);
		return *this;
	}
	~Stream(){
		this->flush_stream<T>();
		if (this->stream)
			this->stream->notify_thread_end();
	}
	T &operator*(){
		return *this->stream;
	}
	T *operator->(){
		return this->stream.get();
	}
	bool operator!() const{
		return !this->stream;
	}
};

}
