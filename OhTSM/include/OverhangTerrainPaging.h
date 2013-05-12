/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2006 Torus Knot Software Ltd
Also see acknowledgments in Readme.html

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.

You may alternatively use this source under the terms of a specific version of
the OGRE Unrestricted License provided you have obtained such a license from
Torus Knot Software Ltd.

Adapted by Jonathan Neufeld (http://www.extollit.com) 2011-2013 for OverhangTerrain Scene Manager
-----------------------------------------------------------------------------
*/

#ifndef __OVERHANGTERRAINPAGING_H__
#define __OVERHANGTERRAINPAGING_H__

#include <OgrePagedWorld.h>
#include <OgrePagedWorldSection.h>
#include <OgrePageManager.h>

#include "OverhangTerrainPrerequisites.h"
#include "OverhangTerrainPagedWorldSection.h"
#include "OverhangTerrainGroup.h"

namespace Ogre
{
	/* A wrapper for creating overhang terrain paging objects compatible with the Ogre paging system and parallels the classic terrain paging system */
	class _OverhangTerrainPluginExport OverhangTerrainPaging
	{
	public:
		/** 
		@param pgman The page manager that manages paged loading/unloading for all sections and worlds */
		OverhangTerrainPaging(PageManager * pgman);

		/** Creates a world section suitable for overhang terrain
		@remarks Call this factory method rather than instantiating the OverhangTerrainPagedWorldSection directly
			as it performs some additional initialization and bindings 
		@param world The parent paged world within which this section resides
		@param trgrp The overhang terrain group that manages page loading/unloading
		@param loadRadius Maximum distance from the camera that terrain pages are loaded
		@param holdRadius Maximum distance from the camera that terrain pages are retained and not unloaded 
		@param minX The minimum possible x-component of the 2D page index for all possible pages
		@param minY The minimum possible y-component of the 2D page index for all possible pages
		@param maxX The maximum possible x-component of the 2D page index for all possible pages
		@param maxY The maximum possible y-component of the 2D page index for all possible pages
		@param sSectName Optional name for the world section otherwise one will be generated 
		@returns An OverhangTerrainPagedWorldSection instance initialized and bound to the specified terrain group */
		OverhangTerrainPagedWorldSection * createWorldSection (
			PagedWorld * world, 
			OverhangTerrainGroup * trgrp, 
			Real loadRadius = 2000, 
			Real holdRadius = 3000, 
			int32 minX = -32768, int32 minY = -32768, int32 maxX = 32767, int32 maxY = 32767, 
			const String & sSectName = StringUtil::BLANK
		);

		~OverhangTerrainPaging();

	private:
		/// The page manager that manages paged loading/unloading for all sections and worlds
		PageManager * _pPageMan;

		/// Adapter to the OGRE SectionFactory API
		class _OverhangTerrainPluginExport SectionFactory : public PagedWorldSectionFactory
		{
		public:
			/// OverhangTerrain-specific factory name
			static const String FACTORY_NAME;

			const String & getName() const 
				{ return FACTORY_NAME; }
			PagedWorldSection * createInstance(const String & name, PagedWorld * parent, SceneManager * sm)
				{ return new OverhangTerrainPagedWorldSection(name, parent, sm); }
			void destroyInstance (PagedWorldSection * pgsect) 
				{ delete pgsect; }
		} _factory;
	};
}

#endif