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
		std::list<blkptr_t*> m_bpl;

		void Insert(blkptr_t* bp, size_t count);
		bool Read(blkptr_t* bp, std::vector<uint8_t>& buff);

		// these should be part of some utility class:

		static bool Verify(std::vector<uint8_t>& buff, uint8_t cksum_type, cksum_t& cksum);
		static bool Decompress(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, size_t lsize, uint8_t comp_type);

	public:
		BlockReader(Pool* pool, blkptr_t* bp, size_t count);
		virtual ~BlockReader();

		bool ReadNext(std::vector<uint8_t>& buff);
		bool ReadToEnd(std::vector<uint8_t>& buff);

		// TODO: function to read only part of the whole data (for large files)
	};
}