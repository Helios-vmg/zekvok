/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"

const std::uint8_t salt[] = {
	0x8c, 0x4c, 0x94, 0x9e,
	0x5d, 0x63, 0x48, 0x90,
	0xc2, 0xf2, 0x89, 0x26,
	0xf3, 0x3a, 0x05, 0x0a,
};

RsaKeyPair::RsaKeyPair(const std::vector<std::uint8_t> &private_key, const std::vector<std::uint8_t> &public_key, const std::string &symmetric_key)
		: private_key(private_key)
		, public_key(public_key)
		, symmetric_key(symmetric_key)
		, priv_size((std::uint32_t)private_key.size()){
	random_number_generator->GenerateBlock(this->initialization_vector.data(), this->initialization_vector.size());
	this->encrypt();
}

RsaKeyPair::~RsaKeyPair(){
	std::fill(this->encrypted_private_key.begin(), this->encrypted_private_key.end(), 0);
	std::fill(this->private_key.begin(), this->private_key.end(), 0);
	std::fill(this->public_key.begin(), this->public_key.end(), 0);
	std::fill(this->symmetric_key.begin(), this->symmetric_key.end(), 0);
}

void RsaKeyPair::rollback_deserialization(){}

struct twofish_encryption{
	typedef CryptoPP::CCM<CryptoPP::Twofish>::Encryption algo_type;
	typedef CryptoPP::AuthenticatedEncryptionFilter filter_type;
};

struct twofish_decryption{
	typedef CryptoPP::CCM<CryptoPP::Twofish>::Decryption algo_type;
	typedef CryptoPP::AuthenticatedDecryptionFilter filter_type;
};

template <typename T>
std::vector<std::uint8_t> cryptoprocess_buffer(
		const std::vector<std::uint8_t> &buffer,
		size_t constant_size,
		const std::string &symmetric_key,
		const std::array<uint8_t, 12> &initialization_vector){
	CryptoPP::PKCS12_PBKDF<CryptoPP::SHA256> derivation;
	CryptoPP::SecByteBlock key(CryptoPP::Twofish::MAX_KEYLENGTH);
	derivation.DeriveKey(key.data(), key.size(), 0, (const byte *)symmetric_key.c_str(), symmetric_key.size(), salt, sizeof(salt), 1 << 10, 0);
	typename T::algo_type e;
	e.SetKeyWithIV(key, key.size(), initialization_vector.data(), initialization_vector.size());
	e.SpecifyDataLengths(0, constant_size, 0);

	std::string pre,
		post;
	pre.resize(buffer.size());
	std::copy(buffer.begin(), buffer.end(), pre.begin());
	CryptoPP::StringSource ssrc(pre, true, new typename T::filter_type(e, new CryptoPP::StringSink(post)));

	std::vector<std::uint8_t> ret;
	ret.resize(post.size());
	std::copy(post.begin(), post.end(), ret.begin());
	return ret;
}

void RsaKeyPair::encrypt(){
	if (!this->symmetric_key.size())
		throw IncorrectImplementationException();
	this->encrypted_private_key = cryptoprocess_buffer<twofish_encryption>(this->private_key, this->priv_size, this->symmetric_key, this->initialization_vector);
}

void RsaKeyPair::decrypt(){
	if (!this->symmetric_key.size())
		throw IncorrectImplementationException();
	try{
		this->private_key = cryptoprocess_buffer<twofish_decryption>(this->encrypted_private_key, this->priv_size, this->symmetric_key, this->initialization_vector);
	}catch (...){
		throw PrivateKeyDecryptionException("Decrypting the private key failed. Invalid symmetric key?");
	}
}

const std::vector<std::uint8_t> &RsaKeyPair::get_private_key(){
	if (this->private_key.size())
		return this->private_key;
	if (!this->symmetric_key.size())
			throw PrivateKeyDecryptionException();
	this->decrypt();
	return this->private_key;
}

const std::vector<std::uint8_t> &RsaKeyPair::get_private_key(const std::string &symmetric_key){
	if (this->private_key.size())
		return this->private_key;
	if (!this->symmetric_key.size()){
		if (!symmetric_key.size())
			throw PrivateKeyDecryptionException();
		this->symmetric_key = symmetric_key;
	}
	this->decrypt();
	return this->private_key;
}
