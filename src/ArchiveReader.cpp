/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "ArchiveReader.h"
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
		std::int64_t start = -uint64_length - md5_digest_length;
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

std::vector<std::shared_ptr<FileSystemObject>> ArchiveReader::get_base_objects(){
	return std::vector<std::shared_ptr<FileSystemObject>>();
}

void ArchiveReader::begin(std::function<void(std::uint64_t, std::istream &)> &){
}
