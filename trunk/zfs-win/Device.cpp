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
#include "Device.h"
#include "Hash.h"

namespace ZFS
{
	// VirtualDevice

	void VirtualDevice::Init(NameValueList* nvl)
	{
		dev = NULL;
		type = nvl->at("type")->str[0];
		id = nvl->at("id")->u64[0];
		guid = nvl->at("guid")->u64[0];
		metaslab_array = nvl->find("metaslab_array") != nvl->end() ? nvl->at("metaslab_array")->u64[0] : 0;
		metaslab_shift = nvl->find("metaslab_shift") != nvl->end() ? nvl->at("metaslab_shift")->u64[0] : 0;
		ashift = nvl->find("ashift") != nvl->end() ? nvl->at("ashift")->u64[0] : 0;
		asize = nvl->find("asize") != nvl->end() ? nvl->at("asize")->u64[0] : 0;
		if(nvl->find("path") != nvl->end()) path = nvl->at("path")->str[0];
		if(nvl->find("devid") != nvl->end()) devid = nvl->at("devid")->str[0];
		nparity = nvl->find("nparity") != nvl->end() ? nvl->at("nparity")->u64[0] : 0;
		whole_disk = nvl->find("whole_disk") != nvl->end() ? nvl->at("whole_disk")->u64[0] : 0;
		is_log = nvl->find("is_log") != nvl->end() ? nvl->at("is_log")->u64[0] : 0;

		if(nvl->find("children") != nvl->end())
		{
			NameValuePair* nvp = nvl->at("children");

			children.resize(nvp->count);

			for(uint32_t i = 0; i < nvp->count; i++)
			{
				children[i].Init(&nvp->list[i]);
			}
		}
	}

	bool VirtualDevice::Read(std::vector<uint8_t>& buff, size_t size, uint64_t offset)
	{
		// TODO: handle chksum errors
		// TODO: parallel reads with overlapped i/o

		buff.resize(size);

		if(type == "disk" || type == "file")
		{
			if(dev != NULL)
			{
				if(dev->Read(buff.data(), size, offset + 0x400000) == size)
				{
					return true;
				}
			}
		}
		else if(type == "mirror")
		{
			for(auto i = children.begin(); i != children.end(); i++)
			{
				VirtualDevice& vdev = *i;

				if(vdev.dev != NULL)
				{
					if(vdev.dev->Read(buff.data(), size, offset + 0x400000) == size)
					{
						return true;
					}
				}
			}
		}
		else if(type == "raidz")
		{
			raidz_map_t rm(offset, size, (uint32_t)ashift, children.size(), (uint32_t)nparity);

			uint64_t total = 0;

			for(size_t i = 1; i < rm.m_col.size(); i++)
			{
				total += rm.m_col[i].size;
			}

			if(total > buff.size())
			{
				return false;
			}

			uint8_t* p = buff.data();

			for(size_t i = 1; i < rm.m_col.size(); i++) // TODO: nparity > 1
			{
				VirtualDevice& vdev = children[(size_t)rm.m_col[i].devidx];

				if(vdev.dev != NULL)
				{
					vdev.dev->BeginRead(p, rm.m_col[i].size, rm.m_col[i].offset + 0x400000);
				}

				p += rm.m_col[i].size;
			}

			size_t succeeded = 1;

			for(size_t i = 1; i < rm.m_col.size(); i++) // TODO: nparity > 1
			{
				VirtualDevice& vdev = children[(size_t)rm.m_col[i].devidx];

				if(vdev.dev != NULL)
				{
					if(vdev.dev->EndRead() == rm.m_col[i].size)
					{
						succeeded++;
					}
				}
			}

			if(succeeded < rm.m_col.size())
			{
				// TODO: reconstruct data

				return false;
			}

			return true;
		}
		else 
		{
			ASSERT(0);
		}

		return false;
	}

	VirtualDevice* VirtualDevice::Find(uint64_t guid_to_find)
	{
		if(guid == guid_to_find)
		{
			return this;
		}

		for(auto i = children.begin(); i != children.end(); i++)
		{
			VirtualDevice* vdev = i->Find(guid_to_find);

			if(vdev != NULL)
			{
				return vdev;
			}
		}

		return NULL;
	}

	void VirtualDevice::GetLeaves(std::list<VirtualDevice*>& leaves)
	{
		if(children.empty())
		{
			leaves.push_back(this);
		}
		else
		{
			for(auto i = children.begin(); i != children.end(); i++)
			{
				i->GetLeaves(leaves);
			}
		}
	}

	// Device

