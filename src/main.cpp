/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "LineProcessor.h"
#include "StreamProcessor.h"
#include "MemoryStream.h"
#include "Globals.h"
#include "System/Threads.h"

void test1(){
	using zstreams::Stream;
	char hello[] = { 'H', 'e', 'l', 'l', 'o', ' ', };
	char world[] = { 'W', 'o', 'r', 'l', 'd', '!', '\n', };
	char hello_world[] = { 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!', '\n', };
	buffer_t memory;
	{
		zstreams::StreamPipeline pipeline;
		Stream<zstreams::MemorySink> mem(memory, pipeline);
		{
			boost::iostreams::stream<zstreams::SynchronousSink> sink(*mem);
			sink.write(hello, sizeof(hello));
		}
		{
			boost::iostreams::stream<zstreams::SynchronousSink> sink(*mem);
			sink.write(world, sizeof(world));
		}
	}
	if (memory.size() != sizeof(hello_world)){
		std::cout << "!=\n";
		return;
	}
	int cmp = memcmp(hello_world, &memory[0], sizeof(hello_world));
	std::cout << cmp << std::endl;
}

void test(){
	test1();
	exit(0);
}

int main(int argc, char **argv){
	CoInitialize(nullptr);
	random_number_generator.reset(new CryptoPP::AutoSeededRandomPool);
	thread_pool.reset(new ThreadPool);
#if defined _DEBUG && 0
	test();
#endif
	LineProcessor processor(argc, argv);
	processor.process();
	return 0;
}
