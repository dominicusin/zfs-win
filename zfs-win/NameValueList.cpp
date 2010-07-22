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
#include "NameValueList.h"

namespace ZFS
{
	NameValuePair::NameValuePair()
	{
		u8 = NULL;
	}

	NameValuePair::~NameValuePair()
	{
		if(u8 != NULL)
		{
			switch(type)
			{
			case TYPE_INT8:
			case TYPE_INT8_ARRAY:
			case TYPE_UINT8:
			case TYPE_UINT8_ARRAY:
				delete [] u8;
				break;
			case TYPE_INT16:
			case TYPE_INT16_ARRAY:
			case TYPE_UINT16:
			case TYPE_UINT16_ARRAY:
				delete [] u16;
				break;
			case TYPE_INT32:
			case TYPE_INT32_ARRAY:
			case TYPE_UINT32:
			case TYPE_UINT32_ARRAY:
				delete [] u32;
				break;
			case TYPE_INT64:
			case TYPE_INT64_ARRAY:
			case TYPE_UINT64:
			case TYPE_UINT64_ARRAY:
				delete [] u64;
				break;
			case TYPE_STRING:
			case TYPE_STRING_ARRAY:
				delete [] str;
				break;
			case TYPE_NVLIST:
			case TYPE_NVLIST_ARRAY:
				delete [] list;
				break;
			default:
				break;
			}

			u8 = NULL;
		}
	}

	NameValueList::NameValueList()
	{
	}

	NameValueList::~NameValueList()
	{
		for(auto i = begin(); i != end(); i++)
		{
			delete i->second;
		}
	}

	const uint8_t* NameValueList::Read(const uint8_t* ptr, size_t size)
	{
		const uint8_t* ptr_end = ptr + size;

		uint32_t version = ReadU32(ptr);
		uint32_t flags = ReadU32(ptr);

		while(ptr < ptr_end)
		{
			const uint8_t* ptr_start = ptr;

			uint32_t esize = ReadU32(ptr);
			uint32_t dsize = ReadU32(ptr);

			if(esize == 0 && dsize == 0) break;

			NameValuePair* pair = new NameValuePair();

			std::string name = ReadString(ptr);

			pair->type = (NameValueType)ReadU32(ptr);
			pair->count = ReadU32(ptr);

			if(pair->count > 0)
			{
				switch(pair->type)
				{
				case TYPE_BOOLEAN:
				case TYPE_BOOLEAN_ARRAY: // ???
					break;
				case TYPE_BYTE:
				case TYPE_BYTE_ARRAY: // ???
					break;
				case TYPE_INT8:
				case TYPE_INT8_ARRAY:
				case TYPE_UINT8:
				case TYPE_UINT8_ARRAY:
					pair->u8 = new uint8_t[pair->count];
					for(uint32_t i = 0; i < pair->count; i++) 
						pair->u8[i] = ReadU8(ptr);
					break;
				case TYPE_INT16:
				case TYPE_INT16_ARRAY:
				case TYPE_UINT16:
				case TYPE_UINT16_ARRAY:
					pair->u16 = new uint16_t[pair->count];
					for(uint32_t i = 0; i < pair->count; i++) 
						pair->u16[i] = ReadU16(ptr);
					break;
				case TYPE_INT32:
				case TYPE_INT32_ARRAY:
				case TYPE_UINT32:
				case TYPE_UINT32_ARRAY:
					pair->u32 = new uint32_t[pair->count];
					for(uint32_t i = 0; i < pair->count; i++) 
						pair->u32[i] = ReadU32(ptr);
					break;
				case TYPE_INT64:
				case TYPE_INT64_ARRAY:
				case TYPE_UINT64:
				case TYPE_UINT64_ARRAY:
					pair->u64 = new uint64_t[pair->count];
					for(uint32_t i = 0; i < pair->count; i++) 
						pair->u64[i] = ReadU64(ptr);
					break;
				case TYPE_STRING:
				case TYPE_STRING_ARRAY:
					pair->str = new std::string[pair->count];
					for(uint32_t i = 0; i < pair->count; i++) 
						pair->str[i] = ReadString(ptr);
					break;
				case TYPE_NVLIST:
				case TYPE_NVLIST_ARRAY:
					pair->list = new NameValueList[pair->count];
					for(uint32_t i = 0; i < pair->count; i++) 
						ptr = pair->list[i].Read(ptr, ptr_start + esize - ptr);
					break;
				case TYPE_BOOLEAN_VALUE: // ???
					ASSERT(0);
					break;
				case TYPE_HRTIME: // ???
					ASSERT(0);
					break;
				case TYPE_DOUBLE: // ???
					ASSERT(0);
					break;
				case TYPE_UNKNOWN:
				default:
					ASSERT(0);
					break;
				}

				auto i = find(name);

				if(i != end())
				{
					delete i->second;

					erase(i);
				}

				(*this)[name] = pair;
			}

			ptr = ptr_start + esize;
		}

		return ptr;
	}

	uint8_t NameValueList::ReadU8(const uint8_t*& ptr)
	{
		uint8_t v = ptr[0];

		ptr += 1;

		return v;
	}

	uint16_t NameValueList::ReadU16(const uint8_t*& ptr)
	{
		uint16_t v = (ptr[0] << 8) | ptr[1];

		ptr += 2;

		return v;
	}

	uint32_t NameValueList::ReadU32(const uint8_t*& ptr)
	{
		uint32_t v = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | (ptr[3] << 0);

		ptr += 4;

		return v;
	}

	uint64_t NameValueList::ReadU64(const uint8_t*& ptr)
	{
		uint64_t v;

		((uint32_t*)&v)[1] = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
		((uint32_t*)&v)[0] = (ptr[4] << 24) | (ptr[5] << 16) | (ptr[6] << 8) | ptr[7];

		ptr += 8;

		return v;
	}

	std::string NameValueList::ReadString(const uint8_t*& ptr)
	{
		uint32_t size = ReadU32(ptr);

		std::string s((const char*)ptr, size);

		ptr += (size + 3) & ~3;

		return s;
	}
}