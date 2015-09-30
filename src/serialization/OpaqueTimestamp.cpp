/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "..\stdafx.h"
#include "fso.generated.h"

OpaqueTimestamp OpaqueTimestamp::utc_now(){
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	OpaqueTimestamp ret;
	ret.timestamp = ft.dwHighDateTime;
	ret.timestamp <<= 32;
	ret.timestamp |= ft.dwLowDateTime;
	return ret;
}
