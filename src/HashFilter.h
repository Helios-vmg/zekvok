/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Filters.h"
#include "StreamProcessor.h"

class HashCalculator{
	std::shared_ptr<CryptoPP::HashTransformation> hash;
	std::uint64_t bytes_processed;
	void *dst;
	size_t dst_length;
	void write_result();
protected:
	void update(const void *input, size_t length){
		if (length){
			this->hash->Update(static_cast<const byte *>(input), length);
			this->bytes_processed += length;
		}
	}
public:
	HashCalculator(CryptoPP::HashTransformation *t)
			: bytes_processed(0)
			, dst(nullptr)
			, dst_length(0){
		this->hash.reset(t);
	}
	virtual ~HashCalculator();
	size_t get_hash_length() const;
	void get_result(void *buffer, size_t max_length);
	std::uint64_t get_bytes_processed() const{
		return this->bytes_processed;
	}
};

class HashOutputFilter : public OutputFilter, public HashCalculator{
public:
	HashOutputFilter(std::ostream &stream, CryptoPP::HashTransformation *t): OutputFilter(stream), HashCalculator(t){}
	std::streamsize write(const char *s, std::streamsize n) override;
	bool flush() override{
		return this->internal_flush();
	}
};

class HashInputFilter : public InputFilter, public HashCalculator{
public:
	HashInputFilter(std::istream &stream, CryptoPP::HashTransformation *t): InputFilter(stream), HashCalculator(t){}
	std::streamsize read(char *s, std::streamsize n) override;
};

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
	const char *class_name() const override{
		return "HashInputFilter";
	}
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
	const char *class_name() const override{
		return "HashOutputFilter";
	}
};

}
