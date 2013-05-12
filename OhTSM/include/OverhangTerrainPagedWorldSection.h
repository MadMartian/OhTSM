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
#ifndef __OVERHANGTERRAINPAGEDWORLDSECTION_H__
#define __OVERHANGTERRAINPAGEDWORLDSECTION_H__

#include <OgrePagedWorld.h>
#include <OgrePagedWorldSection.h>
#include <OgreGrid2DPageStrategy.h>

#include "OverhangTerrainPrerequisites.h"

namespace Ogre
{
	/* A wrapper for handling terrain paging with overhang terrain scene manager, specialization 
		of the Ogre paging system and parallels the classic terrain paging system */
	class _OverhangTerrainPluginExport OverhangTerrainPagedWorldSection : public PagedWorldSection
	{
	public:
		/** 
		@param name An optional name to give the section (if none is provided, one will be generated)
		@param parent Mandatory PagedWorld that owns this section
		@param sm The scene manager within which pages will be managed */
		OverhangTerrainPagedWorldSection(const String& name, PagedWorld* parent, SceneManager* sm);
		virtual ~OverhangTerrainPagedWorldSection();

		/** Binds the terrain group with this world section 
			@remarks This is called automatically when calling the factory method OverhangTerrainPaging::createWorldSection */
		void init (OverhangTerrainGroup * otg);

		/// Sets the load radius for the grid strategy employed
		virtual void setLoadRadius(Real sz) { getGridStrategyData() -> setLoadRadius (sz); }
		/// Sets the hold radius for the grid strategy employed
		virtual void setHoldRadius(Real sz) { getGridStrategyData() -> setHoldRadius (sz); }
		/// Sets the page range for the grid strategy employed
		virtual void setPageRange (int32 minX, int32 minY, int32 maxX, int32 maxY)
		{
			getGridStrategyData() -> setCellRange(minX, minY, maxX, maxY);
		}

		/// Calls OverhangTerrainGroup::saveGroupDefinition
		virtual void saveSubtypeData( StreamSerialiser& ser );
		/// Calls OverhangTerrainGroup::loadGroupDefinition
		virtual void loadSubtypeData( StreamSerialiser& ser );

		/// Calls OverhangTerrainGroup::defineTerrain and if that fails it calls the superclass implementation
		virtual void loadPage( PageID pageID, bool forceSynchronous = false );
		/// Calls OverhangTerrainGroup::unloadTerrain and if that fails it calls the superclass implementation
		virtual void unloadPage( PageID pageID, bool forceSynchronous = false );

		/// Synchronizes the grid strategy's state with the main top-level OhTSM configuration properties
		void syncSettings();

		/// Calculates a page ID from the given 2D index
		inline PageID calculatePageID(const int32 x, const int32 y) const
			{ return getGridStrategyData() -> calculatePageID(x, y); }
		/// Calculates a the 2D index from the given page ID
		inline void calculateCell(const PageID inPageID, int32 * x, int32 * y)
			{ getGridStrategyData() -> calculateCell(inPageID, x, y); }

	private:
		/// Bound terrain group when this section was created with the factory method described previously
		OverhangTerrainGroup * _pOhGrp;
		/// The grid strategy used, simple 2D for now
		Grid2DPageStrategy * _pPageStrategy;

		/// Retrieve the state of the grid strategy
		inline Grid2DPageStrategyData * getGridStrategyData() const { return static_cast<Grid2DPageStrategyData*> (mStrategyData); }
	};

}

#endif