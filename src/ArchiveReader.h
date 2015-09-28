/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Archive.h"
#include "SimpleTypes.h"
#include "serialization/fso.generated.h"

typedef std::function<void(boost::iostreams::filtering_istream &)> input_filter_generator_t;

class ArchiveReader{
public:
	typedef boost::coroutines::asymmetric_coroutine<std::pair<std::uint64_t, std::istream *>> read_everything_co_t;
private:
	//std::deque<input_filter_generator_t> filters;
	std::unique_ptr<std::istream> stream;
	std::shared_ptr<VersionManifest> version_manifest;
	std::vector<std::shared_ptr<FileSystemObject>> base_objects;
	std::int64_t manifest_offset;
	std::uint64_t manifest_size,
		base_objects_offset;
	std::vector<stream_id_t> stream_ids;
	std::vector<std::uint64_t> stream_sizes;

	void read_everything(read_everything_co_t::push_type &);
public:
	ArchiveReader(const path_t &);
	std::shared_ptr<VersionManifest> read_manifest();
	std::vector<std::shared_ptr<FileSystemObject>> read_base_objects();
	std::vector<std::shared_ptr<FileSystemObject>> get_base_objects(){
		return std::move(this->read_base_objects());
	}
	read_everything_co_t::pull_type read_everything();
};

