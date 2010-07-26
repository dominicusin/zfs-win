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

namespace ZFS
{
	enum NameValueType
	{
		TYPE_UNKNOWN = 0,
		TYPE_BOOLEAN,
		TYPE_BYTE,
		TYPE_INT16,
		TYPE_UINT16,
		TYPE_INT32,
		TYPE_UINT32,
		TYPE_INT64,
		TYPE_UINT64,
		TYPE_STRING,
		TYPE_BYTE_ARRAY,
		TYPE_INT16_ARRAY,
		TYPE_UINT16_ARRAY,
		TYPE_INT32_ARRAY,
		TYPE_UINT32_ARRAY,
		TYPE_INT64_ARRAY,
		TYPE_UINT64_ARRAY,
		TYPE_STRING_ARRAY,
		TYPE_HRTIME,
		TYPE_NVLIST,
		TYPE_NVLIST_ARRAY,
		TYPE_BOOLEAN_VALUE,
		TYPE_INT8,
		TYPE_UINT8,
		TYPE_BOOLEAN_ARRAY,
		TYPE_INT8_ARRAY,
		TYPE_UINT8_ARRAY,
		TYPE_DOUBLE
	};

	class NameValueList;

	class NameValuePair
	{
	public:
		NameValueType type;
		uint32_t count;

		union
		{
			int8_t* i8;
			uint8_t* u8;
			int16_t* i16;
			uint16_t* u16;
			int32_t* i32;
			uint32_t* u32;
			int64_t* i64;
			uint64_t* u64;
			std::string* str;
			NameValueList* list;
		};

		NameValuePair();
		virtual ~NameValuePair();
	};

	class NameValueList : public std::map<std::string, NameValuePair*>
	{
		uint8_t ReadU8(const uint8_t*& ptr);
		uint16_t ReadU16(const uint8_t*& ptr);
		uint32_t ReadU32(const uint8_t*& ptr);
		uint64_t ReadU64(const uint8_t*& ptr);
		std::string ReadString(const uint8_t*& ptr);

		const uint8_t* Read(const uint8_t* ptr, size_t size);

	public:
		NameValueList();
		virtual ~NameValueList();

		bool Init(const uint8_t* ptr, size_t size);
	};
}