	Device::Device()
		: m_handle(NULL)
		, m_start(0)
		, m_size(0)
		, m_bytes(0)
		, m_label(NULL)
		, m_active(NULL)
	{
		memset(&m_overlapped, 0, sizeof(m_overlapped));

		m_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);  
	}

	Device::~Device()
	{
		Close();

		CloseHandle(m_overlapped.hEvent); 
	}

	bool Device::Open(const wchar_t* path, uint32_t partition)
	{
		Close();

		m_handle = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED, (HANDLE)NULL);

		if(m_handle == INVALID_HANDLE_VALUE)
		{
			m_handle = NULL;

			return false;
		}

		if(!GetFileSizeEx(m_handle, (LARGE_INTEGER*)&m_size))
		{
			DISK_GEOMETRY_EX dg;
			DWORD sz;
		
			if(DeviceIoControl(m_handle, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &dg, sizeof(dg), &sz, NULL))
			{
				m_size = dg.DiskSize.QuadPart;
			}
		}

		for(int i = 0; i < 2; i++, partition >>= 8)
		{
			uint8_t mbr[0x200];

			Read(mbr, sizeof(mbr), 0);

			if(mbr[0x1fe] == 0x55 || mbr[0x1ff] == 0xaa)
			{
				uint8_t* ptr = &mbr[0x1be];

				for(int i = 0; i < 4; i++)
				{
					if((partition & 0xff) == i)
					{
						uint64_t start = *(uint32_t*)&ptr[i * 16 + 8];
						uint64_t size = *(uint32_t*)&ptr[i * 16 + 12];

						if(start != 0 && size != 0)
						{
							m_start += start << 9;
							m_size = size << 9;
						}

						break;
					}
				}
			}
		}

		m_label = new vdev_label_t();

		Read(m_label, sizeof(vdev_label_t), 0);

		if(m_label->vdev_phys.zbt.magic != ZEC_MAGIC)
		{
			return false;
		}

		// TODO: verify m_label->vdev_phys.zbt.chksum (but which part of m_label does it hash ???)

		if(!m_desc.Init(m_label->vdev_phys))
		{
			return false;
		}

		for(size_t i = 0; i < sizeof(m_label->uberblock); i += m_desc.ub_size)
		{
			uberblock_t* ub = (uberblock_t*)&m_label->uberblock[i];

			if(ub->magic == BSWAP_64(UBERBLOCK_MAGIC))
			{
				// TODO: be <-> le
			}

			if(ub->magic != UBERBLOCK_MAGIC)
			{
				continue;
			}

			if(m_active == NULL || ub->txg > m_active->txg)
			{
				m_active = ub;
			}
		}

		return true;
	}

	void Device::Close()
	{
		if(m_handle != NULL)
		{
			CancelIo(m_handle);

			CloseHandle(m_handle);

			m_handle = NULL;
		}

		if(m_label != NULL)
		{
			delete m_label;

			m_label = NULL;
		}

		m_start = 0;
		m_size = 0;
		m_bytes = 0;

		m_active = NULL;
	}

	size_t Device::Read(void* buff, size_t size, uint64_t offset)
	{
		return BeginRead(buff, size, offset) ? EndRead() : 0;
	}

	bool Device::BeginRead(void* buff, size_t size, uint64_t offset)
	{
		offset += m_start;

		m_overlapped.Offset = (DWORD)offset;
		m_overlapped.OffsetHigh = (DWORD)(offset >> 32);

		if(!ReadFile(m_handle, buff, size, NULL, &m_overlapped))
		{
			switch(GetLastError())
			{
			case ERROR_IO_PENDING:
				break;
			case ERROR_HANDLE_EOF:
				return false;
			}
		}

		return true;
	}

	size_t Device::EndRead()
	{
		DWORD size;

		if(GetOverlappedResult(m_handle, &m_overlapped, &size, TRUE))
		{
			return (size_t)size;
		}

		return 0;
 	}

	// DeviceDesc

	bool DeviceDesc::Init(vdev_phys_t& vd)
	{
		NameValueList nvl;

		nvl.Init(vd.nvlist, sizeof(vd.nvlist));

		try
		{
			guid = nvl.at("guid")->u64[0];
			top_guid = nvl.at("top_guid")->u64[0];
			state = nvl.at("state")->u64[0];
			host.id = nvl.at("hostid")->u64[0];
			host.name = nvl.at("hostname")->str[0];
			pool.guid = nvl.at("pool_guid")->u64[0];
			pool.name = nvl.at("name")->str[0];
			txg = nvl.at("txg")->u64[0];
			version = nvl.at("version")->u64[0];
			top.Init(nvl.at("vdev_tree")->list);
			ub_size = 1 << std::max<size_t>((size_t)top.ashift, UBERBLOCK_SHIFT);
		}
		catch(...)
		{
			return false;
		}

		return true;
	}
}
