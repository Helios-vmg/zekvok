/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "../stdafx.h"

#define CALL_HRESULT_FUNCTION(f, params)           \
	{                                              \
		auto hres = f params;                      \
		if (FAILED(hres))                          \
			throw HresultException(#f "()", hres); \
	}

#define CALL_ASYNC_FUNCTION(f)              \
	{                                       \
		IVssAsync *async = nullptr;         \
		CALL_HRESULT_FUNCTION(f, (&async)); \
		VssAsync(async).wait();             \
	}

class VssAsync{
	IVssAsync *async;
public:
	VssAsync(IVssAsync *async) : async(async){}
	~VssAsync(){
		if (this->async)
			this->async->Release();
	}
	void wait(){
		CALL_HRESULT_FUNCTION(this->async->Wait, ());
	}
};

std::wstring to_wstring(const VSS_ID &guid){
	LPOLESTR temp;
	auto hres = StringFromCLSID(guid, &temp);
	std::wstring ret = temp;
	CoTaskMemFree(temp);
	return ret;
}

static std::wstring to_wstring(const VSS_PWSZ &s){
	return !s ? std::wstring() : s;
}

VssSnapshot::VssSnapshot(const std::vector<std::wstring> &targets){
	this->state = VssState::Initial;
	this->vbc = nullptr;
	this->begin();
	for (auto &s : targets){
		auto hres = this->push_target(s);
		if (FAILED(hres)){
			if (hres == VSS_E_UNEXPECTED_PROVIDER_ERROR || hres == VSS_E_NESTED_VOLUME_LIMIT){
				this->failed_volumes.push_back(s);
				continue;
			}
			throw HresultException("VssSnapshot::push_target()", hres);
		}
	}
	auto hres = this->do_snapshot();
}

VssSnapshot::~VssSnapshot(){
	if (this->state == VssState::SnapshotPerformed){
		try{
			CALL_ASYNC_FUNCTION(this->vbc->GatherWriterStatus);
			CALL_HRESULT_FUNCTION(this->vbc->FreeWriterStatus, ());
			CALL_ASYNC_FUNCTION(this->vbc->BackupComplete);
			LONG deleted_snapshots;
			VSS_ID undeleted;
			CALL_HRESULT_FUNCTION(
				this->vbc->DeleteSnapshots,
				(
					this->props.get_snapshot_set_id(),
					VSS_OBJECT_SNAPSHOT_SET,
					true,
					&deleted_snapshots,
					&undeleted
				)
			);
		}catch (HresultException &){
		}
	}
	if (this->vbc)
		this->vbc->Release();
}

void VssSnapshot::begin(){
	this->state = VssState::Invalid;

	{
		IVssBackupComponents *temp;
		CALL_HRESULT_FUNCTION(CreateVssBackupComponents, (&temp));
		this->vbc = temp;
	}
	CALL_HRESULT_FUNCTION(this->vbc->InitializeForBackup, ());
	CALL_ASYNC_FUNCTION(this->vbc->GatherWriterMetadata);
	CALL_HRESULT_FUNCTION(this->vbc->FreeWriterMetadata, ());
	const auto context = VSS_CTX_APP_ROLLBACK;
	CALL_HRESULT_FUNCTION(this->vbc->SetContext, (context));
	VSS_ID snapshot_set_id;
	CALL_HRESULT_FUNCTION(this->vbc->StartSnapshotSet, (&snapshot_set_id));
	this->props.set_snapshot_set_id(snapshot_set_id);

	this->state = VssState::PushingTargets;
}

HRESULT VssSnapshot::push_target(const std::wstring &target){
	this->state = VssState::Invalid;

	std::vector<wchar_t> vtemp(target.size());
	std::copy(target.begin(), target.end(), vtemp.begin());
	vtemp.push_back(0);
	VSS_PWSZ temp = &vtemp[0];
	VSS_ID shadow_id;
	auto error = this->vbc->AddToSnapshotSet(temp, GUID_NULL, &shadow_id);
	if (FAILED(error)){
		if (error == VSS_E_UNEXPECTED_PROVIDER_ERROR || error == VSS_E_NESTED_VOLUME_LIMIT)
			this->state = VssState::PushingTargets;
		return error;
	}
	this->props.add_shadow_id(shadow_id);

	this->state = VssState::PushingTargets;
	return S_OK;
}

HRESULT VssSnapshot::do_snapshot(){
	this->state = VssState::Invalid;

	CALL_HRESULT_FUNCTION(this->vbc->SetBackupState, (false, true, VSS_BT_COPY));
	CALL_ASYNC_FUNCTION(this->vbc->PrepareForBackup);
	CALL_ASYNC_FUNCTION(this->vbc->GatherWriterStatus);
	CALL_HRESULT_FUNCTION(this->vbc->FreeWriterStatus, ());
	CALL_ASYNC_FUNCTION(this->vbc->DoSnapshotSet);

	this->state = VssState::SnapshotPerformed;

	return this->populate_properties();
}

class RaiiSnapshotProperties{
	VSS_SNAPSHOT_PROP props;
public:
	RaiiSnapshotProperties(const VSS_SNAPSHOT_PROP &props) : props(props){}
	~RaiiSnapshotProperties(){
		VssFreeSnapshotProperties(&this->props);
	}
	VSS_SNAPSHOT_PROP get_properties() const{
		return this->props;
	}
};

HRESULT VssSnapshot::populate_properties(){
	auto &shadows = this->props.get_shadows();

	for (auto &shadow : shadows){
		VSS_SNAPSHOT_PROP props;
		auto hres = this->vbc->GetSnapshotProperties(shadow.get_id(), &props);
		if (FAILED(hres))
			return hres;
		RaiiSnapshotProperties raii_props(props);
		shadow.snapshots_count        = props.m_lSnapshotsCount;
		shadow.snapshot_device_object = to_wstring(props.m_pwszSnapshotDeviceObject);
		shadow.original_volume_name   = to_wstring(props.m_pwszOriginalVolumeName);
		shadow.originating_machine    = to_wstring(props.m_pwszOriginatingMachine);
		shadow.service_machine        = to_wstring(props.m_pwszServiceMachine);
		shadow.exposed_name           = to_wstring(props.m_pwszExposedName);
		shadow.exposed_path           = to_wstring(props.m_pwszExposedPath);
		shadow.provider_id            = props.m_ProviderId;
		shadow.snapshot_attributes    = props.m_lSnapshotAttributes;
		shadow.created_at             = props.m_tsCreationTimestamp;
		shadow.status                 = props.m_eStatus;
	}
	return S_OK;
}
