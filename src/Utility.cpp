/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "Utility.h"

std::set<std::wstring, strcmpci> known_text_extensions = {
	L"asm",
	L"bat",
	L"c",
	L"cpp",
	L"cs",
	L"cxx",
	L"f",
	L"gcl",
	L"gitignore",
	L"gitmodules",
	L"h",
	L"hpp",
	L"hs",
	L"htm",
	L"html",
	L"hxx",
	L"java",
	L"js",
	L"log",
	L"lua",
	L"pas",
	L"py",
	L"qbk",
	L"s",
	L"sh",
	L"ss",
	L"txt",
	L"xml",
};

bool is_text_extension(const std::wstring &ext){
	return known_text_extensions.find(ext) != known_text_extensions.end();
}
