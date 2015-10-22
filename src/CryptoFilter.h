/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

void encrypt_stream(CryptoPP::RandomNumberGenerator &prng, std::ostream &output, std::istream &input, CryptoPP::RSA::PublicKey &pub);
void decrypt_stream(CryptoPP::RandomNumberGenerator &prng, std::ostream &output, std::istream &input, CryptoPP::RSA::PrivateKey &priv);

enum class Algorithm{
	Rijndael,
	Twofish,
	Serpent,
};

class CryptoOutputFilter : public OutputFilter{
protected:
	virtual std::uint8_t *get_buffer(size_t &size) = 0;
	virtual CryptoPP::StreamTransformationFilter *get_filter() = 0;
	CryptoOutputFilter(std::ostream &stream): OutputFilter(stream){}
public:
	static std::shared_ptr<std::ostream> create(
		Algorithm algo,
		std::ostream &stream,
		const CryptoPP::SecByteBlock *key,
		const CryptoPP::SecByteBlock *iv
	);
	std::streamsize write(const char *s, std::streamsize n) override;
	bool flush() override;
};

class CryptoInputFilter : public InputFilter{
	bool done;
protected:
	virtual std::uint8_t *get_buffer(size_t &size) = 0;
	virtual CryptoPP::StreamTransformationFilter *get_filter() = 0;
	CryptoInputFilter(std::istream &stream): InputFilter(stream), done(false){}
public:
	static std::shared_ptr<std::istream> create(
		Algorithm algo,
		std::istream &stream,
		const CryptoPP::SecByteBlock *key,
		const CryptoPP::SecByteBlock *iv
	);
	std::streamsize read(char *s, std::streamsize n);
};
