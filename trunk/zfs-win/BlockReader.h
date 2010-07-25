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

#pragma once

#include "zfs.h"
#include "Pool.h"
#include "Device.h"

namespace ZFS
{
	class BlockReader
	{
		Pool* m_pool;
		dnode_phys_t m_node;
		size_t m_datablksize;
		size_t m_indblksize;
		size_t m_indblkcount;
		uint64_t m_size;
		struct {uint64_t id; std::vector<uint8_t> buff;} m_cache;

		typedef std::vector<blkptr_t> blkcol_t;
		typedef std::vector<blkcol_t*> blklvl_t;
		typedef std::vector<blklvl_t> blktree_t;

		blktree_t m_tree;

		bool FetchBlock(size_t level, uint64_t id, blkptr_t** bp);

	public:
		BlockReader(Pool* pool, dnode_phys_t* dn);
		virtual ~BlockReader();

		size_t Read(void* dst, size_t size, uint64_t offset);
		uint64_t GetDataSize() const {return m_size;}
	};
}