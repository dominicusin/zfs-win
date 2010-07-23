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

#include "Pool.h"
#include "ObjectSet.h"

namespace ZFS
{
	class DataSet
	{
	public:
		dsl_dir_phys_t* m_dir; // TODO: store its properties instead
		dsl_dataset_phys_t* m_dataset; // TODO: store its properties instead
		std::string m_name;
		std::string m_mountpoint;
		std::list<DataSet*> m_children;

	public:
		DataSet();
		virtual ~DataSet();

		bool Read(Pool& pool, ObjectSet& os, const char* name = NULL, dnode_phys_t* root_dataset = NULL);
		void GetMountPoints(std::list<DataSet*>& mpl);

		// TODO: directory browsing functions, handle mount-points transparently
	};
}