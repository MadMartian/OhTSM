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
#ifndef __OVERHANGTERRAINTILEINITPARAMS_H__
#define __OVERHANGTERRAINTILEINITPARAMS_H__

#include <OgreCommon.h>
#include <OgreMaterial.h>

#include <vector>

#include "OverhangTerrainOptions.h"

namespace Ogre
{
	/// A storage class for meta-information necessary to populate a page of terrain data with renderable content
	class PageInitParams
	{
	public:
		/// Parameters specifically intended for an individual terrain-tile in a page
		struct TileParams
		{
			/// Same as PageInitParams::heightmap
			Real * heightmap;
			/// The tile-offset in the 2D-array of terrain-tiles per page
			size_t vx0, vy0;
			/// The colour heightmap (if applicable)
			ColourValue * colourmap;
			/// The material to use for this terrain-tile
			MaterialPtr material;
		};

		/// Heightmap for the page which should be voxelized and transformed into extracted isosurfaces
		Real* heightmap; 

		/// The 2D page index
		const int16 pageX, pageY;

		const size_t 
			/// The number of terrain-tiles that occur along one side of a terrain page
			countTilesPerPageSide, 
			/// The maximum number of vertices that can occur along one axis of an isosurface
			countVerticesPerTileSide,
			/// The sum of the maximum number of vertices that can occur along one axis of an isosurface along one side of a page
			countVerticesPerPageSide,
			/// The total maximum number of vertices that can occur in a horizontal cross-section of a terrain page
			countVerticesPerPage;

		/** 
		@param options The main top-level OhTSM configuration properties
		@param pageX X-component of the 2D page index
		@param pageY Y-component of the 2D page index */
		PageInitParams (const OverhangTerrainOptions & options, const int16 pageX, const int16 pageY);

		/// @returns The material to use for the terrain tile at the specified 2D array offset
		inline const MaterialPtr & material (const size_t i, const size_t j) const
			{ return (*_pMaterials)[getTileIndex(i, j)]; }
		/// @returns The material to use for the terrain tile at the specified 2D array offset
		inline MaterialPtr & material (const size_t i, const size_t j)
			{ return (*_pMaterials)[getTileIndex(i, j)]; }

		/// @returns The field of colours to use for the terrain tile at the specified 2D array offset
		inline const ColourValue * colourmap (const size_t i, const size_t j) const
			{ return (*_pColourMaps)[getTileIndex(i, j)]; }
		/// @returns The field of colours to use for the terrain tile at the specified 2D array offset
		inline ColourValue * colourmap (const size_t i, const size_t j)
			{ return (*_pColourMaps)[getTileIndex(i, j)]; }

		/// @returns The parameters pertinent to the terrain tile at the specified 2D array offset
		TileParams getTile (const size_t i, const size_t j);
		/// @returns The parameters pertinent to the terrain tile at the specified 2D array offset
		const TileParams getTile (const size_t i, const size_t j) const;

		/// @returns True if materials-per-tile are available
		inline bool isMaterials () const { return _pMaterials != NULL; }
		/// @returns True if colours-per-tile are available
		inline bool isColourMap () const { return _pColourMaps != NULL; }

		/// Stores these parameters to the stream excluding materials
		void operator << (StreamSerialiser & ins);
		/// Reads parameters from the stream excluding materials
		void operator >> (StreamSerialiser & outs) const;

		~PageInitParams();

	private:
		/// Optional materials to use per terrain-tile in a page
		std::vector< MaterialPtr > * _pMaterials;
		/// Optional colour fields to use per terrain-tile in a page
		std::vector< ColourValue * > * _pColourMaps;

		/** Populates the object with heightmap, colour, material etc. data for populating a terrain-tile
		@param params Will receive initialization details that can be used to populate a terrain-tile
		@param i The horizontal component of the 2D index of the terrain-tile the parameters are intended for
		@param j The vertical component of the 2D index of the terrain-tile the parameters are intended for */
		void populateTileParams (TileParams & params, const size_t i, const size_t j) const;

		/// @returns Converts the specified 2D array index of terrain-tile in page to a scalar index value
		inline size_t getTileIndex (const size_t i, const size_t j) const
			{ return j * countTilesPerPageSide + i;	}
	};
}

#endif
