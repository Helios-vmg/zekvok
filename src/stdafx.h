#pragma once

#include <iostream>
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
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/ptr_container/ptr_container.hpp>
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

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
