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

	boost::iostreams::stream<HashOutputFilter> overall_hash(*this->stream, new CryptoPP::SHA256);
	{
		bool mt = true;
		boost::iostreams::stream<LzmaOutputFilter> lzma(overall_hash, &mt, 1);
		this->filtered_stream = &lzma;

		auto co = begin->first();
		for (auto i : *co)
			*i.dst = this->add_file(i.id, *i.stream, i.stream_size);
		this->filtered_stream->flush();
	}
	this->initial_fso_offset = overall_hash->get_bytes_processed();
	{
		bool mt = true;
		boost::iostreams::stream<LzmaOutputFilter> lzma(overall_hash, &mt, 8);
		this->filtered_stream = &lzma;

		auto co = begin->second();
		for (auto i : *co)
			this->add_fso(*i);
		this->filtered_stream->flush();
	}
	{
		bool mt = true;
		boost::iostreams::stream<LzmaOutputFilter> lzma(overall_hash, &mt, 8);
		this->filtered_stream = &lzma;

		auto co = begin->third();
		for (auto i : *co){
			this->add_version_manifest(*i);
			break;
		}
		this->filtered_stream->flush();
	}
}

sha256_digest ArchiveWriter::add_file(stream_id_t id, std::istream &stream, std::uint64_t stream_size){
	this->stream_ids.push_back(id);
	this->stream_sizes.push_back(stream_size);

	sha256_digest ret;
	{
		boost::iostreams::stream<HashInputFilter> hash(stream, new CryptoPP::SHA256);
		*this->filtered_stream << hash.rdbuf();
		hash->get_result(ret.data(), ret.size());
	}
	this->any_file = true;
	return ret;
}

void ArchiveWriter::add_fso(const FileSystemObject &fso){

}

void ArchiveWriter::add_version_manifest(const VersionManifest &){
}
