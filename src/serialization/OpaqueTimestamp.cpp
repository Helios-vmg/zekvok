/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "fso.generated.h"
#include "../System/SystemOperations.h"
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

std::uint32_t OpaqueTimestamp::set_to_file_modification_time(const std::wstring &_path){
	auto path = path_from_string(_path);
	static const DWORD attributes[] = {
		FILE_ATTRIBUTE_NORMAL,
		FILE_FLAG_BACKUP_SEMANTICS,
	};
	DWORD error;
	for (auto attr : attributes){
		AutoHandle handle(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, attr, nullptr));
		if (handle.handle == INVALID_HANDLE_VALUE){
			error = GetLastError();
			continue;
		}
		FILETIME mod;
		if (!GetFileTime(handle.handle, nullptr, nullptr, &mod))
			return GetLastError();
		this->set_to_FILETIME(mod);
		return ERROR_SUCCESS;
	}
	return error;
}

void OpaqueTimestamp::print(std::ostream &stream) const{
	FILETIME utc, local;
	utc.dwHighDateTime = this->timestamp >> 32;
	utc.dwLowDateTime = this->timestamp & 0xFFFFFFFF;
	while (1){
		if (!FileTimeToLocalFileTime(&utc, &local))
			break;
		SYSTEMTIME st;
		zero_struct(st);
		if (!FileTimeToSystemTime(&local, &st))
			break;
		stream
			<< std::setw(4) << std::setfill('0') << st.wYear
			<< '-'
			<< std::setw(2) << std::setfill('0') << st.wMonth
			<< '-'
			<< std::setw(2) << std::setfill('0') << st.wDay
			<< ' '
			<< std::setw(2) << std::setfill('0') << st.wHour
			<< ':'
			<< std::setw(2) << std::setfill('0') << st.wMinute
			<< ':'
			<< std::setw(2) << std::setfill('0') << st.wSecond
			<< '.'
			<< std::setw(3) << std::setfill('0') << st.wMilliseconds;
		return;
	}
	stream << "<???>";
}
