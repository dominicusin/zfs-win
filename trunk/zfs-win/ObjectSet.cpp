#include "StdAfx.h"
#include "ObjectSet.h"

namespace ZFS
{
	ObjectSet::ObjectSet()
		: m_dnode_count(0)
	{
	}

	ObjectSet::~ObjectSet()
	{
	}

	bool ObjectSet::Init(std::vector<uint8_t>& objset, std::vector<uint8_t>& dnode)
	{
		m_objset.swap(objset);
		m_dnode.swap(dnode);
		m_dnode_count = m_dnode.size() / sizeof(dnode_phys_t);

		return true;
	}
}