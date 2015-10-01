protected:
	int entry_number;
	BackupStream *backup_stream;
public:
	FileSystemObject(){}
	std::shared_ptr<std::wstring> get_link_target() const{
		return this->link_target;
	}
	bool get_is_main() const{
		return this->is_main;
	}
	void set_is_main(bool v){
		this->is_main = v;
	}
	std::shared_ptr<std::wstring> get_mapped_base_path() const{
		return this->mapped_base_path;
	}
	std::shared_ptr<std::wstring> get_unmapped_base_path() const{
		return this->unmapped_base_path;
	}
	void set_entry_number(int v){
		this->entry_number = v;
	}
	BackupStream *get_backup_stream() const{
		return this->backup_stream;
	}
	void set_backup_stream(BackupStream *bs){
		this->backup_stream = bs;
	}
	const Sha256Digest &get_hash() const{
		return this->hash;
	}
	void set_hash(const sha256_digest &);
	std::uint64_t get_stream_id() const{
		return this->stream_id;
	}
	std::uint64_t get_size() const{
		return this->size;
	}
	const Guid &get_file_system_guid() const{
		return this->file_system_guid;
	}

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
