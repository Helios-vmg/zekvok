/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"

const Algorithm default_crypto_algorithm = Algorithm::Twofish;

ArchiveKeys::ArchiveKeys(size_t key_size, size_t iv_size){
	this->init(key_size, iv_size);
}

void ArchiveKeys::init(size_t key_size, size_t iv_size, bool randomize){
	this->size = 0;
	for (auto &i : this->keys){
		i.resize(key_size);
		this->size += key_size;
		if (randomize)
			random_number_generator->GenerateBlock(i.data(), i.size());
	}
	for (auto &i : this->ivs){
		i.resize(iv_size);
		this->size += iv_size;
		if (randomize)
			random_number_generator->GenerateBlock(i.data(), i.size());
	}
	zekvok_assert(this->size <= 4096 / 8 - 42);
}

const CryptoPP::SecByteBlock &ArchiveKeys::get_key(size_t i) const{
	if (i > array_size(this->keys))
		throw IncorrectImplementationException();
	return this->keys[i];
}

const CryptoPP::SecByteBlock &ArchiveKeys::get_iv(size_t i) const{
	if (i > array_size(this->ivs))
		throw IncorrectImplementationException();
	return this->ivs[i];
}

ArchiveKeys::ArchiveKeys(std::istream &stream, RsaKeyPair &keypair){
	this->init(CryptoPP::Twofish::MAX_KEYLENGTH, CryptoPP::Twofish::BLOCKSIZE, false);

	auto &private_key = keypair.get_private_key();

	CryptoPP::RSA::PrivateKey priv;
	priv.Load(CryptoPP::ArraySource((const byte *)&private_key[0], private_key.size(), true));

	CryptoPP::SecByteBlock buffer(this->size);
	{
		auto n = 4096 / 8;

		typedef CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA>>::Decryptor decryptor_t;
		typedef CryptoPP::PK_DecryptorFilter filter_t;
		decryptor_t dec(priv);
		CryptoPP::SecByteBlock temp(n);
		stream.read((char *)temp.data(), temp.size());
		auto read = stream.gcount();
		if (read != temp.size())
			throw std::exception("!!!");
		CryptoPP::ArraySource(temp.data(), temp.size(), true, new filter_t(*random_number_generator, dec, new CryptoPP::ArraySink(buffer.data(), buffer.size())));
	}

	size_t offset = 0;
	for (size_t i = 0; i < this->key_count; i++){
		auto &key = this->keys[i];
		auto &iv = this->ivs[i];
		memcpy(key.data(), buffer.data() + offset, key.size());
		offset += key.size();
		memcpy(iv.data(), buffer.data() + offset, iv.size());
		offset += iv.size();
	}
}

std::unique_ptr<ArchiveKeys> ArchiveKeys::create_and_save(std::ostream &stream, RsaKeyPair *keypair){
	std::unique_ptr<ArchiveKeys> ret;
	if (keypair){
		ret = make_unique(new ArchiveKeys(CryptoPP::Twofish::MAX_KEYLENGTH, CryptoPP::Twofish::BLOCKSIZE));

		CryptoPP::RSA::PublicKey pub;
		auto &public_key = keypair->get_public_key();
		pub.Load(CryptoPP::ArraySource((const byte *)&public_key[0], public_key.size(), true));

		typedef CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA>>::Encryptor encryptor_t;
		typedef CryptoPP::PK_EncryptorFilter filter_t;

		encryptor_t enc(pub);
		CryptoPP::SecByteBlock buffer(ret->get_size());
		size_t offset = 0;
		for (size_t i = 0; i < ret->key_count; i++){
			auto &key = ret->get_key(i);
			auto &iv = ret->get_iv(i);
			memcpy(buffer.data() + offset, key.data(), key.size());
			offset += key.size();
			memcpy(buffer.data() + offset, iv.data(), iv.size());
			offset += iv.size();
		}
		CryptoPP::ArraySource(buffer.data(), buffer.size(), true, new filter_t(*random_number_generator, enc, new CryptoPP::FileSink(stream)));
	}
	return ret;
}

ArchiveReader::ArchiveReader(const path_t &path, RsaKeyPair *keypair):
		manifest_offset(-1),
		path(path),
		keypair(keypair){
	if (!boost::filesystem::exists(path) || !boost::filesystem::is_regular_file(path))
		throw FileNotFoundException(path);

	auto stream = this->get_stream();
	if (this->keypair)
		archive_keys.reset(new ArchiveKeys(*stream, *this->keypair));
}

std::unique_ptr<std::istream> ArchiveReader::get_stream(){
	auto ret = make_unique((std::istream *)new boost::filesystem::ifstream(path, std::ios::binary));
	if (!*ret)
		throw FileNotFoundException(path);
	return ret;
}

