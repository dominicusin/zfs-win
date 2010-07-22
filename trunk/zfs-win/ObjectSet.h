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
