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
#include "DataSet.h"

namespace ZFS
{
	DataSet::DataSet(Pool* pool)
		: m_pool(pool)
	{
	}

	DataSet::~DataSet()
	{
		for(auto i = m_children.begin(); i != m_children.end(); i++)
		{
			delete *i;
		}
	}

	bool DataSet::Init(ObjectSet& os, const char* name, size_t root_index)
	{
		m_name = name != NULL ? name : m_pool->m_name.c_str();

		dnode_phys_t dn;
		
		if(root_index == -1)
		{
			if(!os.Read("root_dataset", &dn, DMU_OT_DSL_DIR))
			{
				return false;
			}
		}
		else
		{
			if(!os.Read(root_index, &dn, DMU_OT_DSL_DIR))
			{
				return false;
			}
		}

		m_dir = *(dsl_dir_phys_t*)dn.bonus();

		if(!os.Read((size_t)m_dir.head_dataset_obj, &dn, DMU_OT_DSL_DATASET))
		{
			return false;
		}

		m_dataset = *(dsl_dataset_phys_t*)dn.bonus();

		if(os.Read((size_t)m_dir.props_zapobj, &dn, DMU_OT_DSL_PROPS))
		{
			ZFS::ZapObject zap(m_pool);

			if(zap.Init(dn.blkptr, dn.nblkptr))
			{
				zap.Lookup("mountpoint", m_mountpoint);
			}
		}

		if(os.Read((size_t)m_dir.child_dir_zapobj, &dn, DMU_OT_DSL_DIR_CHILD_MAP))
		{
			ZFS::ZapObject zap(m_pool);

			if(zap.Init(dn.blkptr, dn.nblkptr))
			{
				for(auto i = zap.begin(); i != zap.end(); i++)
				{
					uint64_t index;

					if(zap.Lookup(i->first.c_str(), index))
					{
						DataSet* ds = new DataSet(m_pool);

						if(ds->Init(os, i->first.c_str(), (size_t)index))
						{
							m_children.push_back(ds);
						}
						else
						{
							delete ds;
						}
					}
				}
			}
		}

		return true;
	}

	void DataSet::GetMountPoints(std::list<DataSet*>& mpl)
	{
		if(!m_mountpoint.empty())
		{
			mpl.push_back(this);
		}

		for(auto i = m_children.begin(); i != m_children.end(); i++)
		{
			(*i)->GetMountPoints(mpl);
		}
	}
}