std::shared_ptr<VersionManifest> ArchiveReader::read_manifest(){
	auto stream = this->get_stream();
	if (this->manifest_offset < 0){
		const int uint64_length = sizeof(std::uint64_t);
		std::int64_t start = -uint64_length - sha256_digest_length;
		stream->seekg(start, std::ios::end);
		char temp[uint64_length];
		stream->read(temp, uint64_length);
		if (stream->gcount() != uint64_length)
			throw std::exception("Invalid data");
		deserialize_fixed_le_int(this->manifest_size, temp);
		start -= this->manifest_size;
		stream->seekg(start, std::ios::end);
		this->manifest_offset = stream->tellg();
	}else
		stream->seekg(this->manifest_offset);
	{
		boost::iostreams::stream<BoundedInputFilter> bounded(*stream, this->manifest_size);
		std::istream *stream = &bounded;
		std::shared_ptr<std::istream> crypto;
		if (this->keypair){
			crypto = CryptoInputFilter::create(
				default_crypto_algorithm,
				*stream,
				&this->archive_keys->get_key((size_t)KeyIndices::ManifestKey),
				&this->archive_keys->get_iv((size_t)KeyIndices::ManifestKey)
			);
			stream = crypto.get();
		}
		boost::iostreams::stream<LzmaInputFilter> lzma(*stream);

		ImplementedDeserializerStream ds(lzma);
		this->version_manifest.reset(ds.begin_deserialization<VersionManifest>(config::include_typehashes));
		if (!this->version_manifest)
			throw std::exception("Error during deserialization");
	}

	this->base_objects_offset = this->manifest_offset - this->version_manifest->archive_metadata.entries_size_in_archive;
	this->stream_ids = this->version_manifest->archive_metadata.stream_ids;
	this->stream_sizes = this->version_manifest->archive_metadata.stream_sizes;

	return this->version_manifest;
}

std::vector<std::shared_ptr<FileSystemObject>> ArchiveReader::read_base_objects(){
	if (!this->version_manifest)
		this->read_manifest();
	zekvok_assert(this->version_manifest);
	decltype(this->base_objects) ret;
	ret.reserve(this->version_manifest->archive_metadata.entry_sizes.size());
	auto stream = this->get_stream();
	stream->seekg(this->base_objects_offset);
	{
		boost::iostreams::stream<BoundedInputFilter> bounded(*stream, this->manifest_offset - this->base_objects_offset);
		std::istream *stream = &bounded;
		std::shared_ptr<std::istream> crypto;
		if (this->keypair){
			crypto = CryptoInputFilter::create(
				default_crypto_algorithm,
				*stream,
				&this->archive_keys->get_key((size_t)KeyIndices::FileObjectDataKey),
				&this->archive_keys->get_iv((size_t)KeyIndices::FileObjectDataKey)
			);
			stream = crypto.get();
		}
		boost::iostreams::stream<LzmaInputFilter> lzma(*stream);

		for (const auto &s : this->version_manifest->archive_metadata.entry_sizes){
			boost::iostreams::stream<BoundedInputFilter> bounded2(lzma, s);
			ImplementedDeserializerStream ds(bounded2);
			std::shared_ptr<FileSystemObject> fso(ds.begin_deserialization<FileSystemObject>(config::include_typehashes));
			if (!fso)
				throw std::exception("Error during deserialization");
			ret.push_back(fso);
		}
	}
	this->base_objects = std::move(ret);
	return this->base_objects;
}

void ArchiveReader::read_everything(read_everything_co_t::push_type &sink){
	if (!this->version_manifest)
		this->read_manifest();
	auto ptr = this->get_stream();
	auto file_data_start = this->keypair ? 4096 / 8 : 0;
	ptr->seekg(file_data_start);

	boost::iostreams::stream<BoundedInputFilter> bounded(*ptr, this->base_objects_offset - file_data_start);
	std::istream *stream = &bounded;

	std::shared_ptr<std::istream> crypto;
	if (this->keypair){
		crypto = CryptoInputFilter::create(
			default_crypto_algorithm,
			*stream,
			&this->archive_keys->get_key((size_t)KeyIndices::FileDataKey),
			&this->archive_keys->get_iv((size_t)KeyIndices::FileDataKey)
		);
		stream = crypto.get();
	}
	boost::iostreams::stream<LzmaInputFilter> lzma(*stream);

	zekvok_assert(this->stream_ids.size() == this->stream_sizes.size());
	for (size_t i = 0; i < this->stream_ids.size(); i++){
		boost::iostreams::stream<BoundedInputFilter> bounded2(lzma, this->stream_sizes[i]);
		sink(std::make_pair(this->stream_ids[i], &bounded2));
		//Discard left over bytes.
		boost::iostreams::stream<NullOutputStream> null(0);
		null << bounded2.rdbuf();
	}
}

ArchiveReader::read_everything_co_t::pull_type ArchiveReader::read_everything(){
	return read_everything_co_t::pull_type([this](read_everything_co_t::push_type &sink){
		this->read_everything(sink);
	});
}

