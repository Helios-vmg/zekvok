/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "fso.generated.h"
#include "../Exception.h"
#include "../Utility.h"
#include "../AutoHandle.h"
#include "../System/SystemOperations.h"

OpaqueTimestamp OpaqueTimestamp::utc_now(){
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	return OpaqueTimestamp(ft);
}

void OpaqueTimestamp::operator=(std::uint64_t t){
	this->timestamp = t;
}
bool OpaqueTimestamp::operator==(const OpaqueTimestamp &b) const{
	return this->timestamp == b.timestamp;
}

bool OpaqueTimestamp::operator!=(const OpaqueTimestamp &b) const{
	return this->timestamp != b.timestamp;
}

void OpaqueTimestamp::set_to_file_modification_time(const std::wstring &_path){
	auto path = path_from_string(_path);
	static const DWORD attributes[] = {
		FILE_ATTRIBUTE_NORMAL,
		FILE_FLAG_BACKUP_SEMANTICS,
	};
	DWORD error;
	for (auto attr : attributes){
		AutoHandle handle(CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, attr, nullptr));
		if (handle.handle == INVALID_HANDLE_VALUE){
			error = GetLastError();
			continue;
		}
		FILETIME mod;
		if (!GetFileTime(handle.handle, nullptr, nullptr, &mod))
			throw Win32Exception(GetLastError());
		this->set_to_FILETIME(mod);
		return;
	}
	throw Win32Exception(error);
}
