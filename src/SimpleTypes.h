/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

typedef std::int32_t version_number_t;
typedef std::uint64_t stream_id_t;
typedef std::uint64_t differential_chain_id_t;
typedef size_t stream_index_t;

enum class ChangeCriterium{
	ArchiveFlag = 0,
	Size,
	Date,
	Hash,
	HashAuto,
	Default = HashAuto,
};

enum class NameIgnoreType{
	None = 0,
	File,
	Directory,
	All,
};

typedef boost::filesystem::wpath path_t;
const int sha256_digest_length = 256/8;
typedef std::array<std::uint8_t, sha256_digest_length> sha256_digest;

typedef std::array<std::uint8_t, 16> guid_t;

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

enum class FileSystemObjectType{
	Directory,
    RegularFile,
    DirectorySymlink,
    Junction,
    FileSymlink,
    FileReparsePoint,
    FileHardlink,
};

typedef std::uint8_t byte_t;
typedef std::vector<byte_t> buffer_t;
