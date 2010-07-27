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

	bool Pool::Read(std::vector<uint8_t>& dst, blkptr_t* bp)
	{
		for(int i = 0; i < 3; i++)
		{
			dva_t* addr = &bp->blk_dva[i];

			ASSERT(addr->gang == 0); // TODO: zio_gbh_phys_t (not used ??? never encountered one, yet)

			for(auto i = m_vdevs.begin(); i != m_vdevs.end(); i++)
			{
				VirtualDevice* vdev = *i;

				if(vdev->id != addr->vdev)
				{
					continue;
				}

				// << 9 or vdev->ashift? 
				//
				// on-disk spec says: "All sizes are stored as the number of 512 byte sectors (minus one) needed to
				// represent the size of this block.", but is it still true or outdated?

				size_t psize = ((size_t)bp->psize + 1) << 9;
				size_t lsize = ((size_t)bp->lsize + 1) << 9;

				std::vector<uint8_t> src(psize);

				if(!vdev->Read(src, psize, addr->offset << 9))
				{
					continue;
				}

				if(Verify(src, bp->cksum_type, bp->cksum))
				{
					if(Decompress(src, dst, lsize, bp->comp_type))
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	bool Pool::Verify(std::vector<uint8_t>& buff, uint8_t cksum_type, cksum_t& cksum)
	{
		cksum_t c;

		memset(&c, 0, sizeof(c));

		switch(cksum_type)
		{
		case ZIO_CHECKSUM_OFF:
			return true;
		case ZIO_CHECKSUM_ON: // ???
		case ZIO_CHECKSUM_ZILOG:
		case ZIO_CHECKSUM_FLETCHER_2:
			fletcher_2_native(buff.data(), buff.size(), &c);
			break;
		case ZIO_CHECKSUM_ZILOG2:
		case ZIO_CHECKSUM_FLETCHER_4:
			fletcher_4_native(buff.data(), buff.size(), &c);
			break;
		case ZIO_CHECKSUM_LABEL:
		case ZIO_CHECKSUM_GANG_HEADER:
		case ZIO_CHECKSUM_SHA256:
			sha256(buff.data(), buff.size(), &c); // TESTME
			break;
		default:
			ASSERT(0);
			return false;
		}

		ASSERT(cksum == c);

		return cksum == c;
	}

	bool Pool::Decompress(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, size_t lsize, uint8_t comp_type)
	{
		switch(comp_type)
		{
		case ZIO_COMPRESS_ON: // ???
		case ZIO_COMPRESS_LZJB:
			dst.resize(lsize);
			lzjb_decompress(src.data(), dst.data(), src.size(), lsize);
			break;
		case ZIO_COMPRESS_OFF:
		case ZIO_COMPRESS_EMPTY: // ???
			dst.swap(src);
			break;
		case ZIO_COMPRESS_GZIP_1:
		case ZIO_COMPRESS_GZIP_2:
		case ZIO_COMPRESS_GZIP_3:
		case ZIO_COMPRESS_GZIP_4:
		case ZIO_COMPRESS_GZIP_5:
		case ZIO_COMPRESS_GZIP_6:
		case ZIO_COMPRESS_GZIP_7:
		case ZIO_COMPRESS_GZIP_8:
		case ZIO_COMPRESS_GZIP_9:
			gzip_decompress(src.data(), dst.data(), src.size(), lsize); // TESTME
			break;
		case ZIO_COMPRESS_ZLE:
			zle_decompress(src.data(), dst.data(), src.size(), lsize, 64); // TESTME
			break;
		default:
			ASSERT(0);
			return false;
		}

		return true;
	}
}
