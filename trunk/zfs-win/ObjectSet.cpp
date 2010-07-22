#include "StdAfx.h"
#include "ObjectSet.h"

namespace ZFS
{
	ObjectSet::ObjectSet()
		: m_dnode_count(0)
	{
	}

	ObjectSet::~ObjectSet()
	{
	}

	bool ObjectSet::Read(Pool& pool, blkptr_t* bp, size_t count)
	{
		ASSERT(bp->type == DMU_OT_OBJSET);
		
		m_objset.clear();
		m_dnode.clear();
		m_dnode_count = 0;

		if(!pool.Read(m_objset, bp, count))
		{
			return false;
		}

		objset_phys_t* os = (objset_phys_t*)m_objset.data();

		if(os->meta_dnode.type != DMU_OT_DNODE)
		{
			return false;
		}

		if(!pool.Read(m_dnode, os->meta_dnode.blkptr, os->meta_dnode.nblkptr))
		{
			return false;
		}

		m_dnode_count = m_dnode.size() / sizeof(dnode_phys_t);

		if(os->type == DMU_OST_META || os->type == DMU_OST_ZFS)
		{
			if(m_dnode_count < 2) 
			{
				return false;
			}

			dnode_phys_t* dn = (*this)[1];

			if(os->type == DMU_OST_META && dn->type != DMU_OT_OBJECT_DIRECTORY
			|| os->type == DMU_OST_ZFS && dn->type != DMU_OT_MASTER_NODE)
			{
				return false;
			}

			if(!m_objdir.Read(pool, dn->blkptr, dn->nblkptr))
			{
				return false;
			}
		}

		return true;
	}

	objset_phys_t* ObjectSet::operator -> ()
	{
		ASSERT(m_objset.size() >= sizeof(objset_phys_t)); 
		
		return (objset_phys_t*)m_objset.data();
	}

	dnode_phys_t* ObjectSet::operator [] (size_t index) 
	{
		ASSERT(index < m_dnode_count); 
		
		return (dnode_phys_t*)m_dnode.data() + index;
	}

	dnode_phys_t* ObjectSet::operator [] (const char* s) 
	{
		uint64_t index;

		if(m_objdir.Lookup(s, index))
		{
			size_t i = (size_t)index;

			if(i < m_dnode_count)
			{
				return (*this)[i];
			}
		}

		return NULL;
	}

	size_t ObjectSet::count() 
	{
		return m_dnode_count;
	}
}