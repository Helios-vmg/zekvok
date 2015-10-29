/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

class RsaKeyPair;
class KernelTransaction;
class HashOutputFilter;
class VersionManifest;
class FileSystemObject;
class FilishFso;

enum class KeyIndices{
	FileDataKey = 0,
	FileObjectDataKey,
	//ManifestKey,
	KeyCount,
};

class ArchiveKeys{
public:
	static const size_t key_count = (size_t)KeyIndices::KeyCount;
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
	bool get_key_iv(CryptoPP::SecByteBlock &key, CryptoPP::SecByteBlock &iv, KeyIndices);
public:
	ArchiveReader(const path_t &archive, const path_t *encrypted_fso, RsaKeyPair *keypair);
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

class ArchiveWriter{
	enum class State{
		Initial,
		FilesWritten,
		FsosWritten,
		ManifestWritten,
		Final,
	};
	State state;
	KernelTransaction &tx;
	std::unique_ptr<std::ostream> stream;
	std::vector<stream_id_t> stream_ids;
	std::vector<std::uint64_t> stream_sizes;
	bool any_file;
	std::uint64_t initial_fso_offset;
	std::uint64_t entries_size_in_archive;
	std::vector<std::uint64_t> base_object_entry_sizes;
	RsaKeyPair *keypair;
	std::ostream *nested_stream;
	std::unique_ptr<ArchiveKeys> keys;
	size_t archive_key_index;

	std::unique_ptr<ArchiveKeys> gen_and_save_keys(std::ostream &);
public:
	ArchiveWriter(KernelTransaction &tx, const path_t &, RsaKeyPair *keypair);
	void process(const std::function<void()> &callback);
	struct FileQueueElement{
		FilishFso *fso;
		stream_id_t stream_id;
	};
	void add_files(const std::vector<FileQueueElement> &files);
	void add_base_objects(const std::vector<FileSystemObject *> &base_objects);
	void add_version_manifest(VersionManifest &manifest);
};
