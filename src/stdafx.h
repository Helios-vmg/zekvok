/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#define WINVER       _WIN32_WINNT_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA

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
#include <sha.h>
#include <files.h>
#include <base64.h>
#include <rsa.h>
#include <oaep.h>
#include <osrng.h>
#include <cryptlib.h>
#include <twofish.h>
#include <serpent.h>
#include <aes.h>
#include <ccm.h>
#include <pwdbased.h>


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

/*
Explanation: A weak_unique_ptr is a smart pointer that *could* be a unique_ptr,
but doesn't need to be because the efficiency gains are irrelevant in the given
context.
In this case, it's redefined to shared_ptr because shared_ptr can be used to
declare class members without having to have the complete type definition for
the object type.
*/
#define weak_unique_ptr shared_ptr

#include "SimpleTypes.h"
#include "Globals.h"
#include "Exception.h"
#include "Utility.h"
#include "MemoryStream.h"
#include "serialization/fso.aux.h"
