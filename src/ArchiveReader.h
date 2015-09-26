#pragma once

#include "Archive.h"
#include "SimpleTypes.h"
#include "serialization/fso.generated.h"

typedef std::function<void(boost::iostreams::filtering_istream &)> input_filter_generator_t;

class ArchiveReader{
	std::deque<input_filter_generator_t> filters;
	std::unique_ptr<std::istream> stream;
	std::shared_ptr<VersionManifest> version_manifest;
	std::vector<std::shared_ptr<FileSystemObject>> base_objects;
	std::int64_t manifest_offset,
		manifest_size,
		base_objects_offset;

public:
	ArchiveReader(const path_t &);
	std::shared_ptr<VersionManifest> read_manifest();
	std::vector<std::shared_ptr<FileSystemObject>> read_base_objects();
	std::vector<std::shared_ptr<FileSystemObject>> get_base_objects();
	void begin(std::function<void(std::uint64_t, std::istream &)> &);
};

