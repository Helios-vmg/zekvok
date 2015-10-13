/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

void encrypt_stream(CryptoPP::RandomNumberGenerator &prng, std::ostream &output, std::istream &input, CryptoPP::RSA::PublicKey &pub);
void decrypt_stream(CryptoPP::RandomNumberGenerator &prng, std::ostream &output, std::istream &input, CryptoPP::RSA::PrivateKey &priv);
