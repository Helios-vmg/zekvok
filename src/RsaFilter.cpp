/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "RsaFilter.h"
#include "Utility.h"

void encrypt_stream(CryptoPP::RandomNumberGenerator &prng, std::ostream &output, std::istream &input, CryptoPP::RSA::PublicKey &pub){
	CryptoPP::SecByteBlock key(CryptoPP::Twofish::MAX_KEYLENGTH);
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
		std::stringstream temp;
		CryptoPP::ArraySource(buffer.data(), buffer.size(), true, new filter_t(prng, enc, new CryptoPP::FileSink(temp)));
		auto array = serialize_fixed_le_int((std::uint32_t)temp.str().size());
		output.write((const char *)array.data(), array.size());
	}

	CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption e;
	e.SetKeyWithIV(key, key.size(), init_vector, sizeof(init_vector));

	CryptoPP::FileSource(input, true, new CryptoPP::StreamTransformationFilter(e, new CryptoPP::FileSink(output)));
}

void decrypt_stream(CryptoPP::RandomNumberGenerator &prng, std::ostream &output, std::istream &input, CryptoPP::RSA::PrivateKey &priv){
	CryptoPP::SecByteBlock key(CryptoPP::Twofish::MAX_KEYLENGTH);
	byte init_vector[16];
	{
		std::uint32_t n;
		char serialized[sizeof(n)];
		input.read(serialized, sizeof(serialized));
		if (input.gcount() != sizeof(serialized))
			throw std::exception("!!!");
		n = deserialize_fixed_le_int<std::uint32_t>(serialized);

		typedef CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA>>::Decryptor decryptor_t;
		typedef CryptoPP::PK_DecryptorFilter filter_t;
		decryptor_t dec(priv);
		CryptoPP::SecByteBlock buffer(key.size() + sizeof(init_vector));
		CryptoPP::SecByteBlock temp(n);
		input.read((char *)temp.data(), temp.size());
		if (input.gcount() != temp.size())
			throw std::exception("!!!");
		CryptoPP::ArraySource(temp.data(), temp.size(), true, new filter_t(prng, dec, new CryptoPP::ArraySink(buffer.data(), buffer.size())));
		memcpy(key.data(), buffer.data(), key.size());
		memcpy(init_vector, buffer.data() + key.size(), sizeof(init_vector));
	}

	CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption d;
	d.SetKeyWithIV(key, key.size(), init_vector, sizeof(init_vector));

	CryptoPP::FileSource(input, true, new CryptoPP::StreamTransformationFilter(d, new CryptoPP::FileSink(output)));
}
