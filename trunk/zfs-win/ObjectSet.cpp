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
#include "ObjectSet.h"

namespace ZFS
{
	ObjectSet::ObjectSet(Pool* pool)
		: m_pool(pool)
		, m_objdir(pool)
		, m_reader(NULL)
		, m_count(0)
	{
	}

	ObjectSet::~ObjectSet()
	{
		delete m_reader;
	}

	bool ObjectSet::Init(blkptr_t* bp)
	{
		ASSERT(bp->type == DMU_OT_OBJSET);
		
		{
			delete m_reader;

			m_objset.clear();
			m_reader = NULL;
			m_count = 0;
		}

		ASSERT(bp->lvl == 0); // must not be indirect

		if(!m_pool->Read(m_objset, bp))
		{
			return false;
		}

		objset_phys_t* os = (objset_phys_t*)m_objset.data();

		if(os->meta_dnode.type != DMU_OT_DNODE)
		{
			return false;
		}

		m_reader = new BlockReader(m_pool, &os->meta_dnode);

		m_count = (size_t)(m_reader->GetDataSize() / sizeof(dnode_phys_t));

		if(os->type == DMU_OST_META || os->type == DMU_OST_ZFS)
		{
			if(!Read(1, m_objdir, os->type == DMU_OST_META ? DMU_OT_OBJECT_DIRECTORY : DMU_OT_MASTER_NODE))
			{
				return false;
			}
		}

		return true;
	}

	size_t ObjectSet::GetIndex(const char* name)
	{
		uint64_t index;

		if(!m_objdir.Lookup(name, index))
		{
			index = -1;
		}

		return (size_t)index;
	}

	bool ObjectSet::Read(size_t index, dnode_phys_t* dn, dmu_object_type type)
	{
		if(index >= m_count || dn == NULL) 
		{	
			return false;
		}

		size_t size = sizeof(dnode_phys_t);

		if(m_reader->Read(dn, size, (uint64_t)index * sizeof(dnode_phys_t)) != size)
		{
			return false;
		}

		return type == DMU_OT_NONE || dn->type == type;
	}

	bool ObjectSet::Read(size_t index, ZapObject& zap, dmu_object_type type)
	{
		dnode_phys_t dn;

		if(!Read(index, &dn, type))
		{
			return false;
		}

		if(!zap.Init(&dn))
		{
			return false;
		}

		return true;
	}

	bool ObjectSet::Read(size_t index, NameValueList& nvl)
	{
		dnode_phys_t dn;

		if(!Read(index, &dn, DMU_OT_PACKED_NVLIST))
		{
			return false;
		}

		BlockReader r(m_pool, &dn);

		size_t size = (size_t)r.GetDataSize();

		std::vector<uint8_t> buff(size);

		if(r.Read(buff.data(), size, 0) != size)
		{
			return false;
		}

		if(!nvl.Init(buff.data(), buff.size()))
		{
			return false;
		}

		return true;
	}
}