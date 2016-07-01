/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "ArchiveIO.h"
#include "CryptoFilter.h"
#include "serialization/fso.generated.h"
#include "BoundedStreamFilter.h"
#include "LzmaFilter.h"
#include "serialization/ImplementedDS.h"
#include "NullStream.h"
#include "System/Transactions.h"
#include "HashFilter.h"
#include "StreamProcessor.h"

const Algorithm default_crypto_algorithm = Algorithm::Twofish;
using zstreams::Stream;

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
	{
		CryptoPP::ArraySource source(static_cast<const byte *>(&private_key[0]), private_key.size(), true);
		priv.Load(source);
	}

	CryptoPP::SecByteBlock buffer(this->size);
	{
		auto n = 4096 / 8;

		typedef CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA>>::Decryptor decryptor_t;
		typedef CryptoPP::PK_DecryptorFilter filter_t;
		decryptor_t dec(priv);
		CryptoPP::SecByteBlock temp(n);
		stream.read(reinterpret_cast<char *>(temp.data()), temp.size());
		auto read = stream.gcount();
		if (read != temp.size())
			throw RsaBlockDecryptionException("Not enough bytes read from RSA block.");
		try{
			auto sink = new CryptoPP::ArraySink(buffer.data(), buffer.size());
			auto filter = new filter_t(*random_number_generator, dec, sink);
			CryptoPP::ArraySource(temp.data(), temp.size(), true, filter);
		}catch (...){
			throw RsaBlockDecryptionException("Invalid data in RSA block.");
		}
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

std::unique_ptr<ArchiveKeys> ArchiveKeys::create_and_save(zstreams::Sink &stream, RsaKeyPair *keypair){
	std::unique_ptr<ArchiveKeys> ret;
	if (keypair){
		ret = make_unique(new ArchiveKeys(CryptoPP::Twofish::MAX_KEYLENGTH, CryptoPP::Twofish::BLOCKSIZE));

		CryptoPP::RSA::PublicKey pub;
		auto &public_key = keypair->get_public_key();
		{
			CryptoPP::ArraySource source((const byte *)&public_key[0], public_key.size(), true);
			pub.Load(source);
		}

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
		boost::iostreams::stream<zstreams::SynchronousSink> sink(stream);
		CryptoPP::ArraySource(buffer.data(), buffer.size(), true, new filter_t(*random_number_generator, enc, new CryptoPP::FileSink(sink)));
	}
	return ret;
}

ArchiveReader::ArchiveReader(const path_t &path, const path_t *encrypted_fso, RsaKeyPair *keypair):
		path(path),
		manifest_offset(-1),
		keypair(keypair),
		manifest_size(0){
	if (!boost::filesystem::exists(path) || !boost::filesystem::is_regular_file(path))
		throw FileNotFoundException(path);
}

bool ArchiveReader::get_key_iv(CryptoPP::SecByteBlock &key, CryptoPP::SecByteBlock &iv, KeyIndices index){
	if (!this->keypair)
		return false;
	if (!this->archive_keys){
		auto stream = this->get_stream();
		this->archive_keys.reset(new ArchiveKeys(*stream, *this->keypair));
	}
	key = this->archive_keys->get_key((size_t)index);
	iv = this->archive_keys->get_iv((size_t)index);
	return true;
}

std::unique_ptr<std::istream> ArchiveReader::get_stream(){
	auto ret = make_unique(static_cast<std::istream *>(new boost::filesystem::ifstream(path, std::ios::binary)));
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
			throw ArchiveReadException("Invalid data: File is too small to possibly be valid");
		deserialize_fixed_le_int(this->manifest_size, temp);
		start -= this->manifest_size;
		stream->seekg(start, std::ios::end);
		this->manifest_offset = stream->tellg();
	}else
		stream->seekg(this->manifest_offset);
	{
		zstreams::StreamPipeline pipeline;
		Stream<zstreams::StdStreamSource> source(stream, pipeline);
		Stream<zstreams::BoundedSource> bounded(*source, this->manifest_size);
		Stream<zstreams::LzmaSource> lzma(*bounded);
		boost::iostreams::stream<zstreams::SynchronousSource> sync_source(*lzma);

		ImplementedDeserializerStream ds(sync_source);
		this->version_manifest.reset(ds.full_deserialization<VersionManifest>(config::include_typehashes));
		if (!this->version_manifest)
			throw ArchiveReadException("Invalid data: Error during manifest deserialization");
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
		zstreams::StreamPipeline pipeline;
		Stream<zstreams::StdStreamSource> source(stream, pipeline);
		Stream<zstreams::BoundedSource> bounded(*source, this->manifest_offset - this->base_objects_offset);
		zstreams::Source *stream2 = &*bounded;
		Stream<zstreams::CryptoSource> crypto;
		if (this->keypair){
			CryptoPP::SecByteBlock key, iv;
			zekvok_assert(this->get_key_iv(key, iv, KeyIndices::FileObjectDataKey));
			crypto = zstreams::CryptoSource::create(default_crypto_algorithm, *stream2, &key, &iv);
			stream2 = &*crypto;
		}
		Stream<zstreams::LzmaSource> lzma(*stream2);

		for (const auto &s : this->version_manifest->archive_metadata.entry_sizes){
			Stream<zstreams::BoundedSource> bounded2(*lzma, s);
			boost::iostreams::stream<zstreams::SynchronousSource> sync_source(*bounded2);
			ImplementedDeserializerStream ds(sync_source);
			std::shared_ptr<FileSystemObject> fso(ds.full_deserialization<FileSystemObject>(config::include_typehashes));
			if (!fso)
				throw ArchiveReadException("Invalid data: Error during FSO deserialization");
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

	zstreams::StreamPipeline pipeline;
	Stream<zstreams::StdStreamSource> source(ptr, pipeline);
	Stream<zstreams::BoundedSource> bounded(*source, this->base_objects_offset - file_data_start);
	zstreams::Source *stream = &*bounded;

	Stream<zstreams::CryptoSource> crypto;
	if (this->keypair){
		CryptoPP::SecByteBlock key, iv;
		zekvok_assert(this->get_key_iv(key, iv, KeyIndices::FileDataKey));
		crypto = zstreams::CryptoSource::create(default_crypto_algorithm, *stream, &key, &iv);
		stream = &*crypto;
	}
	Stream<zstreams::LzmaSource> lzma(*stream);

	zekvok_assert(this->stream_ids.size() == this->stream_sizes.size());
	for (size_t i = 0; i < this->stream_ids.size(); i++){
		Stream<zstreams::BoundedSource> bounded2(*lzma, this->stream_sizes[i]);
		sink(std::make_pair(this->stream_ids[i], &*bounded2));
		//Discard left over bytes.
		Stream<zstreams::NullSink> null(pipeline);
		bounded2->copy_to(*null);
	}
}

ArchiveReader::read_everything_co_t::pull_type ArchiveReader::read_everything(){
	return read_everything_co_t::pull_type([this](read_everything_co_t::push_type &sink){
		this->read_everything(sink);
	});
}

ArchiveWriter::ArchiveWriter(KernelTransaction &tx, const path_t &path, RsaKeyPair *keypair):
		state(State::Initial),
		tx(tx),
		keypair(keypair),
		any_file(false){
	std::unique_ptr<std::ostream> ptr(new boost::iostreams::stream<TransactedFileSink>(this->tx, path.c_str()));
	this->stream = Stream<zstreams::StdStreamSink>(ptr, this->pipeline);
}

void ArchiveWriter::process(const std::function<void()> &callback){
	std::shared_ptr<zstreams::HashSink<CryptoPP::SHA256>::digest_t> complete_hash;
	{
		Stream<zstreams::HashSink<CryptoPP::SHA256>> overall_hash(*this->stream);
		this->nested_stream = &*overall_hash;
		this->keys = ArchiveKeys::create_and_save(*overall_hash, this->keypair);
		this->archive_key_index = 0;
		this->initial_fso_offset = 0;

		callback();
		zekvok_assert(this->state == State::ManifestWritten);
		this->state = State::Final;

		complete_hash = overall_hash->get_digest();
	}

	boost::iostreams::stream<zstreams::SynchronousSink> sync_sink(*this->stream);
	sync_sink.write(reinterpret_cast<const char *>(complete_hash->data()), complete_hash->size());
}

void ArchiveWriter::add_files(const std::vector<FileQueueElement> &files){
	zekvok_assert(this->state == State::Initial);
	this->state = State::FilesWritten;
	Stream<zstreams::ByteCounterSink> counter(*this->nested_stream, this->initial_fso_offset);
	zstreams::Sink *stream = &*counter;
	Stream<zstreams::CryptoSink> crypto;
	if (this->keypair){
		crypto = zstreams::CryptoSink::create(default_crypto_algorithm, *stream, &this->keys->get_key(this->archive_key_index), &this->keys->get_iv(this->archive_key_index));
		this->archive_key_index++;
		stream = &*crypto;
	}
	
	bool mt = true;
	Stream<zstreams::LzmaSink> lzma(*stream, &mt, 1);

	for (auto &fqe : files){
		std::uint64_t size;
		auto fso = fqe.fso;
		std::wcout << fso->get_unmapped_path() << std::endl;
		auto stream2 = fso->open_for_exclusive_read(size);

		this->stream_ids.push_back(fqe.stream_id);
		this->stream_sizes.push_back(size);

		std::shared_ptr<zstreams::HashSink<CryptoPP::SHA256>::digest_t> digest;
		{
			auto &pipeline = lzma->get_pipeline();
			Stream<zstreams::StdStreamSource> stdstream(stream2, pipeline);
			Stream<zstreams::HashSource<CryptoPP::SHA256>> hash(*stdstream);
			if (size)
				hash->copy_to(*lzma);
			digest = hash->get_digest();
		}
		fso->set_hash(*digest);
		this->any_file = true;
	}
}

void ArchiveWriter::add_base_objects(const std::vector<FileSystemObject *> &base_objects){
	zekvok_assert(this->state == State::FilesWritten);
	this->state = State::FsosWritten;
	this->entries_size_in_archive = 0;

	Stream<zstreams::ByteCounterSink> counter(*this->nested_stream, this->entries_size_in_archive);
	zstreams::Sink *stream = &*counter;
	Stream<zstreams::CryptoSink> crypto;
	if (this->keypair){
		crypto = zstreams::CryptoSink::create(default_crypto_algorithm, *stream, &keys->get_key(this->archive_key_index), &keys->get_iv(this->archive_key_index));
		this->archive_key_index++;
		stream = &*crypto;
	}
		
	bool mt = true;
	Stream<zstreams::LzmaSink> lzma(*stream, &mt, 8);

	for (auto i : base_objects){
		zstreams::streamsize_t bytes_processed = 0;
		{
			Stream<zstreams::ByteCounterSink> counter2(*lzma, bytes_processed);
			boost::iostreams::stream<zstreams::SynchronousSink> sync_sink(*counter2);
			SerializerStream ss(sync_sink);
			ss.full_serialization(*i, config::include_typehashes);
		}
		this->base_object_entry_sizes.push_back(bytes_processed);
	}
}

void ArchiveWriter::add_version_manifest(VersionManifest &manifest){
	zekvok_assert(this->state == State::FsosWritten);
	this->state = State::ManifestWritten;
	zstreams::streamsize_t manifest_length = 0;
	{
		Stream<zstreams::ByteCounterSink> counter(*this->nested_stream, manifest_length);
		
		bool mt = true;

		manifest.archive_metadata.entry_sizes = std::move(this->base_object_entry_sizes);
		manifest.archive_metadata.stream_ids = std::move(this->stream_ids);
		manifest.archive_metadata.stream_sizes = std::move(this->stream_sizes);
		manifest.archive_metadata.entries_size_in_archive = this->entries_size_in_archive;

		Stream<zstreams::LzmaSink> lzma(*counter, &mt, 8);
		boost::iostreams::stream<zstreams::SynchronousSink> sync_sink(*lzma);
		SerializerStream ss(sync_sink);
		ss.full_serialization(manifest, config::include_typehashes);
	}
	auto s_manifest_length = serialize_fixed_le_int(manifest_length);
	boost::iostreams::stream<zstreams::SynchronousSink> sync_sink(*this->nested_stream);
	sync_sink.write(reinterpret_cast<const char *>(s_manifest_length.data()), s_manifest_length.size());
}
