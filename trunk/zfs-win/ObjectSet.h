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
#include "ZapObject.h"
#include "BlockReader.h"

namespace ZFS
{
	class ObjectSet
	{
		Pool* m_pool;
		std::vector<uint8_t> m_objset;
		ZapObject m_objdir;
		BlockReader* m_reader;
		size_t m_count;

	public:
		ObjectSet(Pool* pool);
		virtual ~ObjectSet();

		bool Init(blkptr_t* bp);
		size_t GetCount() {return m_count;}
		bool Read(size_t index, dnode_phys_t* dn, dmu_object_type type = DMU_OT_NONE);
		bool Read(const char* name, dnode_phys_t* dn, dmu_object_type type = DMU_OT_NONE);

		objset_phys_t* operator -> () {return (objset_phys_t*)m_objset.data();}
	};
}
