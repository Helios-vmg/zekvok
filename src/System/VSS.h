/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#pragma once

#include "../Exception.h"

class VssShadow{
	VSS_ID snapshot_set_id;
	VSS_ID id;
public:
	long snapshots_count;
	std::wstring snapshot_device_object,
		original_volume_name,
		originating_machine,
		service_machine,
		exposed_name,
		exposed_path;
	VSS_ID provider_id;
	long snapshot_attributes;
	VSS_TIMESTAMP created_at;
	VSS_SNAPSHOT_STATE status;
	VssShadow(const VSS_ID &snapshot_set_id, const VSS_ID &shadow_id):
		snapshot_set_id(snapshot_set_id),
		id(shadow_id){}
	VSS_ID get_id() const{
		return this->id;
	}
};

class SnapshotProperties{
	VSS_ID snapshot_set_id;
	std::vector<VssShadow> shadows;
public:
	void set_snapshot_set_id(const VSS_ID &snapshot_set_id){
		this->snapshot_set_id = snapshot_set_id;
	}
	const VSS_ID &get_snapshot_set_id() const{
		return this->snapshot_set_id;
	}
	void add_shadow_id(const VSS_ID &shadow_id){
		this->shadows.push_back(VssShadow(this->snapshot_set_id, shadow_id));
	}
	std::vector<VssShadow> &get_shadows(){
		return this->shadows;
	}
	const std::vector<VssShadow> &get_shadows() const{
		return this->shadows;
	}
};

class VssSnapshot{
	enum class VssState{
		Initial,
		PushingTargets,
		SnapshotPerformed,
		CleanedUp,
		Invalid,
	};
	VssState state;
	std::vector<std::wstring> targets;
	SnapshotProperties props;
	IVssBackupComponents *vbc;
	std::vector<std::wstring> failed_volumes;

	HRESULT populate_properties();
	void begin();
	HRESULT push_target(const std::wstring &target);
	HRESULT do_snapshot();
public:
	VssSnapshot(const std::vector<std::wstring> &targets);
	~VssSnapshot();
	const SnapshotProperties &get_snapshot_properties() const{
		return this->props;
	}
};
