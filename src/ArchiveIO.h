/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "StreamProcessor.h"
#include "BoundedStreamFilter.h"

class RsaKeyPair;
class KernelTransaction;
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
	static const size_t key_count = static_cast<size_t>(KeyIndices::KeyCount);
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
	static std::unique_ptr<ArchiveKeys> create_and_save(zstreams::Sink &, RsaKeyPair *keypair);
};

class ArchiveReader{
public:
	class ArchivePart{
		std::uint64_t stream_id,
			skip_bytes,
			stream_length;
		zstreams::Source *source;
		bool was_skipped;
		zstreams::Stream<zstreams::Source> created_source;
	public:
		ArchivePart(std::uint64_t stream_id, zstreams::Source *source, std::uint64_t stream_length, std::uint64_t skip_bytes){
			this->stream_id = stream_id;
			this->skip_bytes = skip_bytes;
			this->stream_length = stream_length;
			this->source = source;
			this->was_skipped = false;
		}
		ArchivePart(const ArchivePart &old) = delete;
		const ArchivePart &operator=(const ArchivePart &old) = delete;
		ArchivePart(const ArchivePart &&old) = delete;
		const ArchivePart &operator=(const ArchivePart &&old) = delete;
		void skip(){
			this->was_skipped = true;
		}
		void check_skip(std::uint64_t &skipped){
			if (this->was_skipped || !this->created_source){
				skipped += this->stream_length;
				return;
			}

			std::uint64_t written = 0;
			this->created_source->set_bytes_written_dst(written);
			this->created_source = zstreams::Stream<zstreams::Source>();
			skipped = this->stream_length - written;
		}
		zstreams::Source *read(){
			if (this->skip_bytes){
				zstreams::Stream<zstreams::BoundedSource> temp(*this->source, this->skip_bytes);
				temp->discard_rest();
				this->skip_bytes = 0;
			}
			this->created_source = zstreams::Stream<zstreams::BoundedSource>(*this->source, this->stream_length);
			return &*this->created_source;
		}
		std::uint64_t get_stream_id() const{
			return this->stream_id;
		}
	};
	typedef boost::coroutines::asymmetric_coroutine<ArchivePart *> read_everything_co_t;
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
	zstreams::StreamPipeline pipeline;
	zstreams::Stream<zstreams::StdStreamSink> stream;
	std::vector<stream_id_t> stream_ids;
	std::vector<std::uint64_t> stream_sizes;
	bool any_file;
	zstreams::streamsize_t initial_fso_offset;
	std::uint64_t entries_size_in_archive;
	std::vector<std::uint64_t> base_object_entry_sizes;
	RsaKeyPair *keypair;
	zstreams::Sink *nested_stream;
	std::unique_ptr<ArchiveKeys> keys;
	size_t archive_key_index;

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
