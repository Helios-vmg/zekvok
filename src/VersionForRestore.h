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
	std::unique_ptr<VersionManifest> manifest;
	path_t path;
	ArchiveReader archive_reader;
	std::map<version_number_t, std::shared_ptr<VersionForRestore>> dependencies;
	std::vector<std::shared_ptr<FileSystemObject>> base_objects;
	std::vector<std::shared_ptr<std::vector<std::shared_ptr<BackupStream>>>> streams;
	std::map<stream_id_t, std::shared_ptr<BackupStream>> streams_dict;
public:
	VersionForRestore(version_number_t, BackupSystem *);
	DEFINE_INLINE_GETTER(version_number)
	DEFINE_INLINE_GETTER(base_objects)
	DEFINE_INLINE_GETTER(manifest)
	void fill_dependencies(std::map<version_number_t, std::shared_ptr<VersionForRestore>> &);
};
