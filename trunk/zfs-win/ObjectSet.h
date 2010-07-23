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

#include "Pool.h"
#include "ZapObject.h"

namespace ZFS
{
	class ObjectSet
	{
		std::vector<uint8_t> m_objset;
		std::vector<uint8_t> m_dnode;
		size_t m_dnode_count;
		ZapObject m_objdir;

	public:
		ObjectSet();
		virtual ~ObjectSet();

		bool Read(Pool& pool, blkptr_t* bp, size_t count);

		objset_phys_t* operator -> ();
		dnode_phys_t* operator [] (size_t index);
		dnode_phys_t* operator [] (const char* s);
		size_t count();
	};
}
