/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"

int main(int argc, char **argv){
	CoInitialize(nullptr);
	random_number_generator.reset(new CryptoPP::AutoSeededRandomPool);
	LineProcessor processor(argc, argv);
	processor.process();
	return 0;
}
