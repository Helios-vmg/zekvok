/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "Utility.h"

static const char * const known_text_extensions[] = {
	"asm",
	"bat",
	"c",
	"cpp",
	"f",
	"gcl",
	"gitignore",
	"gitmodules",
	"h",
	"hpp",
	"hs",
	"htm",
	"html",
	"java",
	"js",
	"log",
	"lua",
	"pas",
	"py",
	"qbk",
	"s",
	"sh",
	"ss",
	"txt",
	"xml",
};
static const size_t known_text_extensions_size = sizeof(known_text_extensions) / sizeof(*known_text_extensions);

bool is_text_extension(const std::wstring &ext){
	return existence_binary_search(
		known_text_extensions,
		known_text_extensions + known_text_extensions_size,
		[&ext](const char *s){
			return strcmpci::less_than(ext, s);
		}
	);
}
