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
#include <time.h>

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
	
	ZFS::Pool pool;

	if(!pool.Open(name, paths))
	{
		return -1;
	}

	ZFS::Device* dev = pool.m_devs.front();

	ZFS::ObjectSet os(&pool);

	if(os.Init(&dev->m_active->rootbp, 1))
	{
		ZFS::DataSet ds(&pool);
		
		if(ds.Init(os))
		{
			std::list<ZFS::DataSet*> mpl;

			ds.GetMountPoints(mpl);

			for(auto i = mpl.begin(); i != mpl.end(); i++)
			{
				if((*i)->m_mountpoint != "/") continue;

				ZFS::ObjectSet os(&pool);

				if(os.Init(&(*i)->m_dataset.bp, 1))
				{
					dnode_phys_t root;
					
					if(os.Read("ROOT", &root, DMU_OT_DIRECTORY_CONTENTS))
					{
						znode_phys_t* node = (znode_phys_t*)root.bonus();

						ZFS::ZapObject zap(&pool);

						if(zap.Init(root.blkptr, root.nblkptr))
						{
							int i = 0;
						}
					}
				}
			}
		}
	}

	return 0;
}

