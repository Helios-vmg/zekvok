#include "stdafx.h"
#include "LineProcessor.h"
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <ctime>

int main(int argc, char **argv){
	LineProcessor processor(argc, argv);
	processor.process();
	return 0;
}