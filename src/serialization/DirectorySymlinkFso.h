private:
	void set_target(const path_t &);
	virtual void iterate(FileSystemObject::iterate_co_t::push_type &sink) override;
	virtual void reverse_iterate(FileSystemObject::iterate_co_t::push_type &sink) override;
	virtual void restore_internal(const path_t *base_path) override;
	
public:
	DirectorySymlinkFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	DirectorySymlinkFso(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	virtual FileSystemObjectType get_type() const;
	const FileSystemObject *find(path_t::iterator begin, path_t::iterator end) const;
	FileSystemObject *find(path_t::iterator begin, path_t::iterator end){
		return (FileSystemObject *)(((const FileSystemObject *)this)->find(begin, end));
	}
