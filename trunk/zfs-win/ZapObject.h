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

namespace ZFS
{
	class ZapObject : public std::map<std::string, std::vector<uint8_t>*>
	{
		Pool* m_pool;

		void RemoveAll();
		void ParseMicro(std::vector<uint8_t>& buff);
		void ParseFat(std::vector<uint8_t>& buff);
		bool ParseArray(std::vector<uint8_t>& buff, zap_leaf_entry_t* e, uint16_t index);

	public:
		ZapObject(Pool* pool);
		virtual ~ZapObject();

		bool Init(dnode_phys_t* dn);

		bool Lookup(const char* name, uint64_t& value);
		bool Lookup(const char* name, std::string& value);
	};
}