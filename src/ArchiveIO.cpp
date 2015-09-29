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
		LzmaInputFilter lzma;
		BoundedInputFilter bounded(this->manifest_size);
		boost::iostreams::filtering_istream filter;
		filter.push(lzma);
		filter.push(bounded);
		filter.push(*this->stream);

		DeserializerStream ds(filter);
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
		LzmaInputFilter lzma;
		BoundedInputFilter bounded(this->manifest_offset - this->base_objects_offset);
		boost::iostreams::filtering_istream filter;
		filter.push(lzma);
		filter.push(bounded);
		filter.push(*this->stream);
		for (const auto &s : this->version_manifest->archive_metadata.entry_sizes){
			BoundedInputFilter second_bound(s);
			boost::iostreams::filtering_istream second_filter;
			second_filter.push(second_bound);
			second_filter.push(filter);
			DeserializerStream ds(second_filter);
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
	{
		LzmaInputFilter lzma;
		boost::iostreams::filtering_istream filter;
		filter.push(lzma);
		filter.push(*this->stream);

		assert(this->stream_ids.size() == this->stream_sizes.size());
		for (size_t i = 0; i < this->stream_ids.size(); i++){
			BoundedInputFilter bounded(this->stream_sizes[i]);
			boost::iostreams::filtering_istream second_filter;
			second_filter.push(bounded);
			second_filter.push(filter);
			sink(std::make_pair(this->stream_ids[i], &second_filter));
		}
		DeserializerStream ds(filter);
		this->version_manifest.reset(ds.begin_deserialization<VersionManifest>(config::include_typehashes));
		if (!this->version_manifest)
			throw std::exception("Error during deserialization");
	}
}

ArchiveReader::read_everything_co_t::pull_type ArchiveReader::read_everything(){
	return read_everything_co_t::pull_type([this](read_everything_co_t::push_type &sink){
		this->read_everything(sink);
	});
}

void ArchiveWriter::process(ArchiveWriter_helper *begin, ArchiveWriter_helper *end){
	if (end - begin != 3)
		throw std::exception("Incorrect usage");

	{
		boost::iostreams::filtering_ostream filter;
		bool mt = true;
		LzmaOutputFilter lzma(mt, 1);
		filter.push(lzma);
		filter.push(*this->stream);
		this->filtered_stream = &filter;

		auto co = begin->first();
		for (auto i : *co)
			*i.dst = this->add_file(i.id, *i.stream);
		filter.flush();
	}
	{
		boost::iostreams::filtering_ostream filter;
		bool mt = true;
		LzmaOutputFilter lzma(mt, 9);
		filter.push(lzma);
		filter.push(*this->stream);
		this->filtered_stream = &filter;

		auto co = begin->second();
		for (auto i : *co)
			this->add_fso(*i);
	}
	{
		boost::iostreams::filtering_ostream filter;
		bool mt = true;
		LzmaOutputFilter lzma(mt, 9);
		filter.push(lzma);
		filter.push(*this->stream);
		this->filtered_stream = &filter;

		auto co = begin->third();
		for (auto i : *co){
			this->add_version_manifest(*i);
			break;
		}
	}
}

sha256_digest ArchiveWriter::add_file(stream_id_t id, std::istream &stream){
}

void ArchiveWriter::add_fso(const FileSystemObject &){
}

void ArchiveWriter::add_version_manifest(const VersionManifest &){
}
