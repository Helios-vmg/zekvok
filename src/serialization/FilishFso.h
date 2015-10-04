private:
	void set_members(const path_t &path);
	
protected:
	virtual void iterate(FileSystemObject::iterate_co_t::push_type &sink) override;
public:
	FilishFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	FilishFso(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	void set_file_system_guid(const path_t &, bool retry = true);
	bool compute_hash(sha256_digest &dst) override;
	bool compute_hash() override;
	iterate_co_t::pull_type get_iterator() override;
	const FileSystemObject *find(path_t::iterator begin, path_t::iterator end) const;
	FileSystemObject *find(path_t::iterator begin, path_t::iterator end){
		return (FileSystemObject *)(((const FileSystemObject *)this)->find(begin, end));
	}
	void delete_existing(const std::wstring &base_path = nullptr) override;
