protected:
	int entry_number;
public:
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
	void set_entry_number(int v){
		this->entry_number = v;
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
