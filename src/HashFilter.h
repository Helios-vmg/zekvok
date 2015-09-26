#pragma once
#include "Filters.h"

class HashCalculator{
	std::unique_ptr<CryptoPP::HashTransformation> hash;
protected:
	void update(const void *input, size_t length){
		if (length)
			this->hash->Update((const byte *)input, length);
	}
public:
	template <typename T>
	HashCalculator(){
		this->hash.reset(new T);
	}
	virtual ~HashCalculator(){}
	size_t get_hash_length() const;
	void get_result(void *buffer, size_t max_length);
};

class HashOutputFilter : public OutputFilter, public HashCalculator{
protected:
	std::streamsize write(write_callback_t cb, void *ud, const void *input, std::streamsize length) override;
	bool flush(write_callback_t cb, void *ud) override;
public:
	template <typename T>
	HashOutputFilter(): HashCalculator<T>(){}
	~HashOutputFilter(){}
};

class HashInputFilter : public InputFilter, public HashCalculator{
	std::unique_ptr<CryptoPP::HashTransformation> hash;
protected:
	std::streamsize read(read_callback_t cb, void *ud, void *output, std::streamsize length) override;
public:
	template <typename T>
	HashInputFilter(): HashCalculator<T>(){}
	~HashInputFilter(){}
};
