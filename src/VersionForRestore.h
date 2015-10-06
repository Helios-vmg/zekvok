/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "SimpleTypes.h"
#include "serialization/fso.generated.h"
#include "ArchiveIO.h"

class BackupSystem;

class VersionForRestore{
	BackupSystem *backup_system;
	version_number_t version_number;
	std::shared_ptr<VersionManifest> manifest;
	path_t path;
	std::unique_ptr<ArchiveReader> archive_reader;
	std::map<version_number_t, std::shared_ptr<VersionForRestore>> dependencies;
	boost::optional<std::vector<std::shared_ptr<FileSystemObject>>> base_objects;
	std::vector<std::shared_ptr<std::vector<std::shared_ptr<BackupStream>>>> streams;
	std::map<stream_id_t, std::shared_ptr<BackupStream>> streams_dict;
	bool dependencies_full;
public:
	VersionForRestore(version_number_t, BackupSystem *);
	DEFINE_INLINE_GETTER(version_number)
	DEFINE_INLINE_GETTER(manifest)
	const std::vector<std::shared_ptr<FileSystemObject>> &get_base_objects();
	std::vector<std::shared_ptr<FileSystemObject>> get_base_objects_without_storage();
	void fill_dependencies(std::map<version_number_t, std::shared_ptr<VersionForRestore>> &);
	void restore(FileSystemObject *fso, std::vector<FileSystemObject *> &restore_later);
};
