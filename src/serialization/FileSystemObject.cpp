/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "fso.generated.h"
#include "../Exception.h"
#include "../Utility.h"
#include "../BackupSystem.h"

path_t FileSystemObject::get_mapped_path() const{
	return this->path_override_base();
}

path_t FileSystemObject::get_unmapped_path() const{
	return this->path_override_base(this->get_unmapped_base_path().get(), BasePathType::Unmapped);
}

path_t FileSystemObject::get_path_without_base() const{
	return this->path_override_base(nullptr, BasePathType::Override);
}

path_t FileSystemObject::path_override_base(const std::wstring *base_path_override, BasePathType override) const{
	auto _this = this;
	std::vector<std::wstring> path_vector;
	size_t expected_length = 0;
	while (1){
		auto &name = _this->get_name();
		path_vector.push_back(name);
		expected_length += name.size() + 1;
		if (!_this->get_parent())
			break;
		_this = _this->get_parent();
	}
	const std::wstring *s;
	switch (override){
		case BasePathType::Mapped:
			s = _this->get_mapped_base_path().get();
			break;
		case BasePathType::Unmapped:
			s = _this->get_unmapped_base_path().get();
			break;
		case BasePathType::Override:
			s = base_path_override;
			break;
		default:
			throw InvalidSwitchVariableException();
	}
	if (s){
		path_vector.push_back(*s);
		expected_length += s->size() + 1;
	}

	path_t ret;
	for (auto i = path_vector.rbegin(), e = path_vector.rend(); i != e; ++i)
		ret /= *i;
	return ret;
}

bool pathcmp(const path_t &a, const path_t &b){
	return strcmpci().equal(a.wstring(), b.wstring());
}

bool path_contains_path(path_t dir, path_t file)
{
	dir = dir.normalize();
	file = file.normalize();
	if (dir.filename() == ".")
		dir.remove_filename();
	assert(file.has_filename());
	file.remove_filename();
	auto dir_len = std::distance(dir.begin(), dir.end());
	auto file_len = std::distance(file.begin(), file.end());
	if (dir_len > file_len)
		return false;
	return std::equal(dir.begin(), dir.end(), file.begin(), pathcmp);
}

bool FileSystemObject::contains(const path_t &path) const{
	return path_contains_path(this->get_mapped_path(), path);
}

void FileSystemObject::set_unique_ids(BackupSystem &bs){
	this->stream_id = bs.get_stream_id();
}

void FileSystemObject::set_hash(const sha256_digest &digest){
	this->hash.digest = digest;
	this->hash.valid = true;
}
