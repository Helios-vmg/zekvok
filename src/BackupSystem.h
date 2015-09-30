/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "SimpleTypes.h"
#include "Utility.h"
#include "System/SystemOperations.h"
#include "serialization/fso.generated.h"

class VssSnapshot;

class BackupSystem{
	version_number_t version_count;
	path_t target_path;
	std::vector<version_number_t> versions;
	std::vector<path_t> sources;
	std::set<std::wstring, strcmpci> ignored_extensions;
	std::set<std::wstring, strcmpci> ignored_paths;
	std::map<std::wstring, NameIgnoreType, strcmpci> ignored_names;
	bool use_snapshots;
	ChangeCriterium change_criterium;
	std::map<std::wstring, system_ops::VolumeInfo> current_volumes;
	std::vector<std::pair<boost::wregex, std::wstring>> path_mapper;
	std::vector<std::pair<boost::wregex, std::wstring>> reverse_path_mapper;
	bool base_objects_set;
	std::vector<std::shared_ptr<FileSystemObject>> old_objects;
	std::vector<std::shared_ptr<FileSystemObject>> base_objects;

	void set_versions();
	path_t get_version_path(version_number_t) const;
	void perform_backup_inner(const OpaqueTimestamp &start_time);
	void set_path_mapper(const VssSnapshot &);
	void create_initial_version(const OpaqueTimestamp &start_time);
	void create_new_version(const OpaqueTimestamp &start_time);
	void set_base_objects();
	void generate_first_archive(const OpaqueTimestamp &start_time);
	std::function<BackupMode(const FileSystemObject &)> make_map(const std::shared_ptr<std::vector<std::wstring>> &for_later_check);
	BackupMode get_backup_mode_for_object(const FileSystemObject &, bool &follow_link_targets);
	std::vector<path_t> get_current_source_locations();
	path_t map_forward(const path_t &);
	path_t map_back(const path_t &);
	bool covered(const path_t &);
	void recalculate_file_guids();
public:
	BackupSystem(const std::wstring &);
	version_number_t get_version_count();
	void add_source(const std::wstring &);
	void perform_backup();
	void restore_backup();
	void add_ignored_extension(const std::wstring &);
	void add_ignored_path(const std::wstring &);
	void add_ignored_name(const std::wstring &, NameIgnoreType);
	bool version_exists(version_number_t) const;
	std::vector<version_number_t> get_version_dependencies(version_number_t) const;
	const std::vector<version_number_t> &get_versions() const{
		return this->versions;
	}
	void set_use_snapshots(bool);
	void set_change_criterium(ChangeCriterium);
};
