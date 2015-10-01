protected:
	std::vector<FileSystemObject *> file_system_objects;
public:
	BackupStream(){}
	void add_file_system_object(FileSystemObject *fso){
		this->file_system_objects.push_back(fso);
	}
	const std::vector<FileSystemObject *> &get_file_system_objects() const{
		return this->file_system_objects;
	}
	void set_unique_id(std::uint64_t unique_id){
		this->unique_id = unique_id;
	}
	std::uint64_t get_unique_id() const{
		return this->unique_id;
	}
	std::shared_ptr<std::istream> open_for_exclusive_read(std::uint64_t &size) const;
