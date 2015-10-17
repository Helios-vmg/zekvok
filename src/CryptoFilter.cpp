/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"

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
	GenericCryptoOutputFilter(std::ostream &stream, const std::vector<std::uint8_t> *public_key): CryptoOutputFilter(stream){
		this->data.reset(new impl);
		CryptoPP::RSA::PublicKey pub;
		pub.Load(CryptoPP::ArraySource((const byte *)&(*public_key)[0], public_key->size(), true));
		CryptoPP::AutoSeededRandomPool prng;

		CryptoPP::SecByteBlock key(T::MAX_KEYLENGTH);
		byte init_vector[16];
		prng.GenerateBlock(key.data(), key.size());
		prng.GenerateBlock(init_vector, sizeof(init_vector));

		{
			typedef CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA>>::Encryptor encryptor_t;
			typedef CryptoPP::PK_EncryptorFilter filter_t;
			encryptor_t enc(pub);
			CryptoPP::SecByteBlock buffer(key.size() + sizeof(init_vector));
			memcpy(buffer.data(), key.data(), key.size());
			memcpy(buffer.data() + key.size(), init_vector, sizeof(init_vector));
			std::string temp;
			CryptoPP::ArraySource(buffer.data(), buffer.size(), true, new filter_t(prng, enc, new CryptoPP::FileSink(*this->stream)));
		}

		this->data->e.SetKeyWithIV(key, key.size(), init_vector, sizeof(init_vector));
		this->data->filter.reset(new CryptoPP::StreamTransformationFilter(this->data->e));
	}
};

std::shared_ptr<std::ostream> CryptoOutputFilter::create(Algorithm algo, std::ostream &stream, const std::vector<std::uint8_t> *public_key){
	std::shared_ptr<std::ostream> ret;
	switch (algo){
		case Algorithm::Rijndael:
			ret.reset(new boost::iostreams::stream<GenericCryptoOutputFilter<CryptoPP::Rijndael>>(stream, public_key));
			break;
		case Algorithm::Serpent:
			ret.reset(new boost::iostreams::stream<GenericCryptoOutputFilter<CryptoPP::Serpent>>(stream, public_key));
			break;
		case Algorithm::Twofish:
			ret.reset(new boost::iostreams::stream<GenericCryptoOutputFilter<CryptoPP::Twofish>>(stream, public_key));
			break;
	}
	return ret;
}

std::streamsize CryptoOutputFilter::write(const char *s, std::streamsize n){
	auto filter = this->get_filter();
	filter->Put((const byte *)s, n);
	size_t size;
	auto buffer = this->get_buffer(size);
	while (1){
		auto read = filter->Get(buffer, size);
		if (!read)
			break;
		this->next_write((const char *)buffer, read);
	}
	return n;
}

bool CryptoOutputFilter::flush(){
	auto filter = this->get_filter();
	size_t size;
	auto buffer = this->get_buffer(size);
	filter->MessageEnd();
	while (1){
		auto read = filter->Get(buffer, size);
		if (!read)
			break;
		this->next_write((const char *)buffer, read);
	}
	return OutputFilter::flush();
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
	GenericCryptoInputFilter(std::istream &stream, const std::vector<std::uint8_t> *private_key): CryptoInputFilter(stream){
		this->data.reset(new impl);
		CryptoPP::RSA::PrivateKey priv;
		priv.Load(CryptoPP::ArraySource((const byte *)&(*private_key)[0], private_key->size(), true));
		CryptoPP::AutoSeededRandomPool prng;

		CryptoPP::SecByteBlock key(T::MAX_KEYLENGTH);
		byte init_vector[16];
		{
			auto n = 4096 / 8;

			typedef CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA>>::Decryptor decryptor_t;
			typedef CryptoPP::PK_DecryptorFilter filter_t;
			decryptor_t dec(priv);
			CryptoPP::SecByteBlock buffer(key.size() + sizeof(init_vector));
			CryptoPP::SecByteBlock temp(n);
			auto read = this->next_read((char *)temp.data(), temp.size());
			if (read != temp.size())
				throw std::exception("!!!");
			CryptoPP::ArraySource(temp.data(), temp.size(), true, new filter_t(prng, dec, new CryptoPP::ArraySink(buffer.data(), buffer.size())));
			memcpy(key.data(), buffer.data(), key.size());
			memcpy(init_vector, buffer.data() + key.size(), sizeof(init_vector));
		}

		this->data->d.SetKeyWithIV(key, key.size(), init_vector, sizeof(init_vector));
		this->data->filter.reset(new CryptoPP::StreamTransformationFilter(this->data->d));
	}
};

std::shared_ptr<std::istream> CryptoInputFilter::create(Algorithm algo, std::istream &stream, const std::vector<std::uint8_t> *private_key){
	std::shared_ptr<std::istream> ret;
	switch (algo){
		case Algorithm::Rijndael:
			ret.reset(new boost::iostreams::stream<GenericCryptoInputFilter<CryptoPP::Rijndael>>(stream, private_key));
			break;
		case Algorithm::Serpent:
			ret.reset(new boost::iostreams::stream<GenericCryptoInputFilter<CryptoPP::Serpent>>(stream, private_key));
			break;
		case Algorithm::Twofish:
			ret.reset(new boost::iostreams::stream<GenericCryptoInputFilter<CryptoPP::Twofish>>(stream, private_key));
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
		auto read = this->next_read((char *)buffer, size);
		if (read >= 0)
			filter->Put(buffer, read);
		else if (!bad){
			if (!this->done){
				filter->MessageEnd();
				this->done = true;
			}
			bad = true;
		}
		read = filter->Get((byte *)s, n);
		if (!read && bad)
			break;
		s += read;
		n -= read;
		ret += read;
	}
	return !ret && bad ? -1 : ret;
}
