/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "CryptoFilter.h"

template <typename T>
class GenericCryptoOutputFilter : public CryptoOutputFilter{
	struct impl{
		typename CryptoPP::CBC_Mode<T>::Encryption e;
		std::unique_ptr<CryptoPP::StreamTransformationFilter> filter;
		std::uint8_t buffer[4096];
	};
	
	std::shared_ptr<impl> data;
protected:
	std::uint8_t *get_buffer(size_t &size) override{
		size = sizeof(this->data->buffer);
		return this->data->buffer;
	}
	CryptoPP::StreamTransformationFilter *get_filter() override{
		return this->data->filter.get();
	}
public:
	GenericCryptoOutputFilter(
			std::ostream &stream,
			const CryptoPP::SecByteBlock &key,
			const CryptoPP::SecByteBlock &iv): CryptoOutputFilter(stream){
		this->data.reset(new impl);
		this->data->e.SetKeyWithIV(key, key.size(), iv.data(), iv.size());
		this->data->filter.reset(new CryptoPP::StreamTransformationFilter(this->data->e));
	}
	~GenericCryptoOutputFilter(){
		if (*this->copies == 1){
			this->enable_flush();
			// ReSharper disable once CppVirtualFunctionCallInsideCtor
			this->flush();
		}
	}
};

std::shared_ptr<std::ostream> CryptoOutputFilter::create(
		Algorithm algo,
		std::ostream &stream,
		const CryptoPP::SecByteBlock *key,
		const CryptoPP::SecByteBlock *iv){
	std::shared_ptr<std::ostream> ret;
	switch (algo){
		case Algorithm::Rijndael:
			ret.reset(new boost::iostreams::stream<GenericCryptoOutputFilter<CryptoPP::Rijndael>>(stream, *key, *iv));
			break;
		case Algorithm::Serpent:
			ret.reset(new boost::iostreams::stream<GenericCryptoOutputFilter<CryptoPP::Serpent>>(stream, *key, *iv));
			break;
		case Algorithm::Twofish:
			ret.reset(new boost::iostreams::stream<GenericCryptoOutputFilter<CryptoPP::Twofish>>(stream, *key, *iv));
			break;
	}
	return ret;
}

std::streamsize CryptoOutputFilter::write(const char *s, std::streamsize n){
	auto filter = this->get_filter();
	filter->Put(reinterpret_cast<const byte *>(s), n);
	size_t size;
	auto buffer = this->get_buffer(size);
	while (1){
		auto read = filter->Get(buffer, size);
		if (!read)
			break;
		this->next_write(reinterpret_cast<const char *>(buffer), read);
	}
	return n;
}

bool CryptoOutputFilter::internal_flush(){
	auto filter = this->get_filter();
	size_t size;
	auto buffer = this->get_buffer(size);
	if (!this->flushed){
		filter->MessageEnd();
		this->flushed = true;
	}
	while (1){
		auto read = filter->Get(buffer, size);
		if (!read)
			break;
		this->next_write(reinterpret_cast<const char *>(buffer), read);
	}
	return OutputFilter::internal_flush();
}

template <typename T>
class GenericCryptoInputFilter : public CryptoInputFilter{
	struct impl{
		typename CryptoPP::CBC_Mode<T>::Decryption d;
		std::unique_ptr<CryptoPP::StreamTransformationFilter> filter;
		std::uint8_t buffer[4096];
	};
	
	std::shared_ptr<impl> data;
protected:
	std::uint8_t *get_buffer(size_t &size) override{
		size = sizeof(this->data->buffer);
		return this->data->buffer;
	}
	CryptoPP::StreamTransformationFilter *get_filter() override{
		return this->data->filter.get();
	}
public:
	GenericCryptoInputFilter(std::istream &stream, const CryptoPP::SecByteBlock &key, const CryptoPP::SecByteBlock &iv): CryptoInputFilter(stream){
		this->data.reset(new impl);
		this->data->d.SetKeyWithIV(key, key.size(), iv.data(), iv.size());
		this->data->filter.reset(new CryptoPP::StreamTransformationFilter(this->data->d));
	}
};

std::shared_ptr<std::istream> CryptoInputFilter::create(
		Algorithm algo,
		std::istream &stream,
		const CryptoPP::SecByteBlock *key,
		const CryptoPP::SecByteBlock *iv){
	std::shared_ptr<std::istream> ret;
	switch (algo){
		case Algorithm::Rijndael:
			ret.reset(new boost::iostreams::stream<GenericCryptoInputFilter<CryptoPP::Rijndael>>(stream, *key, *iv));
			break;
		case Algorithm::Serpent:
			ret.reset(new boost::iostreams::stream<GenericCryptoInputFilter<CryptoPP::Serpent>>(stream, *key, *iv));
			break;
		case Algorithm::Twofish:
			ret.reset(new boost::iostreams::stream<GenericCryptoInputFilter<CryptoPP::Twofish>>(stream, *key, *iv));
			break;
	}
	return ret;
}

std::streamsize CryptoInputFilter::read(char *s, std::streamsize n){
	if (this->done)
		return -1;
	std::streamsize ret = 0;
	bool bad = false;
	size_t size;
	auto buffer = this->get_buffer(size);
	auto filter = this->get_filter();
	while (n){
		auto read = this->next_read(reinterpret_cast<char *>(buffer), size);
		if (read >= 0)
			filter->Put(buffer, read);
		else if (!bad){
			if (!this->done){
				filter->MessageEnd();
				this->done = true;
			}
			bad = true;
		}
		read = filter->Get(reinterpret_cast<byte *>(s), n);
		if (!read && bad)
			break;
		s += read;
		n -= read;
		ret += read;
	}
	return !ret && bad ? -1 : ret;
}
