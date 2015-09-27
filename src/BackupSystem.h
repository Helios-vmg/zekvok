/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "SimpleTypes.h"
#include "Utility.h"

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

	void set_versions();
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
