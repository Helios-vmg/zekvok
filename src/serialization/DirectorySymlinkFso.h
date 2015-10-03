private:
	void set_target(const path_t &);
public:
	DirectorySymlinkFso(){}
	DirectorySymlinkFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	virtual FileSystemObjectType get_type() const;
	iterate_co_t::pull_type get_iterator() override;
	FileSystemObject *find(path_t::iterator begin, path_t::iterator end) const;
