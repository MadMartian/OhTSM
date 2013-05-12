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
#ifndef __OVERHANGTERRAINPAGE_H__
#define __OVERHANGTERRAINPAGE_H__

#include <OgreVector3.h>
#include <OgreAxisAlignedBox.h>
#include <OgreSceneNode.h>
#include <OgreStreamSerialiser.h>

#include "OverhangTerrainPrerequisites.h"

namespace Ogre
{
	/** Provides limited access to a page of terrain in the terrain group */
	class _OverhangTerrainPluginExport IOverhangTerrainPage
	{
	public:
		/// Applies the specified initialization parameters to the page which primarily involves voxelizing the heightmap
		virtual void operator << (const PageInitParams & params) = 0;

		/** Voxelizes the page and links-up tiles
			@see PageSection::conjoin() */
		virtual void conjoin() = 0;
		/** Determines if the page is inconsistent with its state on disk */
		virtual bool isDirty() const = 0;

		/// Adds a listener to the page to receive events fired by this page
		virtual void addListener (IOverhangTerrainListener * pListener) = 0;
		/// Removes a listener from the page previously added
		virtual void removeListener (IOverhangTerrainListener * pListener) = 0;

		/// @returns The center of the page according to its parent node in the scene
		virtual Vector3 getPosition() const = 0;

		/// @returns The bounding box of the page according to the scene
		virtual const AxisAlignedBox & getBoundingBox() const = 0;

		// TODO: MetaWorldFragmentIterator iterateMetaFrags ();

		/// @returns The scene node that this page has renderables attached to as children
		virtual const SceneNode * getSceneNode () const = 0;
		/// @returns The scene node that this page has renderables attached to as children
		virtual SceneNode * getSceneNode () = 0;

		/// Writes the page and all of its contents (including meta-fragments and voxel grids) to the stream
		virtual StreamSerialiser & operator >> (StreamSerialiser & output) const = 0;
		/// Reads a page representation and contents into this object (including meta-fragments and voxel grids) from the stream
		virtual StreamSerialiser & operator << (StreamSerialiser & input) = 0;

	};
}

#endif