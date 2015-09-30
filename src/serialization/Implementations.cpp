/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "fso.generated.h"

#define DEFINE_TRIVIAL_IMPLEMENTATIONS(x) x::~x(){} void x::rollback_deserialization(){}

DEFINE_TRIVIAL_IMPLEMENTATIONS(Sha256Digest)
DEFINE_TRIVIAL_IMPLEMENTATIONS(Guid)
DEFINE_TRIVIAL_IMPLEMENTATIONS(FileSystemObject)
DEFINE_TRIVIAL_IMPLEMENTATIONS(DirectoryishFso)
DEFINE_TRIVIAL_IMPLEMENTATIONS(DirectoryFso)
DEFINE_TRIVIAL_IMPLEMENTATIONS(DirectorySymlinkFso)
DEFINE_TRIVIAL_IMPLEMENTATIONS(JunctionFso)
DEFINE_TRIVIAL_IMPLEMENTATIONS(FilishFso)
DEFINE_TRIVIAL_IMPLEMENTATIONS(RegularFileFso)
DEFINE_TRIVIAL_IMPLEMENTATIONS(FileHardlinkFso)
DEFINE_TRIVIAL_IMPLEMENTATIONS(FileSymlinkFso)
DEFINE_TRIVIAL_IMPLEMENTATIONS(FileReparsePointFso)
DEFINE_TRIVIAL_IMPLEMENTATIONS(ArchiveMetadata)
DEFINE_TRIVIAL_IMPLEMENTATIONS(VersionManifest)
DEFINE_TRIVIAL_IMPLEMENTATIONS(OpaqueTimestamp)
