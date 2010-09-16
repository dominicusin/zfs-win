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
#include "Hash.h"
#include "Compress.h"
#include "String.h"

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

	bool Pool::Open(const std::list<std::wstring>& paths, const wchar_t* name)
	{
		Close();

		if(name != NULL)
		{
			m_name = Util::UTF16To8(name);
		}

		for(auto i = paths.begin(); i != paths.end(); i++)
		{
			Device* dev = new Device();

			if(!dev->Open(i->c_str()))
			{
				return false;
			}

			if(m_name.empty())
			{
				m_name = dev->m_desc.pool.name;
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

			uint32_t missing = 0;

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
					missing++;
				}
			}

			if(vdev->type == "raidz" && missing <= vdev->nparity
			|| vdev->type == "mirror" && missing < vdev->children.size()
			|| missing == 0)
			{
				if(missing > 0)
				{
					wprintf(L"WARNING: vdev %I64d has %d missing disk(s)\n", vdev->id, missing);
				}
			}
			else
			{
				wprintf(L"ERROR: vdev %I64d has too many (%d) missing disk(s)\n", vdev->id, missing);

				return false;
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

	bool Pool::Read(uint8_t* dst, size_t size, blkptr_t* bp)
	{
		ASSERT(((UINT_PTR)dst & 15) == 0);

		bool succeeded = false;

		size_t psize = ((size_t)bp->psize + 1) << 9;
		size_t lsize = ((size_t)bp->lsize + 1) << 9;

		if(size < lsize) return false;

		uint8_t* src = bp->comp_type != ZIO_COMPRESS_OFF ? (uint8_t*)_aligned_malloc(psize, 16) : NULL;

		for(int i = 0; i < 3 && !succeeded; i++)
		{
			dva_t* addr = &bp->blk_dva[i];

			ASSERT(addr->gang == 0); // TODO: zio_gbh_phys_t (not used ??? never encountered one, yet)

			for(auto i = m_vdevs.begin(); i != m_vdevs.end(); i++)
			{
				VirtualDevice* vdev = *i;

				if(vdev->id == addr->vdev)
				{
					BYTE* ptr = src != NULL ? src : dst;

					if(vdev->Read(ptr, psize, addr->offset << 9))
					{
						if(Verify(ptr, psize, bp->cksum_type, bp->cksum))
						{
							if(ptr != src || ZFS::decompress(ptr, dst, psize, lsize, bp->comp_type))
							{
								succeeded = true;
							}
						}
						else
						{
							printf("cksum error (vdev=%I64d offset=%I64d)\n", vdev->id, addr->offset << 9);
						}
					}
					else
					{
						printf("cannot read device (vdev=%I64d offset=%I64d)\n", vdev->id, addr->offset << 9);
					}

					break;
				}
			}
		}

		if(src != NULL) _aligned_free(src);

		return succeeded;
	}

	bool Pool::Verify(uint8_t* buff, size_t size, uint8_t cksum_type, cksum_t& cksum)
	{
		cksum_t c;

		ZFS::hash(buff, size, &c, cksum_type);

		// ASSERT(cksum == c);

		return cksum == c;
	}
}
