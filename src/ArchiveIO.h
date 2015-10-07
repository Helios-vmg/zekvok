/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "Archive.h"
#include "SimpleTypes.h"
#include "serialization/fso.generated.h"
#include "System/Transactions.h"
#include "HashFilter.h"
#include "Exception.h"

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

class ArchiveWriter_helper{
public:
	struct first_push_t{
		sha256_digest *dst;
		stream_id_t id;
		std::istream *stream;
		std::uint64_t stream_size;
	};
	typedef const FileSystemObject *second_push_t;
	typedef VersionManifest *third_push_t;
	typedef boost::coroutines::asymmetric_coroutine<first_push_t> first_co_t;
	typedef boost::coroutines::asymmetric_coroutine<second_push_t> second_co_t;
	typedef boost::coroutines::asymmetric_coroutine<third_push_t> third_co_t;

	virtual ~ArchiveWriter_helper(){}

	virtual std::shared_ptr<first_co_t::pull_type> first(){
		throw IncorrectImplementationException();
	}
	virtual std::shared_ptr<second_co_t::pull_type> second(){
		throw IncorrectImplementationException();
	}
	virtual std::shared_ptr<third_co_t::pull_type> third(){
		throw IncorrectImplementationException();
	}
};

#define DERIVE_ArchiveWriter_helper(x)                        \
class ArchiveWriter_helper_##x : public ArchiveWriter_helper{ \
	typedef std::shared_ptr<x##_co_t::pull_type> r;           \
	r data;                                                   \
public:                                                       \
	ArchiveWriter_helper_##x(const r &data): data(data){}     \
	r x() override{                                           \
		return this->data;                                    \
	}                                                         \
}

DERIVE_ArchiveWriter_helper(first);
DERIVE_ArchiveWriter_helper(second);
DERIVE_ArchiveWriter_helper(third);

class ArchiveWriter{
	KernelTransaction tx;
	std::unique_ptr<std::ostream> stream;
	std::vector<stream_id_t> stream_ids;
	std::vector<std::uint64_t> stream_sizes;
	bool any_file;
	std::uint64_t initial_fso_offset;
	std::vector<std::uint64_t> base_object_entry_sizes;

	typedef boost::iostreams::stream<HashOutputFilter> hash_stream_t;

	void add_files(std::ostream &overall_hash, std::unique_ptr<ArchiveWriter_helper> *&begin);
	void add_base_objects(hash_stream_t &overall_hash, std::unique_ptr<ArchiveWriter_helper> *&begin);
	void add_version_manifest(hash_stream_t &overall_hash, std::unique_ptr<ArchiveWriter_helper> *&begin);
public:
	ArchiveWriter(const path_t &);
	void process(std::unique_ptr<ArchiveWriter_helper> *begin, std::unique_ptr<ArchiveWriter_helper> *end);
};
