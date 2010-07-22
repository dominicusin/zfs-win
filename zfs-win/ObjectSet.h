#pragma once

#include "zfs.h"

namespace ZFS
{
	class ObjectSet
	{
		std::vector<uint8_t> m_objset;
		std::vector<uint8_t> m_dnode;
		size_t m_dnode_count;

	public:
		ObjectSet();
		virtual ~ObjectSet();

		bool Init(std::vector<uint8_t>& objset, std::vector<uint8_t>& dnode);

		objset_phys_t* operator -> () {ASSERT(m_objset.size() >= sizeof(objset_phys_t)); return (objset_phys_t*)m_objset.data();}
		dnode_phys_t* operator [] (size_t index) {ASSERT(index < m_dnode_count); return (dnode_phys_t*)m_dnode.data() + index;}
		size_t count() {return m_dnode_count;}
	};
}
