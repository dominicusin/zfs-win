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
#include "String.h"

namespace ZFS
{
	DataSet::DataSet(Pool* pool)
		: m_pool(pool)
		, m_head(NULL)
	{
	}

	DataSet::~DataSet()
	{
		RemoveAll();
	}

	bool DataSet::Init(ObjectSet& os, const char* name, uint64_t root_index)
	{
		RemoveAll();

		m_name = name;

		dnode_phys_t dn;
		
		if(root_index == -1)
		{
			if(!os.Read(os.GetIndex("root_dataset", 1), &dn, DMU_OT_DSL_DIR))
			{
				return false;
			}

			NameValueList nvl;

			if(os.Read(os.GetIndex("config", 1), nvl))
			{
				// TODO: root vdev config
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

		if(!os.Read(m_dir.head_dataset_obj, &dn, DMU_OT_DSL_DATASET))
		{
			return false;
		}

		m_dataset = *(dsl_dataset_phys_t*)dn.bonus();

		if(m_dataset.bp.type == DMU_OT_OBJSET)
		{
			m_head = new ObjectSet(m_pool);

			if(!m_head->Init(&m_dataset.bp))
			{
				return false;
			}
		}

		ZapObject* zap = NULL;

		if(os.Read(m_dir.props_zapobj, &zap, DMU_OT_DSL_PROPS))
		{
			zap->Lookup("mountpoint", m_mountpoint);
		}

		if(os.Read(m_dir.child_dir_zapobj, &zap, DMU_OT_DSL_DIR_CHILD_MAP))
		{
			for(auto i = zap->begin(); i != zap->end(); i++)
			{
				uint64_t index;

				if(zap->Lookup(i->first.c_str(), index))
				{
					DataSet* ds = new DataSet(m_pool);

					if(ds->Init(os, i->first.c_str(), index))
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

		return true;
	}

	void DataSet::RemoveAll()
	{
		for(auto i = m_children.begin(); i != m_children.end(); i++)
		{
			delete *i;
		}

		if(m_head != NULL)
		{
			delete m_head;

			m_head = NULL;
		}

		m_cache.clear();
	}

	void DataSet::SetDefaults(DataSet* parent)
	{
		// TODO

		for(auto i = m_children.begin(); i != m_children.end(); i++)
		{
			(*i)->SetDefaults(this);
		}
	}
	
	bool DataSet::Init(blkptr_t* bp)
	{
		ObjectSet os(m_pool);

		if(!os.Init(bp))
		{
			return false;
		}

		if(!Init(os, m_pool->m_name.c_str()))
		{
			return false;
		}

		SetDefaults(NULL);

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

	bool DataSet::Find(const wchar_t* path, DataSet** ds)
	{
		std::wstring s(path);

		DataSet* tmp[2] = {this, NULL};

		if(!s.empty())
		{
			std::list<std::wstring> sl;

			Util::Explode(s, sl, L"/");

			while(!sl.empty())
			{
				std::string s = Util::UTF16To8(sl.front().c_str());

				sl.pop_front();

				tmp[1] = NULL;

				for(auto i = tmp[0]->m_children.begin(); i != tmp[0]->m_children.end(); i++)
				{
					if((*i)->m_name == s)
					{
						tmp[1] = *i;

						break;
					}
				}

				if(tmp[1] == NULL) 
				{
					return false;
				}

				tmp[0] = tmp[1];
			}
		}

		*ds = tmp[0];

		return true;
	}

	bool DataSet::Find(const wchar_t* path, dnode_phys_t& dn)
	{
		if(m_head == NULL)
		{
			return false;
		}

		std::wstring s = path;

		for(size_t i = 0; i < s.size(); i++)
		{
			if(s[i] == '\\') s[i] = '/';
		}

		if(s[0] != '/')
		{
			return false;
		}

		std::list<std::wstring> sl;

		Util::Explode(Util::TrimRight(L"ROOT" + s, L"/"), sl, L"/");

		uint64_t index = 1;

		while(!sl.empty())
		{
			std::string name = Util::UTF16To8(sl.front().c_str());

			sl.pop_front();

			index = m_head->GetIndex(name.c_str(), index);

			index = ZFS_DIRENT_OBJ(index);

			if(!m_head->Read(index, &dn))
			{
				return false;
			}

			if(dn.type != DMU_OT_DIRECTORY_CONTENTS && dn.type != DMU_OT_PLAIN_FILE_CONTENTS)
			{
				return false;
			}
		}

		return true;
	}
}