/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "../SimpleTypes.h"

namespace system_ops{

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
	VolumeInfo(){}
	VolumeInfo(const std::wstring &, const std::wstring &, DriveType);
};

typedef boost::coroutines::asymmetric_coroutine<const VolumeInfo &> enumerate_volumes_co_t;
enumerate_volumes_co_t::pull_type enumerate_volumes();
typedef boost::coroutines::asymmetric_coroutine<const std::wstring &> enumerate_mounted_paths_co_t;
enumerate_mounted_paths_co_t::pull_type enumerate_mounted_paths(std::wstring volume_path);
FileSystemObjectType get_file_system_object_type(const std::wstring &);
std::uint64_t get_file_size(const std::wstring &path);
guid_t get_file_guid(const std::wstring &path);
std::vector<std::wstring> list_all_hardlinks(const std::wstring &path);
std::wstring get_reparse_point_target(const std::wstring &path);
}
