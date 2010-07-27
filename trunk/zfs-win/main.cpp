/* 
 *	Copyright (C) 2010 Gabest
 *	http://code.google.com/p/zfs-win/
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "Pool.h"
#include "DataSet.h"
#include "String.h"
#include "../dokan/dokan.h"

using namespace Util;

namespace ZFS
{
	class Context
	{
	public:
		Pool m_pool;
		DataSet* m_root;
		DataSet* m_mounted;
		std::wstring m_name;

		Context() {m_root = NULL; m_mounted = NULL;}
		~Context() {delete m_root;}

		bool Init(std::list<std::wstring>& paths, const std::wstring& name)
		{
			m_name = name;

			if(!m_pool.Open(paths, name.c_str()))
			{
				wprintf(L"Failed to open pool\n");

				return false;
			}

			m_root = new ZFS::DataSet(&m_pool);
		
			if(!m_root->Init(&m_pool.m_devs.front()->m_active->rootbp))
			{
				wprintf(L"Failed to read root dataset\n");

				return false;
			}

			m_mounted = m_root;

			return true;
		}

		bool SetDataSet(std::wstring& dataset)
		{
			if(!m_root->Find(dataset.c_str(), &m_mounted))
			{
				std::list<std::wstring> sl;

				sl.push_back(UTF8To16(m_pool.m_name.c_str()));
				
				if(!dataset.empty()) 
				{
					sl.push_back(dataset);
				}

				wprintf(L"Cannot find dataset '%s'\n", Implode(sl, L"/").c_str());

				return false;
			}

			return true;
		}

		void List(const DataSet* ds, std::wstring parent = L"")
		{
			std::wstring s;
			
			if(!parent.empty())
			{
				s = parent + L"/";
			}
			
			s += Util::UTF8To16(ds->m_name.c_str());

			// wprintf(L"[%d] %s\n", clock(), s.c_str());

			for(auto i = ds->m_children.begin(); i != ds->m_children.end(); i++)
			{
				List(*i, s);
			}
		}
	};

	class FileContext
	{
	public:
		dnode_phys_t node;
		BlockReader reader;
	
		FileContext(Pool* pool, dnode_phys_t* dn)
			: node(*dn)
			, reader(pool, dn)
		{
		}
	};

	static void UnixTimeToFileTime(uint64_t t, FILETIME* pft)
	{
		// http://support.microsoft.com/kb/167296
     
		*(uint64_t*)pft = (t * 10000000) + 116444736000000000ull;
	}

	static int WINAPI CreateFile(
		LPCWSTR FileName, 
		DWORD AccessMode, 
		DWORD ShareMode, 
		DWORD CreationDisposition, 
		DWORD FlagsAndAttributes, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		std::wstring fn = FileName;

		fn = TrimRight(fn, L"\\");

		dnode_phys_t dn;

		memset(&dn, 0, sizeof(dn));

		if(!ctx->m_mounted->Find(fn.c_str(), dn))
		{
			dn.type = 0;
		}

		switch(CreationDisposition)
		{
		case CREATE_NEW:
			return dn.type != 0 ? -ERROR_FILE_EXISTS : -ERROR_ACCESS_DENIED;
		case CREATE_ALWAYS:
			return -ERROR_ACCESS_DENIED;
		case TRUNCATE_EXISTING:
			return dn.type == 0 ? -ERROR_FILE_NOT_FOUND : -ERROR_ACCESS_DENIED;
		case OPEN_EXISTING:
			if(dn.type == 0) return -ERROR_FILE_NOT_FOUND;
			break;
		case OPEN_ALWAYS:
			if(dn.type == 0) return -ERROR_ACCESS_DENIED;
			break;
		default:
			return -1;
		}

		if(dn.type == DMU_OT_DIRECTORY_CONTENTS)
		{
			DokanFileInfo->IsDirectory = 1;
		}

		DokanFileInfo->Context = (ULONG64)new FileContext(&ctx->m_pool, &dn);

		return CreationDisposition == OPEN_ALWAYS ? ERROR_ALREADY_EXISTS : 0;
	}

	static int WINAPI OpenDirectory(
		LPCWSTR FileName, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		std::wstring fn = FileName;

		fn = TrimRight(fn, L"\\");

		dnode_phys_t dn;

		if(!ctx->m_mounted->Find(fn.c_str(), dn) || dn.type != DMU_OT_DIRECTORY_CONTENTS)
		{
			return -ERROR_PATH_NOT_FOUND;
		}

		DokanFileInfo->Context = (ULONG64)new FileContext(&ctx->m_pool, &dn);

		return 0;
	}

	static int WINAPI CreateDirectory(
		LPCWSTR FileName, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI Cleanup(
		LPCWSTR FileName, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		if(DokanFileInfo->Context != 0)
		{
			delete (FileContext*)DokanFileInfo->Context;

			DokanFileInfo->Context = 0;
		}

		return 0;
	}

	static int WINAPI CloseFile(
		LPCWSTR FileName, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		if(DokanFileInfo->Context != 0)
		{
			delete (FileContext*)DokanFileInfo->Context;

			DokanFileInfo->Context = 0;
		}

		return 0;
	}

	static int WINAPI ReadFile(
		LPCWSTR FileName, 
		LPVOID Buffer, 
		DWORD BufferLength, 
		LPDWORD ReadLength, 
		LONGLONG Offset, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"%[%d] %s: %s %d %I64d\n", clock(), __FUNCTIONW__, FileName, BufferLength, Offset);

		FileContext* fctx = (FileContext*)DokanFileInfo->Context;

		znode_phys_t* znode = (znode_phys_t*)fctx->node.bonus();

		BufferLength = (DWORD)std::min<uint64_t>(znode->size - Offset, BufferLength);

		*ReadLength = fctx->reader.Read(Buffer, BufferLength, Offset);

		if(*ReadLength != BufferLength)
		{
			return -ERROR_READ_FAULT;
		}

		return 0;
	}

	static int WINAPI WriteFile(
		LPCWSTR FileName, 
		LPCVOID Buffer, 
		DWORD NumberOfBytesToWrite, 
		LPDWORD NumberOfBytesWritten, 
		LONGLONG Offset, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI FlushFileBuffers(
		LPCWSTR FileName, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI GetFileInformation(
		LPCWSTR FileName, 
		LPBY_HANDLE_FILE_INFORMATION HandleFileInformation, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		FileContext* fctx = (FileContext*)DokanFileInfo->Context;

		znode_phys_t* node = (znode_phys_t*)fctx->node.bonus();

		UnixTimeToFileTime(node->crtime[0], &HandleFileInformation->ftCreationTime);
		UnixTimeToFileTime(node->atime[0], &HandleFileInformation->ftLastAccessTime);
		UnixTimeToFileTime(node->mtime[0], &HandleFileInformation->ftLastWriteTime);

		HandleFileInformation->dwFileAttributes = 0;

		if(1)
		{
			HandleFileInformation->dwFileAttributes |= FILE_ATTRIBUTE_READONLY; // TODO
		}

		if(fctx->node.type == DMU_OT_DIRECTORY_CONTENTS)
		{
			HandleFileInformation->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY; // TODO: symlinks pointing to directories
		}

		HandleFileInformation->dwVolumeSerialNumber = 0;
		HandleFileInformation->nFileSizeLow = (DWORD)(node->size);
		HandleFileInformation->nFileSizeHigh = (DWORD)(node->size >> 32);
		HandleFileInformation->nNumberOfLinks = 1;
		HandleFileInformation->nFileIndexLow = 0;
		HandleFileInformation->nFileIndexHigh = 0;

		return 0;
	}

	static int WINAPI FindFiles(
		LPCWSTR FileName,
		PFillFindData FillFindData, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI FindFilesWithPattern(
		LPCWSTR PathName,
		LPCWSTR SearchPattern,
		PFillFindData Callback, // call this function with PWIN32_FIND_DATAW
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s %s\n", clock(), __FUNCTIONW__, PathName, SearchPattern);

		if(DokanFileInfo->Context != 0)
		{
			FileContext* fctx = (FileContext*)DokanFileInfo->Context;

			if(fctx->node.type != DMU_OT_DIRECTORY_CONTENTS)
			{
				return -1;
			}

			ZFS::ZapObject* zap = NULL;

			if(!ctx->m_mounted->m_head->Read(fctx->node.pad3[0], &zap))
			{
				return -1;
			}

			bool everything = wcscmp(SearchPattern, L"*") == 0;
			bool wildcards = wcschr(SearchPattern, '*') != NULL || wcschr(SearchPattern, '?') != NULL;

			for(auto i = zap->begin(); i != zap->end(); i++)
			{
				std::string fn = i->first;

				uint64_t index = 0;
				
				if(!zap->Lookup(fn.c_str(), index))
				{
					return false;
				}

				index = ZFS_DIRENT_OBJ(index);

				std::wstring wfn = Util::UTF8To16(fn.c_str());

				if(!everything && !(wildcards ? PathMatchSpec(wfn.c_str(), SearchPattern) : wfn == SearchPattern))
				{
					continue;
				}

				dnode_phys_t subdn;

				if(!ctx->m_mounted->m_head->Read(index, &subdn))
				{
					return false;
				}

				znode_phys_t* node = (znode_phys_t*)subdn.bonus();

				WIN32_FIND_DATAW fd;

				fd.dwFileAttributes = 0;

				if(1)
				{
					fd.dwFileAttributes |= FILE_ATTRIBUTE_READONLY; // TODO
				}

				if(subdn.type == DMU_OT_DIRECTORY_CONTENTS)
				{
					fd.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY; // TODO: symlinks pointing to directories
				}

				UnixTimeToFileTime(node->crtime[0], &fd.ftCreationTime);
				UnixTimeToFileTime(node->atime[0], &fd.ftLastAccessTime);
				UnixTimeToFileTime(node->mtime[0], &fd.ftLastWriteTime);

				fd.nFileSizeLow = (DWORD)(node->size);
				fd.nFileSizeHigh = (DWORD)(node->size >> 32);

				wcscpy(fd.cFileName, wfn.c_str());

				fd.cAlternateFileName[0] = 0; // ???

				if(Callback(&fd, DokanFileInfo) != 0)
				{
					break;
				}
			}
		}

		return 0;
	}

	static int WINAPI SetFileAttributes(
		LPCWSTR FileName,
		DWORD FileAttributes, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI SetFileTime(
		LPCWSTR FileName,
		CONST FILETIME* CreationTime,
		CONST FILETIME* LastAccessTime,
		CONST FILETIME* LastWriteTime, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI DeleteFile(
		LPCWSTR FileName, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI DeleteDirectory(
		LPCWSTR FileName, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI MoveFile(
		LPCWSTR FileName, // existing file name
		LPCWSTR NewFileName,
		BOOL ReplaceIfExisting, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI SetEndOfFile(
		LPCWSTR FileName,
		LONGLONG ByteOffset, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI SetAllocationSize(
		LPCWSTR FileName,
		LONGLONG AllocSize, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI LockFile(
		LPCWSTR FileName,
		LONGLONG ByteOffset,
		LONGLONG Length, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI UnlockFile(
		LPCWSTR FileName,
		LONGLONG ByteOffset,
		LONGLONG Length, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s: %s\n", clock(), __FUNCTIONW__, FileName);

		return -ERROR_ACCESS_DENIED;
	}

	static int WINAPI GetDiskFreeSpace(
		PULONGLONG FreeBytesAvailable,
		PULONGLONG TotalNumberOfBytes,
		PULONGLONG TotalNumberOfFreeBytes, 
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s\n", clock(), __FUNCTIONW__);

		uint64_t total = 0;

		for(auto i = ctx->m_pool.m_vdevs.begin(); i != ctx->m_pool.m_vdevs.end(); i++)
		{
			VirtualDevice* vdev = *i;

			if(vdev->type == "raidz")
			{
				size_t count = vdev->children.size();

				if(count > 1)
				{
					total += vdev->asize * (count - vdev->nparity) / count;
				}
			}
			else if(vdev->type == "mirror")
			{
				size_t count = vdev->children.size();

				if(count > 0)
				{
					total += vdev->asize / count;
				}
			}
			else
			{
				total += vdev->asize;
			}
		}

		if(TotalNumberOfBytes != NULL)
		{
			*TotalNumberOfBytes = total;
		}

		if(TotalNumberOfFreeBytes != NULL)
		{
			*TotalNumberOfFreeBytes = total - ctx->m_root->m_dir.used_bytes;
		}

		if(FreeBytesAvailable != NULL)
		{
			*FreeBytesAvailable = total - ctx->m_root->m_dir.used_bytes;
		}

		return 0;
	}

	static int WINAPI GetVolumeInformation(
		LPWSTR VolumeNameBuffer,
		DWORD VolumeNameSize,
		LPDWORD VolumeSerialNumber,
		LPDWORD MaximumComponentLength,
		LPDWORD FileSystemFlags,
		LPWSTR  FileSystemNameBuffer,
		DWORD FileSystemNameSize,
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s\n", clock(), __FUNCTIONW__);

		if(VolumeNameBuffer != NULL)
		{
			wcscpy(VolumeNameBuffer, ctx->m_name.c_str());
		}

		if(VolumeSerialNumber != NULL)
		{
			*VolumeSerialNumber = 0;
		}

		if(MaximumComponentLength != NULL)
		{
			*MaximumComponentLength = 256;
		}

		if(FileSystemFlags != NULL)
		{
			*FileSystemFlags = 
				FILE_CASE_SENSITIVE_SEARCH | 
				FILE_CASE_PRESERVED_NAMES | 
				FILE_UNICODE_ON_DISK |
				FILE_READ_ONLY_VOLUME;
		}

		if(FileSystemNameBuffer != NULL)
		{
			wcscpy(FileSystemNameBuffer, L"ZFS");
		}

		return 0;
	}

	static int WINAPI Unmount(
		DOKAN_FILE_INFO* DokanFileInfo)
	{
		Context* ctx = (Context*)DokanFileInfo->DokanOptions->GlobalContext;

		// wprintf(L"[%d] %s\n", clock(), __FUNCTIONW__);

		return 0;
	}
}

static void usage()
{
	printf(
		"ZFS for Windows\n"
		"\n"
		"usage:\n"
		"  mount <drive> <dataset> <pool ..>\n"
		"  list <pool ..>\n"
		"\n"
		"examples:\n"
		"  zfs-win.exe mount m \"rpool/ROOT/opensolaris\" \"\\\\.\\PhysicalDrive1\" \"\\\\.\\PhysicalDrive2\"\n"
		"  zfs-win.exe list \"Virtual Machine-flat.vmdk\"\n"
		);
}

int _tmain(int argc, _TCHAR* argv[])
{
	if(argc < 2) {usage(); return -1;}

	wchar_t drive = 0;
	std::list<std::wstring> paths;
	std::wstring pool;
	std::wstring  dataset;
	bool list_only = false;

	if(wcsicmp(argv[1], L"mount") == 0)
	{
		if(argc < 5) {usage(); return -1;}

		drive = argv[2][0];

		std::list<std::wstring> sl;

		Util::Explode(std::wstring(argv[3]), sl, L"/");

		pool = sl.front();

		sl.pop_front();

		dataset = Implode(sl, '/');

		for(int i = 4; i < argc; i++)
		{
			paths.push_back(argv[i]);
		}
	}
	else if(wcsicmp(argv[1], L"list") == 0)
	{
		if(argc < 3) {usage(); return -1;}

		for(int i = 2; i < argc; i++)
		{
			paths.push_back(argv[i]);
		}

		list_only = true;
	}
	
	if(paths.empty()) {usage(); return -1;}

	/*

	paths.clear();
	paths.push_back(L"\\\\.\\PhysicalDrive1");
	paths.push_back(L"\\\\.\\PhysicalDrive2");
	paths.push_back(L"\\\\.\\PhysicalDrive3");
	paths.push_back(L"\\\\.\\PhysicalDrive4");

	pool = L"share";
	dataset = L"backup";

	*/

	ZFS::Context ctx;

	if(!ctx.Init(paths, pool))
	{
		return -1;
	}

	if(list_only)
	{
		ctx.List(ctx.m_root);

		return 0;
	}

	if(!ctx.SetDataSet(dataset))
	{
		return -1;
	}

	//

	DOKAN_OPTIONS options;

	memset(&options, 0, sizeof(DOKAN_OPTIONS));

	options.DriveLetter = drive;
	options.GlobalContext = (ULONG64)&ctx;
	options.ThreadCount = 1; // TODO
	options.Options = DOKAN_OPTION_KEEP_ALIVE;

	#ifdef _DEBUG

	options.Options |= DOKAN_OPTION_DEBUG;

	#endif

	DOKAN_OPERATIONS ops;

	memset(&ops, 0, sizeof(DOKAN_OPERATIONS));

	ops.CreateFile = ZFS::CreateFile;
	ops.OpenDirectory = ZFS::OpenDirectory;
	ops.CreateDirectory = ZFS::CreateDirectory;
	ops.Cleanup = ZFS::Cleanup;
	ops.CloseFile = ZFS::CloseFile;
	ops.ReadFile = ZFS::ReadFile;
	ops.WriteFile = ZFS::WriteFile;
	ops.FlushFileBuffers = ZFS::FlushFileBuffers;
	ops.GetFileInformation = ZFS::GetFileInformation;
	ops.FindFiles = ZFS::FindFiles;
	ops.FindFilesWithPattern = ZFS::FindFilesWithPattern;
	ops.SetFileAttributes = ZFS::SetFileAttributes;
	ops.SetFileTime = ZFS::SetFileTime;
	ops.DeleteFile = ZFS::DeleteFile;
	ops.DeleteDirectory = ZFS::DeleteDirectory;
	ops.MoveFile = ZFS::MoveFile;
	ops.SetEndOfFile = ZFS::SetEndOfFile;
	ops.SetAllocationSize = ZFS::SetAllocationSize;
	ops.LockFile = ZFS::LockFile;
	ops.UnlockFile = ZFS::UnlockFile;
	ops.GetDiskFreeSpace = ZFS::GetDiskFreeSpace;
	ops.GetVolumeInformation = ZFS::GetVolumeInformation;
	ops.Unmount = ZFS::Unmount;

	switch(int status = DokanMain(&options, &ops))
	{
	case DOKAN_SUCCESS:
		printf("Success\n");
		break;
	case DOKAN_ERROR:
		printf("Error\n");
		break;
	case DOKAN_DRIVE_LETTER_ERROR:
		printf("Bad Drive letter\n");
		break;
	case DOKAN_DRIVER_INSTALL_ERROR:
		printf("Can't install driver\n");
		break;
	case DOKAN_START_ERROR:
		printf("Driver something wrong\n");
		break;
	case DOKAN_MOUNT_ERROR:
		printf("Can't assign a drive letter\n");
		break;
	default:
		printf("Unknown error: %d\n", status);
		break;
	}

	// std::list<std::wstring> paths;

	/*
	const char* name = "mpool";

	paths.push_back(L"D:\\Virtual Machines\\ZFSVM\\ZFSVM1-flat.vmdk");
	
	paths.push_back(L"D:\\Virtual Machines\\ZFSVM\\ZFSVM2-flat.vmdk");
	paths.push_back(L"D:\\Virtual Machines\\ZFSVM\\ZFSVM3-flat.vmdk");
	paths.push_back(L"D:\\Virtual Machines\\ZFSVM\\ZFSVM4-flat.vmdk");
	paths.push_back(L"D:\\Virtual Machines\\ZFSVM\\ZFSVM5-flat.vmdk");
	paths.push_back(L"D:\\Virtual Machines\\ZFSVM\\ZFSVM6-flat.vmdk");
	paths.push_back(L"D:\\Virtual Machines\\ZFSVM\\ZFSVM7-flat.vmdk");
	paths.push_back(L"D:\\Virtual Machines\\ZFSVM\\ZFSVM8-flat.vmdk");
	*/

	/*
	const char* name = "share";

	paths.push_back(L"\\\\.\\PhysicalDrive1");
	paths.push_back(L"\\\\.\\PhysicalDrive2");
	paths.push_back(L"\\\\.\\PhysicalDrive3");
	paths.push_back(L"\\\\.\\PhysicalDrive4");
	*/

	/*
	const char* name = "rpool";

	paths.push_back(L"D:\\Virtual Machines\\OpenSolaris\\OpenSolaris-flat.vmdk");	
	*/

	/*
	const char* name = "rpool";

	paths.push_back(L"D:\\Virtual Machines\\ZFS1VM\\ZFS1VM-flat.vmdk");
	*/

	return 0;
}

