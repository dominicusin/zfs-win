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
#include "BlockReader.h"

namespace ZFS
{
	ZapObject::ZapObject(Pool* pool)
		: m_pool(pool)
	{
	}

	ZapObject::~ZapObject()
	{
		RemoveAll();
	}

	bool ZapObject::Init(dnode_phys_t* dn)
	{
		RemoveAll();

		BlockReader r(m_pool, dn);

		size_t size = (size_t)r.GetDataSize();

		if(size >= sizeof(uint64_t))
		{
			std::vector<uint8_t> buff(size);

			if(r.Read(buff.data(), size, 0) == size)
			{
				switch(*(uint64_t*)buff.data())
				{
				case ZBT_MICRO:
					ParseMicro(buff);
					return true;
				case ZBT_HEADER:
					ParseFat(buff);
					return true;
				}
			}
		}

		return false;
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
			std::vector<uint8_t>* buff = i->second;

			if(!buff->empty())
			{
				char* ptr = (char*)buff->data();
				size_t size = buff->size();

				while(size > 0 && ptr[size - 1] == 0) 
				{
					size--;
				}

				value = std::string(ptr, size);
			}
			else
			{
				value.empty();
			}

			return true;
		}

		return false;
	}

	void ZapObject::RemoveAll()
	{
		for(auto i = begin(); i != end(); i++)
		{
			delete i->second;
		}

		clear();
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
		// NOTE: not sure about this, the documentation is outdated, 0x4000 granularity seems to work

		ASSERT(buff.size() >= 0x8000 && (buff.size() & 0x3fff) == 0);

		if(buff.size() < 0x8000) 
		{
			return;
		}

		zap_phys_t* zap = (zap_phys_t*)buff.data(); // TODO: zap->ptrtbl ???

		size_t n = buff.size() / 0x4000 - 1;
		size_t m = 0x4000 - offsetof(zap_leaf_phys_t, hash[0x4000 / 32]);

		uint8_t* ptr = buff.data() + 0x4000;

		for(size_t i = 0; i < n; i++, ptr += 0x4000)
		{
			zap_leaf_phys_t* leaf = (zap_leaf_phys_t*)ptr;

			ASSERT(leaf->block_type == ZBT_LEAF);

			zap_leaf_entry_t* e = (zap_leaf_entry_t*)&leaf->hash[0x4000 / 32];

			for(size_t i = 0, n = m / sizeof(zap_leaf_entry_t); i < n; i++)
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