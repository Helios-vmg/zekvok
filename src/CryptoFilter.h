/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Filters.h"

enum class Algorithm{
	Rijndael,
	Twofish,
	Serpent,
};

class CryptoOutputFilter : public OutputFilter{
protected:
	bool flushed;
	virtual std::uint8_t *get_buffer(size_t &size) = 0;
	virtual CryptoPP::StreamTransformationFilter *get_filter() = 0;
	CryptoOutputFilter(std::ostream &stream):
		OutputFilter(stream),
		flushed(false){}
	bool internal_flush() override;
public:
	virtual ~CryptoOutputFilter(){}
	static std::shared_ptr<std::ostream> create(
		Algorithm algo,
		std::ostream &stream,
		const CryptoPP::SecByteBlock *key,
		const CryptoPP::SecByteBlock *iv
	);
	std::streamsize write(const char *s, std::streamsize n) override;
};

class CryptoInputFilter : public InputFilter{
	bool done;
protected:
	virtual std::uint8_t *get_buffer(size_t &size) = 0;
	virtual CryptoPP::StreamTransformationFilter *get_filter() = 0;
	CryptoInputFilter(std::istream &stream): InputFilter(stream), done(false){}
public:
	virtual ~CryptoInputFilter(){}
	static std::shared_ptr<std::istream> create(
		Algorithm algo,
		std::istream &stream,
		const CryptoPP::SecByteBlock *key,
		const CryptoPP::SecByteBlock *iv
	);
	std::streamsize read(char *s, std::streamsize n);
};
