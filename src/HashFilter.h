/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Filters.h"
#include "StreamProcessor.h"

namespace zstreams{

template <typename HashT>
class HashFilter{
public:
	typedef std::array<byte, HashT::DIGESTSIZE> digest_t;
private:
	HashT hash;
	std::shared_ptr<digest_t> digest;
	bool Final_called = false;

	virtual Segment read() = 0;
	virtual void write(Segment &) = 0;

protected:
	void HashFilter_work(bool is_sink){
		std::uint64_t bytes = 0;
		while (true){
			auto segment = this->read();
			if (segment.get_type() == SegmentType::Eof){
				this->Final_called = true;
				this->hash.Final(this->digest->data());
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
	HashFilter(): digest(new digest_t){}
	virtual ~HashFilter(){
		if (!this->Final_called)
			this->hash.Final(this->digest->data());
	}
	std::shared_ptr<digest_t> get_digest(){
		return this->digest;
	}
};

template <typename HashT>
class HashSource : public HashFilter<HashT>, public Source{
	void work() override{
		HashFilter<HashT>::HashFilter_work(false);
	}
	Segment read() override{
		return Source::read();
	}
	void write(Segment &s) override{
		Source::write(s);
	}
public:
	HashSource(StreamPipeline &parent): Source(parent){}
	HashSource(Source &source): Source(source){}
	const char *class_name() const override{
		return "HashSource";
	}
};

template <typename HashT>
class HashSink : public HashFilter<HashT>, public Sink{
	void work() override{
		HashFilter<HashT>::HashFilter_work(true);
	}
	Segment read() override{
		return Sink::read();
	}
	void write(Segment &s) override{
		Sink::write(s);
	}
public:
	HashSink(StreamPipeline &parent): Sink(parent){}
	HashSink(Sink &sink): Sink(sink){}
	const char *class_name() const override{
		return "HashSink";
	}
};

}
