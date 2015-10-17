/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <set>
#include <vector>
#include <codecvt>
#include <memory>
#include <cstdint>
#include <exception>
#include <sstream>
#include <map>
#include <deque>
#include <algorithm>
#include <functional>
#include <cassert>
#include <utility>
#include <tuple>
#include <thread>
#include <mutex>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/ptr_container/ptr_container.hpp>
#include <boost/coroutine/coroutine.hpp>
#include <boost/any.hpp>
#include <boost/regex.hpp>
#include <boost/optional.hpp>
#define LZMA_API_STATIC
#include <lzma.h>
#include <cryptlib/sha.h>
#include <cryptlib/files.h>
#include <cryptlib/base64.h>
#include <cryptlib/rsa.h>
#include <cryptlib/oaep.h>
#include <cryptlib/osrng.h>
#include <cryptlib/cryptlib.h>
#include <cryptlib/twofish.h>
#include <cryptlib/serpent.h>
#include <cryptlib/aes.h>
#include <cryptlib/ccm.h>
#include <cryptlib/pwdbased.h>
#include <Windows.h>
#include <ktmw32.h>
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <comdef.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "SimpleTypes.h"
#include "SymbolicConstants.h"
#include "Exception.h"
#include "Utility.h"
#include "serialization/fso.generated.h"
#include "System/SystemOperations.h"
#include "System/Transactions.h"
#include "System/VSS.h"
#include "Filters.h"
#include "AutoHandle.h"
#include "BoundedStreamFilter.h"
#include "ComposingFilter.h"
#include "CryptoFilter.h"
#include "HashFilter.h"
#include "LzmaFilter.h"
#include "NullStream.h"
#include "ArchiveIO.h"
#include "VersionForRestore.h"
#include "BackupSystem.h"
#include "LineProcessor.h"
#include "serialization/ImplementedDS.h"
