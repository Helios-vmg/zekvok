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
