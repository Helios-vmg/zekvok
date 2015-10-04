public:
	
	class ErrorReporter{
	public:
		virtual bool report_error(const std::exception &e, const char *context) = 0;
	};

	class CreationSettings{
	public:
		BackupSystem *backup_system;
		std::shared_ptr<ErrorReporter> reporter;
		std::function<BackupMode(const FileSystemObject &)> backup_mode_map;
	};
	typedef boost::coroutines::asymmetric_coroutine<FileSystemObject *> iterate_co_t;

private:
	std::shared_ptr<ErrorReporter> reporter;

	void add_exception(const std::exception &e);
	ErrorReporter *get_reporter();
	void set_file_attributes(const path_t &);
	void default_values();

protected:
	int entry_number;
	BackupStream *backup_stream;
	BackupMode backup_mode;
	bool archive_flag;
	BackupSystem *backup_system;
	std::shared_ptr<std::function<BackupMode(const FileSystemObject &)>> backup_mode_map;

	enum class BasePathType{
		Mapped,
		Unmapped,
		Override,
	};

	path_t path_override_base(const std::wstring * = nullptr, BasePathType = BasePathType::Mapped) const;
	void set_backup_mode();
	std::shared_ptr<std::function<BackupMode(const FileSystemObject &)>> get_backup_mode_map();
	FileSystemObject *get_root();
	virtual bool get_stream_required() const{
		return false;
	}
	virtual void restore_internal(const path_t *base_path);
	path_t path_override_unmapped_base_weak(const path_t *base_path = nullptr){
		if (!base_path)
			return this->path_override_unmapped_base_weak((const std::wstring *)nullptr);
		return this->path_override_unmapped_base_weak(&base_path->wstring());
	}
	path_t path_override_unmapped_base_weak(const std::wstring *base_path = nullptr){
		return this->path_override_base(base_path, base_path ? BasePathType::Override : BasePathType::Unmapped);
	}

public:
	FileSystemObject(const path_t &path, const path_t &unmapped_path, CreationSettings &settings);
	FileSystemObject(FileSystemObject *parent, const std::wstring &name, const path_t *path = nullptr);
	DEFINE_INLINE_SETTER_GETTER(link_target)
	DEFINE_INLINE_SETTER_GETTER(is_main)
	DEFINE_INLINE_SETTER_GETTER(mapped_base_path)
	DEFINE_INLINE_SETTER_GETTER(unmapped_base_path)
	DEFINE_INLINE_SETTER_GETTER(entry_number)
	DEFINE_INLINE_SETTER_GETTER(backup_stream)
	DEFINE_INLINE_SETTER_GETTER(stream_id)
	DEFINE_INLINE_GETTER(size)
	DEFINE_INLINE_SETTER_GETTER(latest_version)
	DEFINE_INLINE_SETTER_GETTER(backup_mode)
	DEFINE_INLINE_GETTER(name)
	DEFINE_INLINE_SETTER_GETTER(archive_flag)
	DEFINE_INLINE_SETTER_GETTER(modification_time)
	DEFINE_INLINE_SETTER_GETTER(parent)
	void set_link_target(const path_t &path){
		this->set_link_target(make_shared(new std::wstring(path.wstring())));
	}
	path_t get_mapped_path() const;
	path_t get_unmapped_path() const;
	path_t get_path_without_base() const;
	bool contains(const path_t &) const;
	virtual void set_unique_ids(BackupSystem &);

	static FileSystemObject *create(const path_t &path, const path_t &unmapped_path, CreationSettings &);

	virtual iterate_co_t::pull_type get_iterator() = 0;
	virtual bool is_directoryish() const{
		return false;
	}
	bool is_linkish() const{
		auto type = this->get_type();
		return type == FileSystemObjectType::DirectorySymlink ||
			type == FileSystemObjectType::Junction ||
			type == FileSystemObjectType::FileSymlink ||
			type == FileSystemObjectType::FileHardlink;
	}
	virtual FileSystemObjectType get_type() const = 0;
	const FileSystemObject *find(const path_t &path) const;
	FileSystemObject *find(const path_t &path){
		return (FileSystemObject *)(((const FileSystemObject *)this)->find(path));
	}
	virtual FileSystemObject *find(path_t::iterator begin, path_t::iterator end) = 0;
	virtual const FileSystemObject *find(path_t::iterator begin, path_t::iterator end) const = 0;
	virtual bool compute_hash(sha256_digest &dst) = 0;
	virtual bool compute_hash() = 0;
	std::shared_ptr<std::istream> open_for_exclusive_read(std::uint64_t &size) const;
	bool report_error(const std::exception &, const std::string &context);
	virtual void iterate(iterate_co_t::push_type &sink) = 0;
	virtual void restore(std::istream &, const path_t *base_path = nullptr);
	virtual bool restore(const path_t *base_path = nullptr);
	virtual void delete_existing(const std::wstring &base_path = nullptr) = 0;
