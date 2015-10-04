/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "LineProcessor.h"
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <ctime>

int main(int argc, char **argv){
	CoInitialize(nullptr);
	LineProcessor processor(argc, argv);
	processor.process();
	return 0;
}
