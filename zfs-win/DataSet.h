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

#pragma once

#include "Pool.h"
#include "ObjectSet.h"

namespace ZFS
{
	class DataSet
	{
		Pool* m_pool;
		std::map<std::wstring, dnode_phys_t> m_cache;

		bool Init(ObjectSet& os, const char* name = NULL, uint64_t root_index = -1);
		void RemoveAll();
		void SetDefaults(DataSet* parent);

	public:
		dsl_dir_phys_t m_dir;
		dsl_dataset_phys_t m_dataset;
		std::string m_name;
		std::string m_mountpoint;
		std::list<DataSet*> m_children;
		ObjectSet* m_head;

	public:
		DataSet(Pool* pool);
		virtual ~DataSet();

		bool Init(blkptr_t* bp);
		void GetMountPoints(std::list<DataSet*>& mpl);
		bool Find(const wchar_t* path, DataSet** ds);
		bool Find(const wchar_t* path, dnode_phys_t& dn);
		void Test(uint64_t index = 0, const char* path = "");
	};
}