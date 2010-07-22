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

#include "StdAfx.h"
#include "ZapObject.h"

namespace ZFS
{
	ZapObject::ZapObject()
	{
	}

	ZapObject::~ZapObject()
	{
		Clear();
	}

	void ZapObject::Parse(std::vector<uint8_t>& buff)
	{
		Clear();

		if(buff.size() >= sizeof(uint64_t))
		{
			uint64_t* ptr = (uint64_t*)buff.data();

			if(*ptr == ZBT_MICRO) ParseMicro(buff);
			else if(*ptr == ZBT_HEADER) ParseFat(buff);
		}
	}

	void ZapObject::Clear()
	{
		for(auto i = begin(); i != end(); i++)
		{
			delete i->second;
		}

		clear();
	}

	bool ZapObject::Lookup(const char* name, uint64_t& value)
	{
		auto i = find(name);

		if(i != end())
		{
			if(i->second->size() == 8)
			{
				value = BSWAP_64(*(uint64_t*)i->second->data()); // fat zap big endian???

				return true;
			}
		}

		return false;
	}

	bool ZapObject::Lookup(const char* name, std::string& value)
	{
		auto i = find(name);

		if(i != end())
		{
			value = std::string((char*)i->second->data(), i->second->size());
			
			return true;
		}

		return false;
	}

	void ZapObject::ParseMicro(std::vector<uint8_t>& buff)
	{
		mzap_phys_t* mzap = (mzap_phys_t*)buff.data();

		for(size_t i = 0, n = buff.size() / MZAP_ENT_LEN - 1; i < n; i++)
		{
			std::string name = mzap->chunk[i].name;

			if(name.empty()) continue;

			auto j = find(name);

			if(j != end())
			{
				delete j->second;

				erase(j);
			}

			std::vector<uint8_t>* value = new std::vector<uint8_t>(sizeof(uint64_t));

			uint64_t tmp = BSWAP_64(mzap->chunk[i].value); // make the same as fat zap

			memcpy(value->data(), &tmp, sizeof(uint64_t));

			(*this)[name] = value;
		}
	}

	void ZapObject::ParseFat(std::vector<uint8_t>& buff)
	{
		size_t half_size = buff.size() / 2; // first half wasted ???

		zap_phys_t* zap = (zap_phys_t*)buff.data(); // TODO: zap->ptrtbl ???
		zap_leaf_phys_t* leaf = (zap_leaf_phys_t*)(buff.data() + half_size);

		zap_leaf_entry_t* e = (zap_leaf_entry_t*)(uint8_t*)&leaf->hash[half_size / 32];
		zap_leaf_entry_t* e_end = (zap_leaf_entry_t*)(buff.data() + buff.size());

		for(size_t i = 0, n = e_end - e; i < n; i++)
		{
			if(e[i].type != ZAP_CHUNK_ENTRY)
			{
				continue;
			}

			std::vector<uint8_t> name(e[i].name_numints);

			if(!ParseArray(name, e, e[i].name_chunk) || name.empty())
			{
				continue;
			}

			std::vector<uint8_t>* value = new std::vector<uint8_t>(e[i].value_numints * e[i].value_intlen);

			if(!ParseArray(*value, e, e[i].value_chunk))
			{
				delete value;

				continue;
			}

			std::string s((char*)name.data(), name.size() - 1);

			auto j = find(s);

			if(j != end())
			{
				delete j->second;

				erase(j);
			}
			
			(*this)[s] = value;
		}
	}

	bool ZapObject::ParseArray(std::vector<uint8_t>& buff, zap_leaf_entry_t* e, uint16_t index)
	{
		uint8_t* ptr = buff.data();
		size_t size = buff.size();

		while(index != 0xffff && size > 0)
		{
			zap_leaf_array_t* l = (zap_leaf_array_t*)&e[index];

			if(l->type != ZAP_CHUNK_ARRAY)
			{
				ASSERT(0);
				
				break;
			}

			size_t n = std::min<size_t>(size, ZAP_LEAF_ARRAY_BYTES);

			memcpy(ptr, l->buff, n);

			ptr += n;
			size -= n;

			index = l->next;
		}

		return size == 0;
	}
}