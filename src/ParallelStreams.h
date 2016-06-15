/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "System/Threads.h"
#include "SimpleTypes.h"

extern const size_t default_buffer_size;

enum class SegmentType{
	Eof,
	Data,
	Flush,
	FullFlush,
};

class StreamSegment{
	SegmentType type;
	std::shared_ptr<buffer_t> data;
public:
	size_t offset = 0;
	StreamSegment(const std::shared_ptr<buffer_t> &data): type(SegmentType::Data), data(data){}
	StreamSegment(SegmentType type = SegmentType::Eof): type(type){}
	SegmentType get_type() const{
		return this->type;
	}
	const std::shared_ptr<buffer_t> &get_data() const{
		return this->data;
	}
	static StreamSegment alloc(){
		StreamSegment ret(SegmentType::Data);
		ret.data.reset(new buffer_t(default_buffer_size));
		return ret;
	}
};

class ParallelSink;

class ParallelSource{
protected:
	std::shared_ptr<ItcQueue<StreamSegment>> output;

	void write(const StreamSegment &);
	virtual void yield() = 0;
public:
	ParallelSource(): output(std::make_shared<ItcQueue<StreamSegment>>(16)){}
	virtual ~ParallelSource(){}
	void set_output(ParallelSink &);
	const std::shared_ptr<ItcQueue<StreamSegment>> &get_output(){
		return this->output;
	}
};

class ParallelSink{
protected:
	std::shared_ptr<ItcQueue<StreamSegment>> input;

	StreamSegment take_input();
	StreamSegment read();
	virtual void yield() = 0;
public:
	ParallelSink(): input(std::make_shared<ItcQueue<StreamSegment>>(16)){}
	virtual ~ParallelSink(){}
	void set_input(ParallelSource &);
	const std::shared_ptr<ItcQueue<StreamSegment>> &get_input(){
		return this->input;
	}
};

class ParallelFilter : public FiberJob, public ParallelSource, public ParallelSink{
protected:
	bool pass_eof;

	void fiber_func() override;
	void yield() override{
		FiberJob::yield();
	}
public:
	ParallelFilter();
	virtual ~ParallelFilter(){}
	bool ready() override;
	void set_pass_eof(bool);
	virtual void process() = 0;
};

class ParallelStreamSource : public FiberJob, public ParallelSource{
protected:
	void fiber_func() override;
	void yield() override{
		FiberJob::yield();
	}
public:
	virtual ~ParallelStreamSource(){}
	bool ready() override;
	virtual void process() = 0;
};

class ParallelStreamSink : public FiberJob, public ParallelSink{
	void fiber_func() override;
	void yield() override{
		FiberJob::yield();
	}
public:
	virtual ~ParallelStreamSink(){}
	bool ready() override;
	virtual void process() = 0;
};

class ParallelSizedStreamSource : public ParallelStreamSource{
protected:
	std::uint64_t stream_size = 0;
public:
	const decltype(stream_size) &get_stream_size() const{
		return this->stream_size;
	}
};

class ParallelFileSource : public ParallelSizedStreamSource{
	boost::filesystem::ifstream stream;
public:
	ParallelFileSource(const path_t &path);
	void process() override;
};

class ParallelSha256Filter : public ParallelFilter{
	CryptoPP::SHA256 hash;
public:
	void process() override;
};

class ParallelNullSink : public ParallelStreamSink{
public:
	void process() override;
};
