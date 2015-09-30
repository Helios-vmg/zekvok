/*
Copyright (c), Helios
All rights reserved.

Distributed under a permissive license. See COPYING.txt for details.
*/

#include "stdafx.h"
#include "Transactions.h"
#include "Utility.h"
#include "Exception.h"

KernelTransaction::KernelTransaction(){
	this->tx = CreateTransaction(nullptr, 0, 0, 0, 0, 0, nullptr);
	if (this->tx == INVALID_HANDLE_VALUE){
		auto error = GetLastError();
		throw Win32Exception(error);
	}
}

KernelTransaction::~KernelTransaction(){
	if (std::uncaught_exception())
		this->rollback();
	else
		this->commit();
	this->release();
}

void KernelTransaction::commit(){
	CommitTransaction(this->tx);
}

void KernelTransaction::rollback(){
	RollbackTransaction(this->tx);
}

void KernelTransaction::release(){
	CloseHandle(this->tx);
}

TransactedFileSink::TransactedFileSink(const KernelTransaction &tx, const wchar_t *path){
	static const DWORD open_modes[] = {
		OPEN_EXISTING,
		CREATE_ALWAYS,
	};
	static USHORT TXFS_MINIVERSION_DEFAULT_VIEW = 0xFFFE;
	for (auto m : open_modes){
		this->handle = CreateFileTransactedW(
			path,
			GENERIC_WRITE,
			0,
			nullptr,
			m,
			0,
			nullptr,
			tx.get_handle(),
			&TXFS_MINIVERSION_DEFAULT_VIEW,
			nullptr
		);
		if (this->handle != INVALID_HANDLE_VALUE)
			break;
	}
	if (this->handle == INVALID_HANDLE_VALUE){
		auto error = GetLastError();
		throw Win32Exception(error);
	}
	SetFilePointer(this->handle, 0, 0, FILE_END);
}

TransactedFileSink::~TransactedFileSink(){
	CloseHandle(this->handle);
}

std::streamsize TransactedFileSink::write(const char *buffer, std::streamsize size){
	size_t ret = 0;
	while (size){
		DWORD byte_count = size & all_bits_on<DWORD>::value;
		DWORD bytes_written;
		auto result = WriteFile(this->handle, buffer, byte_count, &bytes_written, nullptr);
		if (!result)
			break;
		ret += bytes_written;
		buffer += bytes_written;
		size -= bytes_written;
	}
	return ret;
}
