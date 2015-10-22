/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

typedef std::function<void(boost::iostreams::filtering_istream &)> input_filter_generator_t;

class ArchiveKeys{
public:
	static const size_t key_count = 3;
private:
	size_t size;
	CryptoPP::SecByteBlock keys[key_count];
	CryptoPP::SecByteBlock ivs[key_count];
	void init(size_t key_size, size_t iv_size, bool = true);
public:
	ArchiveKeys(std::istream &, RsaKeyPair &keypair);
	ArchiveKeys(size_t key_size, size_t iv_size);
	const CryptoPP::SecByteBlock &get_key(size_t) const;
	const CryptoPP::SecByteBlock &get_iv(size_t) const;
	size_t get_size() const{
		return this->size;
	}
	static std::unique_ptr<ArchiveKeys> create_and_save(std::ostream &, RsaKeyPair *keypair);
};

class ArchiveReader{
public:
	typedef boost::coroutines::asymmetric_coroutine<std::pair<std::uint64_t, std::istream *>> read_everything_co_t;
private:
	//std::deque<input_filter_generator_t> filters;
	path_t path;
	std::shared_ptr<VersionManifest> version_manifest;
	std::vector<std::shared_ptr<FileSystemObject>> base_objects;
	std::int64_t manifest_offset;
	std::uint64_t manifest_size,
		base_objects_offset;
	std::vector<stream_id_t> stream_ids;
	std::vector<std::uint64_t> stream_sizes;
	RsaKeyPair *keypair;
	std::unique_ptr<ArchiveKeys> archive_keys;

	void read_everything(read_everything_co_t::push_type &);
	std::unique_ptr<std::istream> get_stream();
public:
	ArchiveReader(const path_t &, RsaKeyPair *keypair);
	std::shared_ptr<VersionManifest> read_manifest();
	std::vector<std::shared_ptr<FileSystemObject>> read_base_objects();
	std::vector<std::shared_ptr<FileSystemObject>> get_base_objects(){
		return std::move(this->read_base_objects());
	}
	read_everything_co_t::pull_type read_everything();
	std::uint64_t get_file_data_size() const{
		return this->base_objects_offset;
	}
	std::uint64_t get_base_objects_size() const{
		return this->manifest_offset - this->base_objects_offset;
	}
	std::uint64_t get_manifest_size() const{
		return this->manifest_size;
	}
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
	std::uint64_t entries_size_in_archive;
	std::vector<std::uint64_t> base_object_entry_sizes;
	RsaKeyPair *keypair;

	typedef boost::iostreams::stream<HashOutputFilter> hash_stream_t;

	void add_files(std::ostream &overall_hash, std::unique_ptr<ArchiveWriter_helper> *&begin);
	void add_base_objects(std::ostream &overall_hash, std::unique_ptr<ArchiveWriter_helper> *&begin);
	void add_version_manifest(std::ostream &overall_hash, std::unique_ptr<ArchiveWriter_helper> *&begin);
	std::unique_ptr<ArchiveKeys> gen_and_save_keys(std::ostream &);
public:
	ArchiveWriter(const path_t &, RsaKeyPair *keypair);
	void process(std::unique_ptr<ArchiveWriter_helper> *begin, std::unique_ptr<ArchiveWriter_helper> *end);
};
