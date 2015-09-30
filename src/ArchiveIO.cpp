/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "ArchiveIO.h"
#include "SymbolicConstants.h"
#include "Utility.h"
#include "LzmaFilter.h"
#include "BoundedStreamFilter.h"
#include "HashFilter.h"

ArchiveReader::ArchiveReader(const path_t &path){
	this->stream.reset(new boost::filesystem::ifstream(path, std::ios::binary));
	if (!*this->stream)
		throw std::exception("File not found.");
	//this->filtered_stream.reset(new boost::iostreams::filtering_istream);
	//this->filters.push_back([](boost::iostreams::filtering_istream &filter){
	//
	//});
}

std::shared_ptr<VersionManifest> ArchiveReader::read_manifest(){
	if (this->manifest_offset < 0){
		const int uint64_length = sizeof(std::uint64_t);
		std::int64_t start = -uint64_length - sha256_digest_length;
		this->stream->seekg(start, std::ios::end);
		char temp[uint64_length];
		this->stream->read(temp, uint64_length);
		if (this->stream->gcount() != uint64_length)
			throw std::exception("Invalid data");
		deserialize_fixed_le_int(this->manifest_size, temp);
		start -= this->manifest_size;
		this->stream->seekg(start, std::ios::end);
		this->manifest_offset = this->stream->tellg();
	}else
		this->stream->seekg(this->manifest_offset);
	{
		boost::iostreams::stream<BoundedInputFilter> bounded(*this->stream, this->manifest_size);
		boost::iostreams::stream<LzmaInputFilter> lzma(bounded);

		DeserializerStream ds(lzma);
		this->version_manifest.reset(ds.begin_deserialization<VersionManifest>(config::include_typehashes));
		if (!this->version_manifest)
			throw std::exception("Error during deserialization");
	}

	this->base_objects_offset = this->version_manifest->archive_metadata.entries_size_in_archive;
	this->stream_ids = this->version_manifest->archive_metadata.stream_ids;
	this->stream_sizes = this->version_manifest->archive_metadata.stream_sizes;

	return this->version_manifest;
}

std::vector<std::shared_ptr<FileSystemObject>> ArchiveReader::read_base_objects(){
	if (!this->version_manifest)
		this->read_manifest();
	assert(this->version_manifest);
	decltype(this->base_objects) ret;
	ret.reserve(this->version_manifest->archive_metadata.entry_sizes.size());
	this->stream->seekg(this->base_objects_offset);
	{
		boost::iostreams::stream<BoundedInputFilter> bounded(*this->stream, this->manifest_offset - this->base_objects_offset);
		boost::iostreams::stream<LzmaInputFilter> lzma(bounded);
		for (const auto &s : this->version_manifest->archive_metadata.entry_sizes){
			boost::iostreams::stream<BoundedInputFilter> bounded2(lzma, s);
			DeserializerStream ds(bounded2);
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
	this->stream->seekg(0);

	boost::iostreams::stream<LzmaInputFilter> lzma(*this->stream);

	assert(this->stream_ids.size() == this->stream_sizes.size());
	for (size_t i = 0; i < this->stream_ids.size(); i++){
		boost::iostreams::stream<BoundedInputFilter> bounded(lzma, this->stream_sizes[i]);
		sink(std::make_pair(this->stream_ids[i], &bounded));
	}
}

ArchiveReader::read_everything_co_t::pull_type ArchiveReader::read_everything(){
	return read_everything_co_t::pull_type([this](read_everything_co_t::push_type &sink){
		this->read_everything(sink);
	});
}

ArchiveWriter::ArchiveWriter(const path_t &path){
	this->stream.reset(new boost::iostreams::stream<TransactedFileSink>(this->tx, path.c_str()));
}

void ArchiveWriter::process(ArchiveWriter_helper *begin, ArchiveWriter_helper *end){
	if (end - begin != 3)
		throw std::exception("Incorrect usage");

	sha256_digest complete_hash;
	{
		hash_stream_t overall_hash(*this->stream, new CryptoPP::SHA256);
		this->add_files(overall_hash, begin);
		this->initial_fso_offset = overall_hash->get_bytes_processed();
		this->add_base_objects(overall_hash, begin);
		this->add_version_manifest(overall_hash, begin);
		overall_hash->get_result(complete_hash.data(), complete_hash.size());
	}
	this->stream->write((const char *)complete_hash.data(), complete_hash.size());
}

void ArchiveWriter::add_files(hash_stream_t &overall_hash, ArchiveWriter_helper *&begin){
	bool mt = true;
	boost::iostreams::stream<LzmaOutputFilter> lzma(overall_hash, &mt, 1);

	auto co = (begin++)->first();
	for (auto i : *co){
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
	lzma.flush();
}

void ArchiveWriter::add_base_objects(hash_stream_t &overall_hash, ArchiveWriter_helper *&begin){
	bool mt = true;
	boost::iostreams::stream<LzmaOutputFilter> lzma(overall_hash, &mt, 8);

	auto co = (begin++)->second();
	for (auto i : *co){
		auto x0 = lzma->get_bytes_written();
		SerializerStream ss(lzma);
		ss.begin_serialization(*i, config::include_typehashes);
		auto x1 = lzma->get_bytes_written();
		this->base_object_entry_sizes.push_back(x1 - x0);
	}
	lzma.flush();
}

void ArchiveWriter::add_version_manifest(hash_stream_t &overall_hash, ArchiveWriter_helper *&begin){
	bool mt = true;
	boost::iostreams::stream<LzmaOutputFilter> lzma(overall_hash, &mt, 8);

	auto co = (begin++)->third();
	for (auto i : *co){
		auto &manifest = *i;
		manifest.archive_metadata.entry_sizes = this->base_object_entry_sizes;
		manifest.archive_metadata.stream_ids = this->stream_ids;
		manifest.archive_metadata.entries_size_in_archive = overall_hash->get_bytes_processed() - this->initial_fso_offset;
		auto x0 = overall_hash->get_bytes_processed();
		{
			SerializerStream ss(lzma);
			ss.begin_serialization(manifest);
		}
		lzma.flush();
		auto x1 = overall_hash->get_bytes_processed();
		std::uint64_t manifest_length = x1 - x0;
		auto s_manifest_length = serialize_fixed_le_int(manifest_length);
		overall_hash.write((const char *)s_manifest_length.data(), s_manifest_length.size());
		break;
	}
	lzma.flush();
}
