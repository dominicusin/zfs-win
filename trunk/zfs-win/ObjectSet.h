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
#include "NameValueList.h"
#include "ZapObject.h"
#include "BlockReader.h"

namespace ZFS
{
	class ObjectSet
	{
		Pool* m_pool;
		std::vector<uint8_t> m_objset;
		std::map<uint64_t, ZapObject*> m_objdir;
		std::map<uint64_t, dnode_phys_t> m_cache; // TODO: only remember the last few nodes
		BlockReader* m_reader;
		uint64_t m_count;

		void RemoveAll();

	public:
		ObjectSet(Pool* pool);
		virtual ~ObjectSet();

		bool Init(blkptr_t* bp);

		uint64_t GetCount() {return m_count;}
		uint64_t GetIndex(const char* name, uint64_t parent_index);

		bool Read(uint64_t index, dnode_phys_t* dn, dmu_object_type type = DMU_OT_NONE);
		bool Read(uint64_t index, ZapObject** zap, dmu_object_type type = DMU_OT_NONE);
		bool Read(uint64_t index, NameValueList& nvl);

		objset_phys_t* operator -> () {return (objset_phys_t*)m_objset.data();}
	};
}
