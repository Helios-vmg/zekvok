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

class CryptoSink : public Sink{
protected:
	bool flushed;
	virtual CryptoPP::StreamTransformationFilter *get_filter() = 0;
	CryptoSink(Sink &stream):
		Sink(stream),
		flushed(false){}
	void work() override;
	void flush_impl() override;
	void flush_filter(CryptoPP::StreamTransformationFilter *);
public:
	virtual ~CryptoSink(){}
	static Stream<CryptoSink> create(
		Algorithm algo,
		Sink &stream,
		const CryptoPP::SecByteBlock *key,
		const CryptoPP::SecByteBlock *iv
	);
	const char *class_name() const override{
		return "CryptoOutputStream";
	}
};

class CryptoSource : public Source{
protected:
	virtual CryptoPP::StreamTransformationFilter *get_filter() = 0;
	CryptoSource(Source &stream): Source(stream){}
	void work() override;
	IGNORE_FLUSH_COMMAND
public:
	virtual ~CryptoSource(){}
	static Stream<CryptoSource> create(
		Algorithm algo,
		Source &stream,
		const CryptoPP::SecByteBlock *key,
		const CryptoPP::SecByteBlock *iv
	);
	const char *class_name() const override{
		return "CryptoInputStream";
	}
};

}
