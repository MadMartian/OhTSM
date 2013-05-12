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

#include <OgrePageManager.h>
#include <OgreGrid2DPageStrategy.h>

#include "OverhangTerrainPagedWorldSection.h"
#include "OverhangTerrainGroup.h"
#include "DebugTools.h"

namespace Ogre
{
	OverhangTerrainPagedWorldSection::OverhangTerrainPagedWorldSection( const String& name, PagedWorld* parent, SceneManager* sm )
		: PagedWorldSection(name, parent, sm), 
			_pOhGrp(NULL), 
			_pPageStrategy(static_cast< Grid2DPageStrategy * > (parent->getManager()->getStrategy("Grid2D")))
	{
		setStrategy(_pPageStrategy);
	}
	OverhangTerrainPagedWorldSection::~OverhangTerrainPagedWorldSection()
	{
		_pOhGrp->setOverhangTerrainPagedWorldSection(NULL);
		delete _pOhGrp;
	}

	void OverhangTerrainPagedWorldSection::init( OverhangTerrainGroup * otg )
	{
		if (_pOhGrp == otg)
			return;

		delete _pOhGrp;
		_pOhGrp = otg;
		_pOhGrp->setOverhangTerrainPagedWorldSection(this);
		syncSettings();
	}

	void OverhangTerrainPagedWorldSection::saveSubtypeData( StreamSerialiser& ser )
	{
		_pOhGrp->saveGroupDefinition(ser);
	}

	void OverhangTerrainPagedWorldSection::loadSubtypeData( StreamSerialiser& ser )
	{
		assert(_pOhGrp != NULL);
		_pOhGrp->loadGroupDefinition(ser);
	}

	void OverhangTerrainPagedWorldSection::loadPage( PageID pageID, bool forceSynchronous /*= false */ )
	{
		if (!mParent->getManager()->getPagingOperationsEnabled())
			return;

		int32 x, y;
		// pageID is the same as a packed index
		calculateCell(pageID, &x, &y);

		if (_pOhGrp->defineTerrain(x, y, true, forceSynchronous))
			PagedWorldSection::loadPage(pageID, forceSynchronous);
	}

	void OverhangTerrainPagedWorldSection::unloadPage( PageID pageID, bool forceSynchronous /*= false */ )
	{
		if (!mParent->getManager()->getPagingOperationsEnabled())
			return;

		// trigger terrain unload
		int32 x, y;
		// pageID is the same as a packed index
		calculateCell(pageID, &x, &y);
		OHT_DBGTRACE("UNLOAD(" << x << "," << y << ")");

		if (_pOhGrp->unloadTerrain(x, y))
			PagedWorldSection::unloadPage(pageID, forceSynchronous);
	}

	void OverhangTerrainPagedWorldSection::syncSettings()
	{
		// Base grid on terrain settings
		Grid2DPageStrategyData * gridData = getGridStrategyData();

		switch (_pOhGrp->options.alignment)
		{
		case ALIGN_X_Y:
			gridData->setMode(G2D_X_Y);
			break;
		case ALIGN_X_Z:
			gridData->setMode(G2D_X_Z);
			break;
		case ALIGN_Y_Z:
			gridData->setMode(G2D_Y_Z);
			break;
		}

		gridData->setOrigin(_pOhGrp->getOrigin());
		gridData->setCellSize(_pOhGrp->options.getPageWorldSize());
	}
}