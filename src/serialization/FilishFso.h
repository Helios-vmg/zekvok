private:
	void set_members(const path_t &path);
public:
	FilishFso(){}
	FilishFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	void set_file_system_guid(const path_t &, bool retry = true);
	bool compute_hash(sha256_digest &dst) const override;
	bool compute_hash() const override;
	iterate_co_t::pull_type get_iterator() override;
	FileSystemObject *find(path_t::iterator begin, path_t::iterator end) const;
