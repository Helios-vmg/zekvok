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
class HashInputFilter : public HashFilter<HashT>, public Source{
	void work() override{
		HashFilter<HashT>::HashFilter_work();
	}
	Segment read() override{
		return Source::read();
	}
	void write(Segment &s) override{
		Source::write(s);
	}
public:
	HashInputFilter(StreamPipeline &parent): Source(parent){}
	HashInputFilter(Source &source): Source(source){}
	const char *class_name() const override{
		return "HashInputFilter";
	}
};

template <typename HashT>
class HashOutputFilter : public HashFilter<HashT>, public Sink{
	void work() override{
		HashFilter<HashT>::HashFilter_work();
	}
	Segment read() override{
		Sink::read();
	}
	void write(Segment &s) override{
		Sink::write(s);
	}
public:
	HashOutputFilter(StreamPipeline &parent): Sink(parent){}
	HashOutputFilter(Sink &sink): Sink(sink){}
	const char *class_name() const override{
		return "HashOutputFilter";
	}
};

}
