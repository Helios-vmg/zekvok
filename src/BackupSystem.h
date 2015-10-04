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
#include "VersionForRestore.h"

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
	typedef std::vector<std::pair<boost::wregex, std::wstring>> path_mapper_t;
	path_mapper_t path_mapper,
		reverse_path_mapper;
	bool base_objects_set;
	std::vector<std::shared_ptr<FileSystemObject>> old_objects;
	std::map<std::wstring, FileSystemObject *, strcmpci> old_objects_map;
	std::vector<std::shared_ptr<FileSystemObject>> base_objects;
	stream_id_t next_stream_id;
	stream_id_t next_differential_chain_id;
	std::deque<FilishFso *> recalculate_file_guids_queue;
	std::vector<std::shared_ptr<BackupStream>> streams;

	void set_versions();
	void perform_backup_inner(const OpaqueTimestamp &start_time);
	void set_path_mapper(const VssSnapshot &);
	void create_initial_version(const OpaqueTimestamp &start_time);
	void create_new_version(const OpaqueTimestamp &start_time);
	void set_base_objects();
	void generate_first_archive(const OpaqueTimestamp &start_time);
	typedef std::map<guid_t, std::shared_ptr<BackupStream>> known_guids_t;
	typedef std::shared_ptr<BackupStream> (BackupSystem::*generate_archive_fp)(FileSystemObject &, known_guids_t &);
	void generate_archive(const OpaqueTimestamp &start_time, generate_archive_fp, version_number_t = 0);
	std::function<BackupMode(const FileSystemObject &)> make_map(const std::shared_ptr<std::vector<std::wstring>> &for_later_check);
	BackupMode get_backup_mode_for_object(const FileSystemObject &, bool &follow_link_targets);
	std::vector<path_t> get_current_source_locations();
	path_t map_forward(const path_t &);
	path_t map_back(const path_t &);
	path_t map_path(const path_t &, const path_mapper_t &);
	bool covered(const path_t &);
	void recalculate_file_guids();
	std::shared_ptr<BackupStream> generate_initial_stream(FileSystemObject &, known_guids_t &);
	typedef std::map<stream_index_t, std::vector<std::shared_ptr<BackupStream>>> stream_dict_t;
	stream_dict_t generate_streams(generate_archive_fp);
	void get_dependencies(std::set<version_number_t> &, FileSystemObject &) const;
	bool should_be_added(FileSystemObject &, known_guids_t &);
	static void fix_up_stream_reference(FileSystemObject &, known_guids_t &);
	version_number_t get_new_version_number(){
		return this->get_version_count();
	}
	void set_old_objects_map();
	std::shared_ptr<BackupStream> check_and_maybe_add(FileSystemObject &, known_guids_t &);
	bool file_has_changed(version_number_t &, FilishFso &);
	bool file_has_changed(const FileSystemObject &, const FileSystemObject &);
	ChangeCriterium get_change_criterium(const FileSystemObject &);
	std::shared_ptr<VersionForRestore> compute_latest_version();
	void perform_restore(const std::shared_ptr<VersionForRestore> &, const std::vector<FileSystemObject *> &);
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
	stream_id_t get_stream_id();
	void enqueue_file_for_guid_get(FilishFso *);
	path_t get_version_path(version_number_t) const;
	std::vector<std::shared_ptr<FileSystemObject>> get_entries(version_number_t);
};
