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
	DataSet::DataSet()
		: m_dir(NULL)
		, m_dataset(NULL)
	{
	}

	DataSet::~DataSet()
	{
		for(auto i = m_children.begin(); i != m_children.end(); i++)
		{
			delete *i;
		}
	}

	bool DataSet::Read(Pool& pool, ObjectSet& os, const char* name, dnode_phys_t* root_dataset)
	{
		m_name = name != NULL ? name : pool.m_name.c_str();

		if(root_dataset == NULL)
		{
			root_dataset = os["root_dataset"];
		}

		if(root_dataset == NULL || root_dataset->type != DMU_OT_DSL_DIR)
		{
			return false;
		}

		m_dir = (dsl_dir_phys_t*)root_dataset->bonus();

		dnode_phys_t* dn = os[(size_t)m_dir->head_dataset_obj];

		if(dn == NULL || dn->type != DMU_OT_DSL_DATASET)
		{
			return false;
		}

		m_dataset = (dsl_dataset_phys_t*)dn->bonus();

		dn = os[(size_t)m_dir->props_zapobj];
				
		if(dn != NULL && dn->type == DMU_OT_DSL_PROPS)
		{
			ZFS::ZapObject zap;

			if(zap.Read(pool, dn->blkptr, dn->nblkptr))
			{
				zap.Lookup("mountpoint", m_mountpoint);
			}
		}

		dn = os[(size_t)m_dir->child_dir_zapobj];

		if(dn != NULL && dn->type == DMU_OT_DSL_DIR_CHILD_MAP)
		{
			ZFS::ZapObject zap;

			if(zap.Read(pool, dn->blkptr, dn->nblkptr))
			{
				for(auto i = zap.begin(); i != zap.end(); i++)
				{
					uint64_t index;

					if(zap.Lookup(i->first.c_str(), index))
					{
						DataSet* ds = new DataSet();

						if(ds->Read(pool, os, i->first.c_str(), os[(size_t)index]))
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