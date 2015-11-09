/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"
#include "RsyncableFile.h"
#include "StreamBlockReader.h"
#include "MiscFunctions.h"
#include "MiscTypes.h"
#include "RollingChecksum.h"
#include "circular_buffer.h"
#include "FileComparer.h"

RsyncableFile::RsyncableFile(const std::wstring &path){
	const auto file_size = get_file_size(path.c_str());
	const auto block_size = scaler_function(file_size);
	this->block_size = block_size;
	
	BlockByBlockReader stream(path.c_str(), block_size);

	std::cout << "Selected block size: " << block_size << std::endl;

	this->rsync_table.reserve(blocks_per_file(file_size, block_size));
	CryptoPP::SHA1 global_sha1;
	circular_buffer buffer(1);
	file_offset_t offset = 0;
	while (stream.next_block(buffer)){
		CryptoPP::SHA1 local_sha1;
		global_sha1.Update(buffer.data(), buffer.size());
		local_sha1.Update(buffer.data(), buffer.size());

		rsync_table_item item;
		item.rolling_checksum = compute_rsync_rolling_checksum(buffer);
		local_sha1.Final(item.complex_hash);
		item.file_offset = offset;
		this->rsync_table.push_back(item);
		offset += buffer.size();
	}
	global_sha1.Final(this->sha1);
	std::sort(this->rsync_table.begin(), this->rsync_table.end());
	this->init_bitmap();
}

RsyncableFile::RsyncableFile(const FileComparer &comparer){
	this->rsync_table = comparer.get_new_table();
	this->init_bitmap();
	memcpy(this->sha1, comparer.get_new_digest(), sizeof(this->sha1));
	this->block_size = comparer.get_new_block_size();
}

void RsyncableFile::init_bitmap(){
	this->bitmap.resize(1 << 24, false);
	for (auto &i : this->rsync_table)
		this->bitmap[i.rolling_checksum & 0xFFFFFF] = true;
}

void RsyncableFile::save(const char *path){
	if (!this->rsync_table.size())
		return;
	std::ofstream file(path, std::ios::binary);
	if (!file)
		return;
	file.write((const char *)&this->rsync_table[0], sizeof(rsync_table_item) * this->rsync_table.size());
}

bool RsyncableFile::does_not_contain(rolling_checksum_t x){
	return !this->bitmap[x & 0xFFFFFF];
}
