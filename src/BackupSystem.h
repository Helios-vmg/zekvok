#pragma once

#include "SimpleTypes.h"

enum class ChangeCriterium{
	ArchiveFlag = 0,
	Size,
	Date,
	Hash,
	HashAuto,
};

enum class NameIgnoreType{
	File = 0,
	Directory,
	All,
};

class BackupSystem{
public:
	BackupSystem(const std::wstring &);
	version_number_t get_version_count() const;
	void add_source(const std::wstring &);
	void perform_backup();
	void restore_backup();
	void add_ignored_extension(const std::wstring &);
	void add_ignored_path(const std::wstring &);
	void add_ignored_name(const std::wstring &, NameIgnoreType);
	bool version_exists(version_number_t) const;
	std::vector<version_number_t> get_version_dependencies(version_number_t) const;
	std::vector<version_number_t> get_versions() const;
	void set_use_snapshots(bool);
	void set_change_criterium(ChangeCriterium);
};
