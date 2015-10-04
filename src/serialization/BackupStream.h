protected:
	std::vector<FilishFso *> file_system_objects;
public:
	BackupStream(): unique_id(invalid_stream_id){}
	void add_file_system_object(FilishFso *fso){
		this->file_system_objects.push_back(fso);
	}
	DEFINE_INLINE_GETTER(file_system_objects)
	DEFINE_INLINE_SETTER_GETTER(unique_id)
	virtual bool has_data() const{
		return true;
	}
	virtual void get_dependencies(std::set<version_number_t> &dst) const = 0;
