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
