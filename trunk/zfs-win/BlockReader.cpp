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
#include "BlockReader.h"
#include "Compress.h"

namespace ZFS
{
	BlockReader::BlockReader(Pool* pool, blkptr_t* bp, size_t count)
		: m_pool(pool)
	{
		Insert(bp, count);
	}

	BlockReader::~BlockReader()
	{
		for(auto i = m_bpl.begin(); i != m_bpl.end(); i++)
		{
			delete *i;
		}
	}

	bool BlockReader::ReadNext(std::vector<uint8_t>& buff)
	{
		if(m_bpl.empty())
		{
			return false;
		}

		std::auto_ptr<blkptr_t> bp(m_bpl.front());

		m_bpl.pop_front();

		while(bp->lvl > 0)
		{
			if(!Read(bp.get(), buff))
			{
				return false;
			}

			Insert((blkptr_t*)buff.data(), buff.size() / sizeof(blkptr_t));

			bp = std::auto_ptr<blkptr_t>(m_bpl.front());

			m_bpl.pop_front();
		}
		
		return Read(bp.get(), buff);
	}

	bool BlockReader::ReadToEnd(std::vector<uint8_t>& dst)
	{
		dst.clear();

		// TODO: resize/reserve (how much?)

		size_t i = 0;

		std::vector<uint8_t> buff;

		while(ReadNext(buff))
		{
			if(i + buff.size() > dst.size())
			{
				dst.resize(i + buff.size());
			}

			memcpy(dst.data() + i, buff.data(), buff.size());

			i += buff.size();
		}

		return true;
	}

	void BlockReader::Insert(blkptr_t* bp, size_t count)
	{
		if(m_bpl.empty())
		{
			for(size_t i = 0; i < count && bp[i].type != DMU_OT_NONE; i++)
			{
				m_bpl.push_back(new blkptr_t(bp[i]));
			}
		}
		else
		{
			std::list<blkptr_t*> l;

			for(size_t i = 0; i < count && bp[i].type != DMU_OT_NONE; i++)
			{
				l.push_back(new blkptr_t(bp[i]));
			}

			m_bpl.insert(m_bpl.begin(), l.begin(), l.end());
		}
	}

	bool BlockReader::Read(blkptr_t* bp, std::vector<uint8_t>& dst)
	{
		for(int i = 0; i < 3; i++)
		{
			dva_t* addr = &bp->blk_dva[i];

			ASSERT(addr->gang == 0); // TODO: zio_gbh_phys_t (not used ??? never encountered one, yet)

			for(auto i = m_pool->m_vdevs.begin(); i != m_pool->m_vdevs.end(); i++)
			{
				VirtualDevice* vdev = *i;

				if(vdev->id != addr->vdev)
				{
					continue;
				}

				size_t psize = (size_t)(bp->psize + 1) << 9;
				size_t lsize = (size_t)(bp->lsize + 1) << 9;

				std::vector<uint8_t> src(psize);

				vdev->Read(src, psize, addr->offset << 9);

				// TODO: verify bp->chksum

				switch(bp->comp)
				{
				case ZIO_COMPRESS_ON:
				case ZIO_COMPRESS_LZJB:
					dst.resize(lsize);
					lzjb_decompress(src.data(), dst.data(), psize, lsize);
					break;
				case ZIO_COMPRESS_OFF:
					dst.swap(src);
					break;
				default: // TODO: gzip, zle
					ASSERT(0);
					return false;
				}

				return true;
			}
		}

		return false;
	}
}
