/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "CryptoFilter.h"

namespace zstreams{

template <typename T>
class GenericCryptoOutputStream : public CryptoOutputStream{
	typename CryptoPP::CBC_Mode<T>::Encryption e;
	std::unique_ptr<CryptoPP::StreamTransformationFilter> filter;

protected:
	CryptoPP::StreamTransformationFilter *get_filter() override{
		return this->filter.get();
	}
public:
	GenericCryptoOutputStream(
			OutputStream &stream,
			const CryptoPP::SecByteBlock &key,
			const CryptoPP::SecByteBlock &iv): CryptoOutputStream(stream){
		this->e.SetKeyWithIV(key, key.size(), iv.data(), iv.size());
		this->filter.reset(new CryptoPP::StreamTransformationFilter(this->e));
	}
};

Stream<CryptoOutputStream> CryptoOutputStream::create(Algorithm algo, OutputStream &stream, const CryptoPP::SecByteBlock *key, const CryptoPP::SecByteBlock *iv){
	switch (algo){
		case Algorithm::Rijndael:
			return Stream<GenericCryptoOutputStream<CryptoPP::Rijndael>>(stream, *key, *iv);
		case Algorithm::Serpent:
			return Stream<GenericCryptoOutputStream<CryptoPP::Serpent>>(stream, *key, *iv);
		case Algorithm::Twofish:
			return Stream<GenericCryptoOutputStream<CryptoPP::Twofish>>(stream, *key, *iv);
	}
	zekvok_assert(false);
}

void CryptoOutputStream::work(){
	auto filter = this->get_filter();
	while (true){
		{
			auto segment = this->read();
			if (segment.get_type() == SegmentType::Eof)
				break;
			auto data = segment.get_data();
			filter->Put(data.data, data.size);
		}

		this->flush_filter(filter);
	}
	this->flush_impl();
}

static void flush_filter(Processor *p, CryptoPP::StreamTransformationFilter *filter){
	while (true){
		auto segment = p->allocate_segment();
		auto data = segment.get_data();
		auto read = filter->Get(data.data, data.size);
		if (!read)
			return;
		segment.trim_to_size(read);
		p->write(segment);
	}
}

void CryptoOutputStream::flush_filter(CryptoPP::StreamTransformationFilter *filter){
	zstreams::flush_filter(this, filter);
}

void CryptoOutputStream::flush_impl(){
	auto filter = this->get_filter();
	if (!this->flushed){
		filter->MessageEnd();
		this->flushed = true;
	}
	this->flush_filter(filter);
}

template <typename T>
class GenericCryptoInputStream : public CryptoInputStream{
	typename CryptoPP::CBC_Mode<T>::Decryption d;
	std::unique_ptr<CryptoPP::StreamTransformationFilter> filter;

protected:
	CryptoPP::StreamTransformationFilter *get_filter() override{
		return this->filter.get();
	}
public:
	GenericCryptoInputStream(InputStream &stream, const CryptoPP::SecByteBlock &key, const CryptoPP::SecByteBlock &iv): CryptoInputStream(stream){
		this->d.SetKeyWithIV(key, key.size(), iv.data(), iv.size());
		this->filter.reset(new CryptoPP::StreamTransformationFilter(this->d));
	}
};

Stream<CryptoInputStream> CryptoInputStream::create(
		Algorithm algo,
		InputStream &stream,
		const CryptoPP::SecByteBlock *key,
		const CryptoPP::SecByteBlock *iv){
	switch (algo){
		case Algorithm::Rijndael:
			return Stream<GenericCryptoInputStream<CryptoPP::Rijndael>>(stream, *key, *iv);
		case Algorithm::Serpent:
			return Stream<GenericCryptoInputStream<CryptoPP::Serpent>>(stream, *key, *iv);
		case Algorithm::Twofish:
			return Stream<GenericCryptoInputStream<CryptoPP::Twofish>>(stream, *key, *iv);
	}
	zekvok_assert(false);
}

void CryptoInputStream::work(){
	auto filter = this->get_filter();
	bool done = false;

	while (!done){
		{
			auto segment = this->read();
			if (segment.get_type() == SegmentType::Eof){
				filter->MessageEnd();
				done = true;
			}else{
				auto data = segment.get_data();
				filter->Put(data.data, data.size);
			}
		}

		zstreams::flush_filter(this, filter);
	}
	Segment eof(SegmentType::Eof);
	this->write(eof);
}

}
