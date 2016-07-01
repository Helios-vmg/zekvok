/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "../SimpleTypes.h"

std::wstring path_from_string(const std::wstring &path);

namespace system_ops{

template <typename ResultT, typename ErrorT>
class complex_result{
public:
	bool success;
	ErrorT error;
	ResultT result;
	complex_result(const ResultT &result):
		success(true),
		error(),
		result(result){}
	complex_result(const ErrorT &error):
		success(false),
		error(error),
		result(){}
};

enum class DriveType{
	// Summary:
    //     The type of drive is unknown.
    Unknown = 0,
    //
    // Summary:
    //     The drive does not have a root directory.
    NoRootDirectory = 1,
    //
    // Summary:
    //     The drive is a removable storage device, such as a floppy disk drive or a
    //     USB flash drive.
    Removable = 2,
    //
    // Summary:
    //     The drive is a fixed disk.
    Fixed = 3,
    //
    // Summary:
    //     The drive is a network drive.
    Network = 4,
    //
    // Summary:
    //     The drive is an optical disc device, such as a CD or DVD-ROM.
    CDRom = 5,
    //
    // Summary:
    //     The drive is a RAM disk.
    Ram = 6,
};

struct VolumeInfo{
	std::wstring volume_path;
	std::wstring volume_label;
	DriveType drive_type;
	std::vector<std::wstring> mounted_paths;
	VolumeInfo(): drive_type(DriveType::Unknown){}
	VolumeInfo(const std::wstring &, const std::wstring &, DriveType);
};

typedef boost::coroutines::asymmetric_coroutine<const VolumeInfo &> enumerate_volumes_co_t;
enumerate_volumes_co_t::pull_type enumerate_volumes();
typedef boost::coroutines::asymmetric_coroutine<const std::wstring &> enumerate_mounted_paths_co_t;
enumerate_mounted_paths_co_t::pull_type enumerate_mounted_paths(std::wstring volume_path);
FileSystemObjectType get_file_system_object_type(const std::wstring &);
complex_result<std::uint64_t, DWORD> get_file_size(const std::wstring &path);
complex_result<guid_t, DWORD> get_file_guid(const std::wstring &path);
complex_result<std::vector<std::wstring>, DWORD> list_all_hardlinks(const std::wstring &path);
complex_result<std::wstring, DWORD> get_reparse_point_target(const std::wstring &path);
bool get_archive_bit(const std::wstring &path);
DWORD create_symlink(const std::wstring &link_location, const std::wstring &target_location);
DWORD create_directory_symlink(const std::wstring &link_location, const std::wstring &target_location);
DWORD create_file_reparse_point(const std::wstring &link_location, const std::wstring &target_location);
DWORD create_junction(const std::wstring &link_location, const std::wstring &target_location);
DWORD create_hardlink(const std::wstring &link_location, const std::wstring &existing_file);

}
