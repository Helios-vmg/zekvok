/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Filters.h"

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
