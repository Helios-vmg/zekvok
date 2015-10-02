protected:
	int entry_number;
	BackupStream *backup_stream;
	BackupMode backup_mode;
	bool archive_flag;
public:
	FileSystemObject(){}
	DEFINE_INLINE_SETTER_GETTER(link_target)
	DEFINE_INLINE_SETTER_GETTER(is_main)
	DEFINE_INLINE_SETTER_GETTER(mapped_base_path)
	DEFINE_INLINE_SETTER_GETTER(unmapped_base_path)
	DEFINE_INLINE_SETTER_GETTER(entry_number)
	DEFINE_INLINE_SETTER_GETTER(backup_stream)
	DEFINE_INLINE_GETTER(hash)
	void set_hash(const sha256_digest &);
	DEFINE_INLINE_SETTER_GETTER(stream_id)
	DEFINE_INLINE_GETTER(size)
	DEFINE_INLINE_GETTER(file_system_guid)
	DEFINE_INLINE_SETTER_GETTER(latest_version)
	DEFINE_INLINE_SETTER_GETTER(backup_mode)
	DEFINE_INLINE_GETTER(name)
	DEFINE_INLINE_SETTER_GETTER(archive_flag)
	DEFINE_INLINE_SETTER_GETTER(modification_time)
	path_t get_path() const;
	bool contains(const path_t &) const;
	void set_unique_ids(BackupSystem *);

	class ErrorReporter{
	public:
		virtual bool report_error(std::exception &e, const char *context) = 0;
	};

	class CreationSettings{
	public:
		std::shared_ptr<ErrorReporter> reporter;
		std::function<BackupMode(const FileSystemObject &)> backup_mode_map;
	};

	static FileSystemObject *create(const path_t &path, const path_t &unmapped_path, CreationSettings &);

	typedef boost::coroutines::asymmetric_coroutine<FileSystemObject *> iterate_co_t;
	iterate_co_t::pull_type get_iterator() const;
	virtual bool is_directoryish() const{
		return false;
	}
	virtual bool is_linkish() const{
		return false;
	}
	virtual FileSystemObjectType get_type() const = 0;
	FileSystemObject *find(const path_t &) const;
	virtual bool compute_hash(sha256_digest &dst) const = 0;
	virtual bool compute_hash() const = 0;
