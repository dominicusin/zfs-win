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
#include "NameValueList.h"

namespace ZFS
{
	class Device;

	class VirtualDevice
	{
	public:
		Device* dev;
		std::string type;
		uint64_t id;
		uint64_t guid;
		uint64_t metaslab_array;
		uint64_t metaslab_shift;
		uint64_t ashift;
		uint64_t asize;
		std::string path;
		std::string devid;
		uint64_t nparity;
		uint64_t whole_disk;
		uint64_t is_log;
		std::vector<VirtualDevice> children;

		void Init(NameValueList* nvl) throw(...);
		bool Read(std::vector<uint8_t>& buff, uint64_t size, uint64_t offset);
		VirtualDevice* Find(uint64_t guid_to_find);
		void GetLeaves(std::list<VirtualDevice*>& leaves);
	};

	class DeviceDesc
	{
	public:
		uint64_t guid;
		uint64_t top_guid;
		uint64_t state;
		struct {uint64_t id; std::string name;} host;
		struct {uint64_t guid; std::string name;} pool;
		uint64_t txg;
		uint64_t version;
		VirtualDevice top;
		size_t ub_size;

		bool Init(vdev_phys_t& vd);
	};

	class Device
	{
	public:
		DeviceDesc m_desc;
		HANDLE m_handle;
		uint64_t m_start;
		uint64_t m_size;
		uint64_t m_bytes;
		vdev_label_t* m_label;
		uberblock_t* m_active;

	public:
		Device();
		virtual ~Device();

		bool Open(const wchar_t* path, uint32_t partition = 0); // partition 0x0000EEPP (PP primary, EE extended, zero based index)
		void Close();

		uint64_t Seek(uint64_t pos);
		size_t Read(void* buff, uint64_t size);
	};
}