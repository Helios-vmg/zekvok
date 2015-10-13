/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "RsaFilter.h"

RsaOutputFilter::RsaOutputFilter(std::ostream &stream, std::vector<std::uint8_t> &public_key): OutputFilter(stream){
	this->data.reset(new impl);
	{
		CryptoPP::StringSource temp((const byte *)&public_key[0], public_key.size(), true);
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
