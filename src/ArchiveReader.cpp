#include "stdafx.h"
#include "ArchiveReader.h"

ArchiveReader::ArchiveReader(const path_t &path){
	this->stream.reset(new boost::filesystem::ifstream(path, std::ios::binary));
	if (!*this->stream)
		throw std::exception("File not found.");
	//this->filtered_stream.reset(new boost::iostreams::filtering_istream);
	this->filters.push_back([](boost::iostreams::filtering_istream &filter){

	});
}

std::shared_ptr<VersionManifest> ArchiveReader::read_manifest(){
	return std::shared_ptr<VersionManifest>();
}

std::vector<std::shared_ptr<FileSystemObject>> ArchiveReader::read_base_objects(){
	return std::vector<std::shared_ptr<FileSystemObject>>();
}

std::vector<std::shared_ptr<FileSystemObject>> ArchiveReader::get_base_objects(){
	return std::vector<std::shared_ptr<FileSystemObject>>();
}

void ArchiveReader::begin(std::function<void(std::uint64_t, std::istream &)> &){
}
