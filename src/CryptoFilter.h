/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Filters.h"
#include "StreamProcessor.h"

enum class Algorithm{
	Rijndael,
	Twofish,
	Serpent,
};

namespace zstreams{

class CryptoOutputStream : public OutputStream{
protected:
	bool flushed;
	virtual CryptoPP::StreamTransformationFilter *get_filter() = 0;
	CryptoOutputStream(OutputStream &stream):
		OutputStream(stream),
		flushed(false){}
	void work() override;
	void flush_impl() override;
	void flush_filter(CryptoPP::StreamTransformationFilter *);
public:
	virtual ~CryptoOutputStream(){}
	static Stream<CryptoOutputStream> create(
		Algorithm algo,
		OutputStream &stream,
		const CryptoPP::SecByteBlock *key,
		const CryptoPP::SecByteBlock *iv
	);
	const char *class_name() const override{
		return "CryptoOutputStream";
	}
};

class CryptoInputStream : public InputStream{
protected:
	virtual CryptoPP::StreamTransformationFilter *get_filter() = 0;
	CryptoInputStream(InputStream &stream): InputStream(stream){}
	void work() override;
	IGNORE_FLUSH_COMMAND
public:
	virtual ~CryptoInputStream(){}
	static Stream<CryptoInputStream> create(
		Algorithm algo,
		InputStream &stream,
		const CryptoPP::SecByteBlock *key,
		const CryptoPP::SecByteBlock *iv
	);
	const char *class_name() const override{
		return "CryptoInputStream";
	}
};

}
