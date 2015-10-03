private:
	//void set_members(const path_t &);
public:
	FileReparsePointFso();
	FileReparsePointFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	FileReparsePointFso(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	virtual FileSystemObjectType get_type() const;
