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
#ifndef __OVERHANGTERRAINOPTIONS_H__
#define __OVERHANGTERRAINOPTIONS_H__

#include "OverhangTerrainPrerequisites.h"

#include "ChannelIndex.h"

#include <OgreStreamSerialiser.h>
#include <OgreMaterial.h>

namespace Ogre
{
	/// The alignment of the terrain
	enum OverhangTerrainAlignment
	{
		/// Terrain is in the X/Z plane
		ALIGN_X_Z = 0, 
		/// Terrain is in the X/Y plane
		ALIGN_X_Y = 1, 
		/// Terrain is in the Y/Z plane
		ALIGN_Y_Z = 2,

		NumTerrainAlign = 3
	};

	/// Flags describing what data is stored in the data grid.
	enum OverhangTerrainVoxelRegionFlags
	{
		/// The voxel region stores gradient vectors.
		VRF_Gradient = 1 << 0,
		/// The voxel region stores colour values.
		VRF_Colours = 1 << 1,
		/// The voxel region stores texture coordinates.
		VRF_TexCoords = 1 << 2
	};

	/// Type of normals requested during a IsoSurfaceBuilder operation
	enum NormalsType
	{
		/// Don't generate normals
		NT_None,
		/// Normals are calculated as a weighted average of face normals.
		NT_WeightedAverage,
		/// Normals are calculated as an average of face normals.
		NT_Average,
		/// Normals are calculated by interpolating the gradient in the data grid.
		NT_Gradient
	};

	/** Main top-level and base configuration for OhTSM */
    class _OverhangTerrainPluginExport OverhangTerrainOptions
    {
    public:
		/** Channel-specific OhTSM configuration */
		class _OverhangTerrainPluginExport ChannelOptions
		{
		public:
			MaterialPtr material;
			bool materialPerTile;
			
			/// Type of normals that IsoSurfaceBuilder generates
			NormalsType normals;
			/// Whether or not to invert generated normals
			bool flipNormals;
			/// The maximum terrain geo-mipmap level
			size_t maxGeoMipMapLevel;
			/// The maximum pixel error allowed before switching resolutions to compensate
			Real maxPixelError;
			/// Ratio of space that a transition cell takes-up of a normal regular grid cell
			Real transitionCellWidthRatio;

			/// Flags describing what channels of a CubeDataRegion are relevant
			OverhangTerrainVoxelRegionFlags voxelRegionFlags;

			ChannelOptions();
		};

		/// The set of channel-specific configuration options
		Channel::Index< ChannelOptions > channels;

        OverhangTerrainOptions();

		/// The primary camera, used for error metric calculation and page choice
		const Camera* primaryCamera;

		/// The axis that terrain is aligned upon
		OverhangTerrainAlignment alignment;
        /// The size of one edge of a terrain page, in vertices
        size_t pageSize;
        /// The size of one edge of a terrain tile, in vertices
        size_t tileSize; 
        /// The scale factor to apply to the terrain (each vertex is 1 unscaled unit
        /// away from the next, and height is from 0 to 1)
        /// NOTE: Terrain in the world is aligned along the x/z plane
        Real cellScale, heightScale;
		/// Whether each individual terrain tile gets a separate material
		bool materialPerTile;
		/// Whether to automatically save dirty pages upon unloading
		bool autoSave;

		/// The area of the terrain page, in vertices
		inline const ulong getTotalPageSize() const { return pageSize * pageSize; }

		/// Retrieves the number of tiles per page along one axis
		inline const uint getTilesPerPage () const { return (pageSize - 1) / (tileSize - 1); }

		/// Gets the length of a terrain page along one edge in world units
		inline Real getPageWorldSize () const { return static_cast< Real > ((pageSize - 1) * cellScale); }

		/// Gets the length of a terrain tile along one edge in world units
		inline Real getTileWorldSize () const { return static_cast< Real > ((tileSize - 1) * cellScale); }

		/// Saves this object to the stream except for the primary camera
		StreamSerialiser & operator << (StreamSerialiser & stream);
		/// Restores this object from the stream except for the primary camera
		StreamSerialiser & operator >> (StreamSerialiser & stream) const;

	private:
		/// Used for serialization
		static const uint32 CHUNK_ID;
		static const uint16 CHUNK_VERSION;
    };
}

#endif