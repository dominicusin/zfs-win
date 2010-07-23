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
		, m_dnode_reader(NULL)
		, m_dnode_count(0)
	{
	}

	ObjectSet::~ObjectSet()
	{
		delete m_dnode_reader;
	}

	bool ObjectSet::Init(blkptr_t* bp, size_t count)
	{
		ASSERT(bp->type == DMU_OT_OBJSET);
		
		{
			m_objset.clear();

			if(m_dnode_reader != NULL)
			{
				delete m_dnode_reader;

				m_dnode_reader = NULL;
			}

			m_dnode_count = 0;
		}

		if(!m_pool->Read(m_objset, bp, count))
		{
			return false;
		}

		objset_phys_t* os = (objset_phys_t*)m_objset.data();

		if(os->meta_dnode.type != DMU_OT_DNODE)
		{
			return false;
		}

		m_dnode_reader = new BlockFile(m_pool, os->meta_dnode.blkptr, os->meta_dnode.nblkptr);

		m_dnode_count = (size_t)(m_dnode_reader->GetLogicalSize() / sizeof(dnode_phys_t));

		if(os->type == DMU_OST_META || os->type == DMU_OST_ZFS)
		{
			dnode_phys_t dn;

			if(!Read(1, &dn, os->type == DMU_OST_META ? DMU_OT_OBJECT_DIRECTORY : DMU_OT_MASTER_NODE))
			{
				return false;
			}

			if(!m_objdir.Init(dn.blkptr, dn.nblkptr))
			{
				return false;
			}
		}

		return true;
	}

	bool ObjectSet::Read(size_t index, dnode_phys_t* dn, dmu_object_type type)
	{
		if(index >= m_dnode_count || dn == NULL) 
		{	
			return false;
		}

		if(!m_dnode_reader->Read(dn, sizeof(dnode_phys_t), sizeof(dnode_phys_t) * index))
		{
			return false;
		}

		return type == DMU_OT_NONE || dn->type == type;
	}
	
	bool ObjectSet::Read(const char* name, dnode_phys_t* dn, dmu_object_type type)
	{
		uint64_t index;

		if(!m_objdir.Lookup(name, index))
		{
			return false;
		}

		return Read((size_t)index, dn, type);
	}
}