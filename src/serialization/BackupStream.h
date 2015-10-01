protected:
	std::vector<FileSystemObject *> file_system_objects;
public:
	const std::vector<FileSystemObject *> &get_file_system_objects() const{
		return this->file_system_objects;
	}
	std::uint64_t get_unique_id() const{
		return this->unique_id;
	}
	std::shared_ptr<std::istream> open_for_exclusive_read(std::uint64_t &size) const;
