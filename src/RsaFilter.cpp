/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "RsaFilter.h"

RsaOutputFilter::RsaOutputFilter(std::ostream &stream, const std::vector<std::uint8_t> *public_key): OutputFilter(stream){
	this->data.reset(new impl);
	{
		CryptoPP::StringSource temp((const byte *)&(*public_key)[0], public_key->size(), true);
		this->data->key.Load(temp);
	}
	this->data->encryptor.reset(new encryptor_t(this->data->key));
	this->data->filter.reset(new filter_t(this->data->prng, *this->data->encryptor));
}

std::streamsize RsaOutputFilter::write(const char *s, std::streamsize n){
	if (this->data->flushed)
		return 0;
	auto &filter = *this->data->filter;
	filter.Put((const byte *)s, n);
	auto &buffer = this->data->buffer;
	while (1){
		auto read = filter.Get(buffer, sizeof(buffer));
		if (!read)
			break;
		this->next_write((const char *)buffer, read);
	}
	return n;
}

bool RsaOutputFilter::flush(){
	if (!this->data->flushed){
		auto &filter = *this->data->filter;
		filter.LastPut(nullptr, 0);
		auto &buffer = this->data->buffer;
		while (1){
			auto read = filter.Get(buffer, sizeof(buffer));
			if (!read)
				break;
			this->next_write((const char *)buffer, read);
		}
		this->data->flushed = true;
	}
	return OutputFilter::flush();
}

RsaInputFilter::RsaInputFilter(std::istream &stream, const std::vector<std::uint8_t> *private_key): InputFilter(stream){
	this->data.reset(new impl);
	{
		CryptoPP::StringSource temp((const byte *)&(*private_key)[0], private_key->size(), true);
		this->data->key.Load(temp);
	}
	this->data->decryptor.reset(new decryptor_t(this->data->key));
	this->data->filter.reset(new filter_t(this->data->prng, *this->data->decryptor));
}

std::streamsize RsaInputFilter::read(char *s, std::streamsize n){
	std::streamsize ret = 0;
	bool bad = false;
	auto &buffer = this->data->buffer;
	auto &filter = *this->data->filter;
	while (1){
		auto read = this->next_read((char *)buffer, sizeof(buffer));
		if (read < 0){
			bad = true;
			break;
		}
		filter.Put(buffer, read);
		read = filter.Get((byte *)s, n);
		if (!read)
			break;
		s += read;
		n -= read;
		ret += read;
	}
	return !ret && bad ? -1 : ret;
}
