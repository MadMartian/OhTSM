/*
-----------------------------------------------------------------------------
This source file is part of the OverhangTerrainSceneManager
Plugin for OGRE
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2007 Martin Enge
martin.enge@gmail.com

Modified (2013) by Jonathan Neufeld (http://www.extollit.com) to implement Transvoxel
Transvoxel conceived by Eric Lengyel (http://www.terathon.com/voxels/)

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

-----------------------------------------------------------------------------
*/

#ifndef __METAHEIGHTMAP_H__
#define __METAHEIGHTMAP_H__

#include "OverhangTerrainPrerequisites.h"

#include <OgreAxisAlignedBox.h>
#include <OgreStreamSerialiser.h>

#include "MetaObject.h"

namespace Ogre
{
	/// Class representing a meta height-map that makes-up part of the discrete 3D voxel field
	class _OverhangTerrainPluginExport MetaHeightMap : public MetaObject
	{
	public:
		/// @param t Each meta heightmap must be bound to a terrain tile
		MetaHeightMap(const TerrainTile * t);

		/// Applies this meta heightmap to the voxel grid as discretely sampled voxels
		virtual void updateDataGrid(const Voxel::CubeDataRegion * pDG, Voxel::DataAccessor * pAccess);

		/// Retrieve the bounding box which is vertically bound by the min/max heightmap altitudes
		AxisAlignedBox getAABB () const;

		/// Updates the position of this object from the position of the terrain-tile to which it is bound
		void updatePosition ();

		/// Retrieve the bound terrain tile
		inline
			const TerrainTile * getTerrainTile () const { return _pTerrainTile; }
	
		/// Used for serialization
		virtual MOType getObjectType () const { return MOT_HeightMap; }
		virtual void write(StreamSerialiser & output) const;
		virtual void read(StreamSerialiser & input);

	private:
		/// The bound terrain tile
		const TerrainTile * _pTerrainTile;

		/// Compute the position for a meta-heightmap given a terrain tile
		static Vector3 computePosition (const TerrainTile * pTile);
	};
}/// namespace Ogre
#endif ///_META_HEIGTHMAP_H_