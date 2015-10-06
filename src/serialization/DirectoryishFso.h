protected:
	std::vector<std::shared_ptr<FileSystemObject>> construct_children_list(const path_t &);
public:
	DirectoryishFso(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	DirectoryishFso(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	bool compute_hash(sha256_digest &dst) override{
		return false;
	}
	bool compute_hash() override{
		return false;
	}
	FileSystemObject *create_child(const std::wstring &name, const path_t *path = nullptr);
	void delete_existing(const std::wstring *base_path = nullptr) override;
	bool is_directoryish() const override{
		return true;
	}
