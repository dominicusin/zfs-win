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

namespace ZFS
{
	BlockReader::BlockReader(Pool* pool, dnode_phys_t* dn)
		: m_pool(pool)
	{
		m_node = *dn;
		m_datablksize = m_node.datablkszsec << 9;
		m_indblksize = 1 << m_node.indblkshift;
		m_indblkcount = m_indblksize / sizeof(blkptr_t);
		m_size = (m_node.maxblkid + 1) * m_datablksize;
		m_cache.id = -1;

		ASSERT(m_node.nlevels > 0);
		ASSERT(m_node.indblkshift >= 7);
		ASSERT(m_node.nblkptr <= m_indblkcount);

		m_tree.resize(m_node.nlevels);

		size_t n = (size_t)((m_node.maxblkid + 1 + m_indblkcount - 1) / m_indblkcount);

		for(size_t i = 0; i < m_tree.size(); i++)
		{
			blklvl_t& lvl = m_tree[i];

			lvl.resize(n);

			memset(lvl.data(), 0, lvl.size() * sizeof(blkcol_t*));

			n = (n + m_indblkcount - 1) / m_indblkcount;
		}

		blkcol_t* col = new blkcol_t(m_node.nblkptr);

		memcpy(col->data(), m_node.blkptr, m_node.nblkptr * sizeof(blkptr_t));

		m_tree.back()[0] = col;
	}

	BlockReader::~BlockReader()
	{
		for(auto i = m_tree.begin(); i != m_tree.end(); i++)
		{
			for(auto j = i->begin(); j != i->end(); j++)
			{
				delete *j;
			}
		}
	}

	size_t BlockReader::Read(void* dst, size_t size, uint64_t offset)
	{
		uint64_t block_id = offset / m_datablksize;
		size_t block_offset = (size_t)(offset - (uint64_t)m_datablksize * block_id);

		uint8_t* ptr = (uint8_t*)dst;

		for(; block_id <= m_node.maxblkid && size > 0; block_id++)
		{
			blkptr_t* bp = NULL;

			if(!FetchBlock(0, block_id, &bp))
			{
				return false;
			}

			size_t bytes = 0;

			if(bp->type != DMU_OT_NONE)
			{
				if(m_cache.id != block_id)
				{
					if(!m_pool->Read(m_cache.buff, bp))
					{
						break;
					}

					m_cache.id = block_id;
				}

				ASSERT(m_cache.buff.size() == m_datablksize);

				uint8_t* src = m_cache.buff.data() + block_offset;
				size_t src_size = m_cache.buff.size() - block_offset;

				bytes = std::min<size_t>(src_size, size);

				memcpy(ptr, src, bytes);
			}
			else
			{
				if(ptr == (uint8_t*)dst && m_node.type == DMU_OT_PLAIN_FILE_CONTENTS)
				{
					dnode_phys_t* dnode = &m_node;
					znode_phys_t* znode = (znode_phys_t*)m_node.bonus();

					uint8_t* extra = (uint8_t*)(znode + 1);
					size_t extra_size = (uint8_t*)(dnode + 1) - extra;

					if(znode->size <= extra_size)
					{
						bytes = std::min<size_t>(size, (size_t)znode->size);
						
						memcpy(ptr, extra, bytes);

						return bytes;
					}
				}

				size_t src_size = m_datablksize - block_offset;

				bytes = std::min<size_t>(src_size, size);

				memset(ptr, 0, bytes);
			}

			ptr += bytes;
			size -= bytes;

			block_offset = 0;
		}

		ASSERT(size == 0); 

		return ptr - (uint8_t*)dst;
	}

	bool BlockReader::FetchBlock(size_t level, uint64_t id, blkptr_t** bp)
	{
		blklvl_t& lvl = m_tree[level];

		size_t col_id = (size_t)(id >> (m_node.indblkshift - 7));

		if(lvl[col_id] == NULL)
		{
			blkptr_t* bp = NULL;

			if(!FetchBlock(level + 1, col_id, &bp))
			{
				return false;
			}

			std::vector<uint8_t> buff;

			if(bp->type != DMU_OT_NONE)
			{
				if(!m_pool->Read(buff, bp) || buff.size() != m_indblksize)
				{
					return false;
				}
			}
			else
			{
				// FIXME: there may be empty pointers in the middle of other valid pointers (???)

				buff.resize(m_indblksize);

				memset(buff.data(), 0, m_indblksize);
			}

			blkcol_t* col = new blkcol_t(m_indblkcount);

			memcpy(col->data(), buff.data(), buff.size()); // TODO: read into col->data() directly

			lvl[col_id] = col;
		}

		blkcol_t* col = lvl[col_id];

		uint64_t mask = (1ull << (m_node.indblkshift - 7)) - 1;

		*bp = &col->at((size_t)(id & mask));

		return true;
	}
}
