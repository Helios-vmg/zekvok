/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include <boost/filesystem.hpp>
#include <boost/coroutine/coroutine.hpp>
//#include <array>
//#include <set>
//#include <map>
//#include "../stdafx.h"
#include "../SimpleTypes.h"

#define DEFINE_INLINE_GETTER(x) const decltype(x) &get_##x() const{ return this->x; } 
#define DEFINE_INLINE_SETTER(x) void set_##x(const decltype(x) &v){ this->x = v; }
#define DEFINE_INLINE_SETTER_GETTER(x) DEFINE_INLINE_GETTER(x) DEFINE_INLINE_SETTER(x)
