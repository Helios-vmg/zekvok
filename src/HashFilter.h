/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once
#include "Filters.h"

template <typename T>
struct template_parameter_passer{};

class HashCalculator{
	std::unique_ptr<CryptoPP::HashTransformation> hash;
	std::uint64_t bytes_processed;
protected:
	void update(const void *input, size_t length){
		if (length){
			this->hash->Update((const byte *)input, length);
			this->bytes_processed += length;
		}
	}
public:
	template <typename T>
	HashCalculator(const template_parameter_passer<T> &): bytes_processed(0){
		this->hash.reset(new T);
	}
	HashCalculator(const HashCalculator &) = delete;
	virtual ~HashCalculator(){}
	size_t get_hash_length() const;
	void get_result(void *buffer, size_t max_length);
	std::uint64_t get_bytes_processed() const{
		return this->bytes_processed;
	}
};

class HashOutputFilter : public OutputFilter, public HashCalculator{
public:
	template <typename T>
	HashOutputFilter(const template_parameter_passer<T> &x): HashCalculator(x){}
	HashOutputFilter(const HashOutputFilter &) = delete;
	std::streamsize write(write_callback_t cb, void *ud, const void *input, std::streamsize length) override;
	bool flush(write_callback_t cb, void *ud) override;
};

class HashInputFilter : public InputFilter, public HashCalculator{
	std::unique_ptr<CryptoPP::HashTransformation> hash;
public:
	template <typename T>
	HashInputFilter(const template_parameter_passer<T> &x): HashCalculator(x){}
	HashInputFilter(const HashInputFilter &) = delete;
	std::streamsize read(read_callback_t cb, void *ud, void *output, std::streamsize length) override;
};
