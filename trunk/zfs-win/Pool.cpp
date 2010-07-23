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
#include "Pool.h"
#include "BlockReader.h"

namespace ZFS
{
	Pool::Pool()
		: m_guid(0)
	{
	}

	Pool::~Pool()
	{
		Close();
	}

	bool Pool::Open(const char* name, const std::list<std::wstring>& paths)
	{
		Close();

		m_name = name;

		for(auto i = paths.begin(); i != paths.end(); i++)
		{
			Device* dev = new Device();

			if(!dev->Open(i->c_str()))
			{
				return false;
			}

			if(m_name == dev->m_desc.pool.name && (m_guid == 0 || m_guid == dev->m_desc.pool.guid))
			{
				m_guid = dev->m_desc.pool.guid;

				m_devs.push_back(dev);

				auto cmp_tree = [&] (const VirtualDevice* vdev) -> bool {return vdev->guid == dev->m_desc.top.guid;};

				if(std::find_if(m_vdevs.begin(), m_vdevs.end(), cmp_tree) == m_vdevs.end())
				{
					m_vdevs.push_back(&dev->m_desc.top);
				}
			}
			else
			{
				delete dev;
			}
		}

		if(m_devs.empty())
		{
			return false;
		}

		for(auto i = m_vdevs.begin(); i != m_vdevs.end(); i++)
		{
			std::list<VirtualDevice*> leaves;

			VirtualDevice* vdev = *i;

			vdev->GetLeaves(leaves);

			for(auto j = leaves.begin(); j != leaves.end(); j++)
			{
				VirtualDevice* leaf = *j;

				for(auto k = m_devs.begin(); k != m_devs.end(); k++)
				{
					Device* dev = *k;

					if(leaf->guid == dev->m_desc.guid)
					{
						ASSERT(vdev->guid == dev->m_desc.top_guid);

						leaf->dev = dev;

						break;
					}
				}

				if(leaf->dev == NULL)
				{
					return false;
				}
			}
		}

		return true;
	}

	void Pool::Close()
	{
		for(auto i = m_devs.begin(); i != m_devs.end(); i++)
		{
			delete *i;
		}

		m_guid = 0;
		m_name.clear();
		m_devs.clear();
		m_vdevs.clear();
	}

	bool Pool::Read(std::vector<uint8_t>& buff, blkptr_t* bp, size_t count)
	{
		BlockStream r(this, bp, count);

		return r.ReadToEnd(buff);
	}
}
