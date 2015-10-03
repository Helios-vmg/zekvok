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

void OpaqueTimestamp::set_to_file_modification_time(const std::wstring &path){
	AutoHandle handle(CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr));
	if (handle.handle == INVALID_HANDLE_VALUE)
		throw Win32Exception(GetLastError());
	FILETIME mod;
	if (!GetFileTime(handle.handle, nullptr, nullptr, &mod))
		throw Win32Exception(GetLastError());
	this->set_to_FILETIME(mod);
}
