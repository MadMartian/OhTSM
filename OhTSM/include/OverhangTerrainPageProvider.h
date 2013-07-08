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

#ifndef __OHTSMPAGEPROV_H__
#define __OHTSMPAGEPROV_H__

#include <Ogre.h>

#include "OverhangTerrainPrerequisites.h"
#include "OverhangTerrainPageInitParams.h"

namespace Ogre
{
	/// A listener class that provides a custom provider with the mechanism to override the default terrain page loading/unloading behavior of this library
	class IOverhangTerrainPageProvider
	{
	public:
		/** Called in background worker thread when a request has been made to load a page
		@remarks Can be used to load data into the page, implementations should first populate the initialization parameters, call the overloaded "<<" operator for both streams and initialization parameters followed by the "conjoin" method.
		@param x Terrain slot x-offset
		@param y Terrain slot y-offset
		@param pInitParams The initialization parameters that must be initialized
		@param pPage The page that must be initialized / populated
		@returns True if the page load operation was handled by this implementation, false if another implementation should receive and process this event */
		virtual bool loadPage (const int16 x, const int16 y, PageInitParams * pInitParams, IOverhangTerrainPage * pPage) = 0;

		/** Called in background worker thread when a request has been made to flush a page to disk, usually just before it is to be unloaded
		@param pPage The page being saved
		@param x Terrain slot x-offset
		@param y Terrain slot y-offset
		@param nPageAxis The size of one edge of a terrain page, in vertices
		@param nTotalPageSize The area of the terrain page, in vertices
		@returns True if the page save operation was handled by this implementation, false if another implementation should receive and process this event */
		virtual bool savePage (const IOverhangTerrainPage * pPage, const int16 x, const int16 y, const size_t nPageAxis, const unsigned long nTotalPageSize) = 0;

		/** Called in main thread when a page is about to be unloaded
		@param x Terrain slot x-offset
		@param y Terrain slot y-offset */
		virtual void unloadPage (const int16 x, const int16 y) = 0;

		/** Called last of all in main thread after page has been fully initialised
		@param x Terrain slot x-offset
		@param y Terrain slot y-offset
		@param pPage The page being saved */
		virtual void preparePage (const int16 x, const int16 y, IOverhangTerrainPage * pPage) = 0;

		/** Called initially in main thread to detach and prepare the page for deletion
		@param x Terrain slot x-offset
		@param y Terrain slot y-offset
		@param pPage The page being saved */
		virtual void detachPage (const int16 x, const int16 y, IOverhangTerrainPage * pPage) = 0;
	};
}

#endif