protected:
	std::vector<FileSystemObject *> file_system_objects;
public:
	BackupStream(){}
	void add_file_system_object(FileSystemObject *fso){
		this->file_system_objects.push_back(fso);
	}
	DEFINE_INLINE_GETTER(file_system_objects)
	DEFINE_INLINE_SETTER_GETTER(unique_id)
	std::shared_ptr<std::istream> open_for_exclusive_read(std::uint64_t &size) const;
	virtual bool has_data() const{
		return true;
	}
	virtual void get_dependencies(std::set<version_number_t> &dst) const = 0;
