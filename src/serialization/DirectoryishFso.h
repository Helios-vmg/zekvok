protected:
	std::vector<std::shared_ptr<FileSystemObject>> construct_children_list(const path_t &);
	void delete_existing_internal(const std::wstring *base_path = nullptr) override;
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
	bool is_directoryish() const override{
		return true;
	}
