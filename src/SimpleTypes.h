/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "SymbolicConstants.h"

typedef std::int32_t version_number_t;
typedef std::uint64_t stream_id_t;

enum class ChangeCriterium{
	ArchiveFlag = 0,
	Size,
	Date,
	Hash,
	HashAuto,
	Default = HashAuto,
};

enum class NameIgnoreType{
	File = 0,
	Directory,
	All,
};

typedef boost::filesystem::wpath path_t;
typedef std::array<std::uint8_t, sha256_digest_length> sha256_digest;

enum class BackupMode{
	//File or directory will not be backed up.
    NoBackup,
    //Directory will be backed up. Only for directoryish file system objects.
    //Link-like objects will be backed up as links.
    Directory,
    //File will be backed up unconditionally. Only for filish file system
    //objects.
    ForceFull,
    //File will be backed up fully if changes were made to it. Only for
    //filish file system objects.
    Full,
    //Used to signal that no changes have ocurred since the last time the
    //file was backup up, and so a reference to the old version will be
    //stored, rather than file data.
    Unmodified,
    //File will be backed up if changes were made to it, storing only the
    //parts of the file that changed, using the rsync algorithm. Only for
    //filish file system objects.
    Rsync,
};
