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
#include "Device.h"

namespace ZFS
{
	class Pool
	{
	public:
		uint64_t m_guid; 
		std::string m_name;
		std::vector<Device*> m_devs;
		std::vector<VirtualDevice*> m_vdevs;

		static bool Verify(uint8_t* buff, size_t size, uint8_t cksum_type, cksum_t& cksum);

	public:
		Pool();
		virtual ~Pool();

		bool Open(const std::list<std::wstring>& paths, const wchar_t* name = NULL);
		void Close();

		bool Read(uint8_t* buff, size_t size, blkptr_t* bp);
	};
}