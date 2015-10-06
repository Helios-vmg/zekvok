private:
	void set_members(const path_t &path);
	
protected:
	virtual void iterate(FileSystemObject::iterate_co_t::push_type &sink) override;
	virtual void reverse_iterate(FileSystemObject::iterate_co_t::push_type &sink) override;
	void delete_existing_internal(const std::wstring *base_path = nullptr) override;
public:
	FilishFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	FilishFso(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	DEFINE_INLINE_GETTER(hash)
	void set_hash(const sha256_digest &);
	DEFINE_INLINE_GETTER(file_system_guid)
	void set_file_system_guid(const path_t &, bool retry = true);
	bool compute_hash(sha256_digest &dst) override;
	bool compute_hash() override;
	const FileSystemObject *find(path_t::iterator begin, path_t::iterator end) const;
	FileSystemObject *find(path_t::iterator begin, path_t::iterator end){
		return (FileSystemObject *)(((const FileSystemObject *)this)->find(begin, end));
	}
