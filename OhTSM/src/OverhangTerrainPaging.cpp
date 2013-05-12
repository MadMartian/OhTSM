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

#include "pch.h"

#include "OverhangTerrainPaging.h"

namespace Ogre
{
	OverhangTerrainPaging::OverhangTerrainPaging( PageManager * pgman )
		: _pPageMan(pgman)
	{
		_pPageMan->addWorldSectionFactory(&_factory);
	}
	OverhangTerrainPaging::~OverhangTerrainPaging(void)
	{
		_pPageMan->removeWorldSectionFactory(&_factory);
	}

	OverhangTerrainPagedWorldSection * OverhangTerrainPaging::createWorldSection(
		PagedWorld * world, 
		OverhangTerrainGroup * trgrp, 
		Real loadRadius /* = 2000 */, 
		Real holdRadius /* = 3000 */, 
		int32 minX /* = -32768 */, int32 minY /* = -32768 */, int32 maxX /* = 32767 */, int32 maxY /* = 32767 */, 
		const String & sSectName /* = StringUtil::BLANK */
	)
	{
		OverhangTerrainPagedWorldSection * sect = 
			static_cast< OverhangTerrainPagedWorldSection * > 
				(world->createSection(trgrp->getSceneManager(), SectionFactory::FACTORY_NAME, sSectName));

		sect->setLoadRadius(loadRadius);
		sect->setHoldRadius(holdRadius);
		sect->setPageRange(minX, minY, maxX, maxY);
		sect->init(trgrp);	// Must be called after the load/hold radii have been set

		return sect;
	}

	const String OverhangTerrainPaging::SectionFactory::FACTORY_NAME ("OverhangTerrain");
}