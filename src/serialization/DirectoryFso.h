protected:
	virtual void iterate(FileSystemObject::iterate_co_t::push_type &sink) override;
public:
	DirectoryFso(){}
	DirectoryFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	DirectoryFso(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	virtual FileSystemObjectType get_type() const;
	iterate_co_t::pull_type get_iterator() override;
	FileSystemObject *find(path_t::iterator begin, path_t::iterator end) const;