ArchiveWriter::ArchiveWriter(KernelTransaction &tx, const path_t &path, RsaKeyPair *keypair): keypair(keypair), tx(tx){
	this->stream.reset(new boost::iostreams::stream<TransactedFileSink>(this->tx, path.c_str()));
}

void ArchiveWriter::process(std::unique_ptr<ArchiveWriter_helper> *begin, std::unique_ptr<ArchiveWriter_helper> *end){
	if (end - begin != 3)
		throw IncorrectImplementationException();

	sha256_digest complete_hash;
	{
		hash_stream_t overall_hash(*this->stream, new CryptoPP::SHA256);
		auto keys = ArchiveKeys::create_and_save(overall_hash, this->keypair);
		size_t archive_key_index = 0;
		
		this->initial_fso_offset = 0;
		{
			boost::iostreams::stream<ByteCounterOutputFilter> counter(overall_hash, &this->initial_fso_offset);
			std::ostream *stream = &counter;
			std::shared_ptr<std::ostream> crypto;
			if (this->keypair){
				crypto = CryptoOutputFilter::create(default_crypto_algorithm, *stream, &keys->get_key(archive_key_index), &keys->get_iv(archive_key_index));
				archive_key_index++;
				stream = crypto.get();
			}
			this->add_files(*stream, begin);
		}
		this->entries_size_in_archive = 0;
		{
			boost::iostreams::stream<ByteCounterOutputFilter> counter(overall_hash, &this->entries_size_in_archive);
			std::ostream *stream = &counter;
			std::shared_ptr<std::ostream> crypto;
			if (this->keypair){
				crypto = CryptoOutputFilter::create(default_crypto_algorithm, *stream, &keys->get_key(archive_key_index), &keys->get_iv(archive_key_index));
				archive_key_index++;
				stream = crypto.get();
			}
			this->add_base_objects(*stream, begin);
		}
		{
			std::uint64_t manifest_length = 0;
			{
				boost::iostreams::stream<ByteCounterOutputFilter> counter(overall_hash, &manifest_length);
				std::ostream *stream = &counter;
				std::shared_ptr<std::ostream> crypto;
				if (this->keypair){
					crypto = CryptoOutputFilter::create(default_crypto_algorithm, *stream, &keys->get_key(archive_key_index), &keys->get_iv(archive_key_index));
					archive_key_index++;
					stream = crypto.get();
				}
				this->add_version_manifest(*stream, begin);
			}
			auto s_manifest_length = serialize_fixed_le_int(manifest_length);
			overall_hash.write((const char *)s_manifest_length.data(), s_manifest_length.size());
		}
		overall_hash->get_result(complete_hash.data(), complete_hash.size());
	}
	this->stream->write((const char *)complete_hash.data(), complete_hash.size());
}

void ArchiveWriter::add_files(std::ostream &stream, std::unique_ptr<ArchiveWriter_helper> *&begin){
	bool mt = true;
	boost::iostreams::stream<LzmaOutputFilter> lzma(stream, &mt, 1);

	auto co = (*(begin++))->first();
	for (auto i : *co){
		if (!i.dst)
			continue;
		this->stream_ids.push_back(i.id);
		this->stream_sizes.push_back(i.stream_size);

		sha256_digest &ret = *i.dst;
		{
			boost::iostreams::stream<HashInputFilter> hash(*i.stream, new CryptoPP::SHA256);
			lzma << hash.rdbuf();
			hash->get_result(ret.data(), ret.size());
		}
		this->any_file = true;
	}
}

void ArchiveWriter::add_base_objects(std::ostream &overall_hash, std::unique_ptr<ArchiveWriter_helper> *&begin){
	bool mt = true;
	boost::iostreams::stream<LzmaOutputFilter> lzma(overall_hash, &mt, 8);

	auto co = (*(begin++))->second();
	for (auto i : *co){
		if (!i)
			continue;
		std::uint64_t bytes_processed = 0;
		{
			boost::iostreams::stream<ByteCounterOutputFilter> counter(lzma, &bytes_processed);
			SerializerStream ss(counter);
			ss.begin_serialization(*i, config::include_typehashes);
		}
		this->base_object_entry_sizes.push_back(bytes_processed);
	}
}

void ArchiveWriter::add_version_manifest(std::ostream &overall_hash, std::unique_ptr<ArchiveWriter_helper> *&begin){
	bool mt = true;

	auto co = (*(begin++))->third();
	for (auto i : *co){
		if (!i)
			continue;
		auto &manifest = *i;
		manifest.archive_metadata.entry_sizes = std::move(this->base_object_entry_sizes);
		manifest.archive_metadata.stream_ids = std::move(this->stream_ids);
		manifest.archive_metadata.stream_sizes = std::move(this->stream_sizes);
		manifest.archive_metadata.entries_size_in_archive = this->entries_size_in_archive;

		boost::iostreams::stream<LzmaOutputFilter> lzma(overall_hash, &mt, 8);
		SerializerStream ss(lzma);
		ss.begin_serialization(manifest, config::include_typehashes);
		break;
	}
}
