/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"

void UnmodifiedStream::get_dependencies(std::set<version_number_t> &dst) const{
	dst.insert(this->containing_version);
}

void FullStream::get_dependencies(std::set<version_number_t> &dst) const{}
