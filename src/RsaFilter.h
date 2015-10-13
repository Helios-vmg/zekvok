/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Filters.h"

class RsaOutputFilter : public OutputFilter{
	typedef CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA>>::Encryptor encryptor_t;
	typedef CryptoPP::PK_EncryptorFilter filter_t;
	struct impl{
		CryptoPP::RSA::PublicKey key;
		CryptoPP::AutoSeededRandomPool prng;
		std::unique_ptr<encryptor_t> encryptor;
		std::unique_ptr<filter_t> filter;
		bool flushed;
		std::uint8_t buffer[4096];
		impl(): flushed(false){}
	};
	
	std::shared_ptr<impl> data;

public:
	RsaOutputFilter(std::ostream &stream, std::vector<std::uint8_t> &public_key);
	std::streamsize write(const char *s, std::streamsize n) override;
	bool flush() override;
};
