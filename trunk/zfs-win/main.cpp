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
#include "Device.h"
#include "ZapObject.h"

int _tmain(int argc, _TCHAR* argv[])
{
	// this is just a test, recreating the steps of "ZFS On-Disk Data Walk (Or: Where's My Data)" (google for it)

	std::list<std::wstring> paths;
	/*
	for(int i = 2; i < argc; i++)
	{
		paths.push_back(argv[i]);
	}
	*/
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
	/**/
	const char* name = "rpool";

	paths.push_back(L"D:\\Virtual Machines\\OpenSolaris\\OpenSolaris-flat.vmdk");	
	
	ZFS::Pool p;

	if(!p.Open(name, paths))
	{
		return -1;
	}

	ZFS::Device* dev = p.m_devs.front();

	ZFS::ObjectSet os;

	if(p.Read(os, &dev->m_active->rootbp, 1))
	{
		ASSERT(os.count() > 2);

		dnode_phys_t* root_dataset = NULL;

		ASSERT(os[1]->type == DMU_OT_OBJECT_DIRECTORY);

		if(os[1]->type == DMU_OT_OBJECT_DIRECTORY)
		{
			ZFS::ZapObject zap;

			if(p.Read(zap, os[1]->blkptr, os[1]->nblkptr))
			{
				uint64_t index;

				if(zap.Lookup("root_dataset", index))
				{
					if(index < os.count() && os[index]->type == DMU_OT_DSL_DIR)
					{
						root_dataset = os[index];
					}
				}
			}
		}

		dnode_phys_t* head_dataset = NULL;

		if(root_dataset != NULL)
		{
			dsl_dir_phys_t* dir = (dsl_dir_phys_t*)root_dataset->bonus;

			size_t index = (size_t)dir->head_dataset_obj;

			if(index < os.count() && os[index]->type == DMU_OT_DSL_DATASET)
			{
				head_dataset = os[index];
			}
		}

		if(head_dataset != NULL)
		{
			dsl_dataset_phys_t* ds = (dsl_dataset_phys_t*)head_dataset->bonus;

			if(ds->bp.type == DMU_OT_OBJSET)
			{
				ZFS::ObjectSet os;

				if(p.Read(os, &ds->bp, 1))
				{
					dnode_phys_t* root = NULL;

					ASSERT(os[1]->type == DMU_OT_MASTER_NODE);

					if(os[1]->type == DMU_OT_MASTER_NODE)
					{
						ZFS::ZapObject zap;

						if(p.Read(zap, os[1]->blkptr, os[1]->nblkptr))
						{
							uint64_t index;

							if(zap.Lookup("ROOT", index)) // NOTE: the ROOT dataset may not contain too many files, don't be surprised
							{
								if(index < os.count() && os[index]->type == DMU_OT_DIRECTORY_CONTENTS)
								{
									root = os[index];
								}
								else
								{
									ASSERT(0);
								}
							}
						}
					}

					if(root != NULL)
					{
						znode_phys_t* node = (znode_phys_t*)root->bonus;

						std::vector<uint8_t> buff;

						if(p.Read(buff, root->blkptr, root->nblkptr))
						{
							mzap_phys_t* mzap = (mzap_phys_t*)buff.data();

							// finally, arrived at the root directory

							int i = 0;
						}
					}
				}
			}
		}
	}

	return 0;
}

