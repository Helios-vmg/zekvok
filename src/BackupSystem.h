#pragma once

#include "SimpleTypes.h"

class BackupSystem{
public:
	BackupSystem(const std::wstring &);
	version_number_t get_version_count() const;
	void add_source(const std::wstring &);
	void perform_backup();
	void restore_backup();
};
