/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "VersionForRestore.h"
#include "BackupSystem.h"
#include "ArchiveIO.h"

VersionForRestore::VersionForRestore(version_number_t version, BackupSystem &bs):
		backup_system(&bs),
		version_number(version),
		dependencies_full(false){
	this->path = bs.get_version_path(version);
	this->archive_reader.reset(new ArchiveReader(this->path));
	this->manifest = this->archive_reader->read_manifest();
}

void VersionForRestore::fill_dependencies(std::map<version_number_t, std::shared_ptr<VersionForRestore>> &map){
	if (this->dependencies_full)
		return;
	for (auto &dep : this->manifest->version_dependencies)
		this->dependencies[dep] = map[dep];
	this->dependencies_full = true;
}

void VersionForRestore::restore(FileSystemObject *fso, std::vector<FileSystemObject *> &restore_later){
	if (!fso->restore() && fso->get_latest_version() >= 0)
		restore_later.push_back(fso);
}

const std::vector<std::shared_ptr<FileSystemObject>> &VersionForRestore::get_base_objects(){
	if (!this->base_objects.is_initialized())
		this->base_objects = this->get_base_objects_without_storage();
	return *this->base_objects;
}

std::vector<std::shared_ptr<FileSystemObject>> VersionForRestore::get_base_objects_without_storage(){
	return this->archive_reader->get_base_objects();
}
