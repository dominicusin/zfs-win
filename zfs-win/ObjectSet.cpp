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
		, m_reader(NULL)
		, m_count(0)
	{
	}

	ObjectSet::~ObjectSet()
	{
		RemoveAll();
	}

	void ObjectSet::RemoveAll()
	{
		for(auto i = m_objdir.begin(); i != m_objdir.end(); i++)
		{
			delete i->second;
		}

		m_objdir.clear();

		m_cache.clear();

		delete m_reader;

		m_reader = NULL;

		m_count = 0;
	}

	bool ObjectSet::Init(blkptr_t* bp)
	{
		ASSERT(bp->type == DMU_OT_OBJSET);
		
		RemoveAll();

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

		m_count = m_reader->GetDataSize() / sizeof(dnode_phys_t);

		return true;
	}

	uint64_t ObjectSet::GetIndex(const char* name, uint64_t parent_index)
	{
		ZapObject* zap = NULL;

		if(!Read(parent_index, &zap))
		{
			return -1;
		}

		uint64_t index;

		if(!zap->Lookup(name, index))
		{
			return -1;
		}

		return index;
	}

	bool ObjectSet::Read(uint64_t index, dnode_phys_t* dn, dmu_object_type type)
	{
		ASSERT(index == -1 || index < UINT_MAX);

		if(index >= m_count || dn == NULL) 
		{	
			return false;
		}

		auto i = m_cache.find(index);

		if(i != m_cache.end())
		{
			*dn = i->second;
		}
		else
		{
			size_t size = sizeof(dnode_phys_t);

			if(m_reader->Read(dn, size, index * size) != size)
			{
				return false;
			}

			dn->pad3[0] = index;
		
			if(dn->type != DMU_OT_PLAIN_FILE_CONTENTS)
			{
				m_cache[index] = *dn;
			}
		}

		return type == DMU_OT_NONE || dn->type == type;
	}

	bool ObjectSet::Read(uint64_t index, ZapObject** zap, dmu_object_type type)
	{
		auto i = m_objdir.find(index);

		if(i == m_objdir.end())
		{
			dnode_phys_t dn;

			if(!Read(index, &dn, type))
			{
				return false;
			}

			*zap = new ZapObject(m_pool);

			if(!(*zap)->Init(&dn))
			{
				delete *zap;

				return false;
			}

			m_objdir[index] = *zap;
		}
		else
		{
			*zap = i->second;
		}

		return true;
	}

	bool ObjectSet::Read(uint64_t index, NameValueList& nvl)
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