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
#include "Util.h"

namespace Ogre
{
	/// Class representing a meta height-map that makes-up part of the discrete 3D voxel field
	class _OverhangTerrainPluginExport MetaHeightMap : public MetaObject
	{
	public:
		MetaHeightMap();
		~MetaHeightMap();

		/** Configure this meta-heightmap with a DEM
		@param pHM The DEM, across and down, non-interlaced, scanline is map width
		@param width The width of the DEM, also the scanline size
		@param depth Depth of the heightmap, also number of scanlines
		@param hscale Units to scale the map coverage area by (horizontal scaling)
		@param vscale Units to scale the DEM by (vertical scaling) */
		void load (Real * pHM, const size_t width, const size_t depth, const Real hscale, const Real vscale);

		/// Applies this meta heightmap to the voxel grid as discretely sampled voxels
		virtual void updateDataGrid(const Voxel::CubeDataRegion * pDG, Voxel::DataAccessor * pAccess);

		/// Retrieve the bounding box which is vertically bound by the min/max heightmap altitudes
		AxisAlignedBox getAABB () const;

		/** Determines the minimum and maximum DEM values enclosed by the region specified.
		@param x0 Minimal horizontal x offset
		@param z0 Minimal horizontal z offset
		@param xN Maximal horizontal x offset (inclusive)
		@param zN Maximal horizontal z offset (inclusive)
		@param min Receives the minimum DEM value
		@param max Receives the maximum DEM value */
		void span(const size_t x0, const size_t z0, const size_t xN, const size_t zN, Real & min, Real & max) const;

		/** Determines the union of the minimum and maximum DEM values enclosed by the region specified with the input min/max values.
		@remarks This method takes into account the input values of 'min' and 'max' to compute a union of those values with the values derived from the DEM
		@param x0 Minimal horizontal x offset
		@param z0 Minimal horizontal z offset
		@param xN Maximal horizontal x offset (inclusive)
		@param zN Maximal horizontal z offset (inclusive)
		@param min In/Out minimum DEM value
		@param max In/Out maximum DEM value */
		void unyion(const size_t x0, const size_t z0, const size_t xN, const size_t zN, Real & min, Real & max) const;
		/// Computes the bounding-box intersection of the DEM with the specified bounding-box, presently ignores the bbox y-coordinate and assumes +/- infinity
		virtual void intersection(AxisAlignedBox & bbox) const;
	
		/// Used for serialization
		virtual MOType getObjectType () const { return MOT_HeightMap; }
		virtual void write(StreamSerialiser & output) const;
		virtual void read(StreamSerialiser & input);

		/// @returns Discretely samples the heightmap altitude at the specified 2-dimension coordinates that coincide with the arrangement of the cross-section of all voxels occurring in the page to which this meta-heightmap belongs
		inline Real height (const signed int x, const signed int y) const
		{
			using bitmanip::clamp;
			return _vHeightmap[clamp(0, (signed int)_d1, y) * _width + clamp(0, (signed int)_w1, x)] * _vscale;
		}

	private:
		/// Discrete heightmap field from which voxel grids making-up the page to which this meta-heightmap belongs are created
		Real * _vHeightmap, 
			/// Units to scale horizontal coverage area of the heightmap, and vertical scaling of the DEM values
			_hscale, _vscale;

		/// Current bounding box of the heightmap in world-space coordinates relative to page
		AxisAlignedBox _bbox;

		/// Width and depth of the heightmap corresponding to the horizontal size of a page
		size_t _width, _depth, _w1, _d1;
	};
}/// namespace Ogre
#endif ///_META_HEIGTHMAP_H_