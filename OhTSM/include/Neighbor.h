/*
-----------------------------------------------------------------------------
This source file is part of the Overhang Terrain Scene Manager library (OhTSM)
for use with OGRE.

Copyright (c) 2013 extollIT Enterprises
http://www.extollit.com

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
-----------------------------------------------------------------------------
*/
#ifndef __OVERHANGNEIGHBORSTUFF_H__
#define __OVERHANGNEIGHBORSTUFF_H__

#include "OverhangTerrainPrerequisites.h"

#include <string>
#include <strstream>

namespace Ogre
{
	/// Enums for a Von Neumann neighborhood
	enum VonNeumannNeighbor
	{
		VonN_NORTH = 0,
		VonN_EAST = 1,
		VonN_WEST = 2,
		VonN_SOUTH = 3,
			
		CountVonNeumannNeighbors = 4,
		VonNaN = ~0
	};

	/// Superset of Von Neumann neighborhood, enumerates a 3-dimensional axis-aligned neighborhood analogous to the 6 sides of a cube
	enum OrthogonalNeighbor
	{
		OrthoN_NORTH = VonN_NORTH,
		OrthoN_EAST = VonN_EAST,
		OrthoN_WEST = VonN_WEST,
		OrthoN_SOUTH = VonN_SOUTH,

		OrthoN_ABOVE = 0 + CountVonNeumannNeighbors,
		OrthoN_BELOW = 1 + CountVonNeumannNeighbors,

		CountOrthogonalNeighbors = 2 + CountVonNeumannNeighbors,
		OrthoNaN = ~0
	};

	/// Enums for the 4 corners of a square box
	enum BoxCorners
	{
		BoxC_NW = 0,
		BoxC_NE = 1,
		BoxC_SW = 2,
		BoxC_SE = 3,

		CountBoxCorners = 4
	};

	/// Enums for the 4 edges of a square box
	enum BoxEdges
	{
		BoxE_TOP = VonN_NORTH,
		BoxE_BOTTOM = VonN_SOUTH,
		BoxE_LEFT = VonN_WEST,
		BoxE_RIGHT = VonN_EAST,

		CountBoxEdges = CountVonNeumannNeighbors
	};

	/// Superset of orthogonal neighbors, enumerates all possible neighbors of a 3-dimensional cube that correspond to faces, line segments, and corners
	enum Moore3DNeighbor
	{
		Moore3N_NORTH = OrthoN_NORTH,
		Moore3N_EAST = OrthoN_EAST,
		Moore3N_WEST = OrthoN_WEST,
		Moore3N_SOUTH = OrthoN_SOUTH,

		Moore3N_ABOVE = OrthoN_ABOVE,
		Moore3N_BELOW = OrthoN_BELOW,

		Moore3N_ABOVENORTH = 0 + CountOrthogonalNeighbors,
		Moore3N_ABOVESOUTH = 1 + CountOrthogonalNeighbors,
		Moore3N_ABOVEEAST = 2 + CountOrthogonalNeighbors,
		Moore3N_ABOVEWEST = 3 + CountOrthogonalNeighbors,
		Moore3N_NORTHEAST = 4 + CountOrthogonalNeighbors,
		Moore3N_NORTHWEST = 5 + CountOrthogonalNeighbors,
		Moore3N_SOUTHEAST = 6 + CountOrthogonalNeighbors,
		Moore3N_SOUTHWEST = 7 + CountOrthogonalNeighbors,
		Moore3N_BELOWEAST = 8 + CountOrthogonalNeighbors,
		Moore3N_BELOWWEST = 9 + CountOrthogonalNeighbors,
		Moore3N_BELOWNORTH = 10 + CountOrthogonalNeighbors,
		Moore3N_BELOWSOUTH = 11 + CountOrthogonalNeighbors,

		Moore3N_ABOVENORTHWEST = 12 + CountOrthogonalNeighbors,
		Moore3N_ABOVENORTHEAST = 13 + CountOrthogonalNeighbors,
		Moore3N_ABOVESOUTHWEST = 14 + CountOrthogonalNeighbors,
		Moore3N_ABOVESOUTHEAST = 15 + CountOrthogonalNeighbors,
		Moore3N_BELOWNORTHWEST = 16 + CountOrthogonalNeighbors,
		Moore3N_BELOWNORTHEAST = 17 + CountOrthogonalNeighbors,
		Moore3N_BELOWSOUTHWEST = 18 + CountOrthogonalNeighbors,
		Moore3N_BELOWSOUTHEAST = 19 + CountOrthogonalNeighbors,

		BeginNonOrthogonals = 0 + CountOrthogonalNeighbors,
		BeginMoore3DEdges = 0 + CountOrthogonalNeighbors,
		BeginMoore3DCorners = 12 + CountOrthogonalNeighbors,
		CountMoore3DEdges = 12,
		CountMoore3DCorners = 8,
		CountMoore3DNeighbors = CountMoore3DEdges + CountMoore3DCorners + CountOrthogonalNeighbors,
		Moore3NaN = ~0,
		NonOrthogonalsMask = ~((1 << BeginNonOrthogonals) - 1),
		OrthogonalMask = ~NonOrthogonalsMask
	};

	/// Bit flags corresponding to Moore3Neighbor types
	enum NeighborFlag
	{
		NF_NORTH = (1 << Moore3N_NORTH),
		NF_EAST = (1 << Moore3N_EAST),
		NF_WEST = (1 << Moore3N_WEST),
		NF_SOUTH = (1 << Moore3N_SOUTH),

		NF_SKY = (1 << Moore3N_ABOVE),
		NF_ABYSS = (1 << Moore3N_BELOW),

		NF_SKYNORTH = (1 << Moore3N_ABOVENORTH),
		NF_SKYSOUTH = (1 << Moore3N_ABOVESOUTH),
		NF_SKYEAST = (1 << Moore3N_ABOVEEAST),
		NF_SKYWEST = (1 << Moore3N_ABOVEWEST),
		NF_NORTHEAST = (1 << Moore3N_NORTHEAST),
		NF_NORTHWEST = (1 << Moore3N_NORTHWEST),
		NF_SOUTHEAST = (1 << Moore3N_SOUTHEAST),
		NF_SOUTHWEST = (1 << Moore3N_SOUTHWEST),
		NF_ABYSSEAST = (1 << Moore3N_BELOWEAST),
		NF_ABYSSWEST = (1 << Moore3N_BELOWWEST),
		NF_ABYSSNORTH = (1 << Moore3N_BELOWNORTH),
		NF_ABYSSSOUTH = (1 << Moore3N_BELOWSOUTH),

		NF_SKYNORTHWEST = (1 << Moore3N_ABOVENORTHWEST),
		NF_SKYNORTHEAST = (1 << Moore3N_ABOVENORTHEAST),
		NF_SKYSOUTHWEST = (1 << Moore3N_ABOVESOUTHWEST),
		NF_SKYSOUTHEAST = (1 << Moore3N_ABOVESOUTHEAST),
		NF_ABYSSNORTHWEST = (1 << Moore3N_BELOWNORTHWEST),
		NF_ABYSSNORTHEAST = (1 << Moore3N_BELOWNORTHEAST),
		NF_ABYSSSOUTHWEST = (1 << Moore3N_BELOWSOUTHWEST),
		NF_ABYSSSOUTHEAST = (1 << Moore3N_BELOWSOUTHEAST)
	};

	namespace Neighborhood
	{
		/// Maps neighbors to their mirror-opposites presuming the subject as center
		extern _OverhangTerrainPluginExport unsigned short OPPOSITE_FLIPPERS[CountMoore3DNeighbors][2];
		/** Maps each Moore3DNeighbor flush to the edge of a cubical space (analogous to a hypotenuse) 
			to two sets of two orthogonal neighbors (analogous to opposite and adjacent and vice versa)
			that constitute an axis-aligned path to the same neighbor */
		extern _OverhangTerrainPluginExport OrthogonalNeighbor ALT_NEIGHBOR_PATH[CountMoore3DNeighbors][2];
		/// Maps neighbor constants to string names and abbreviations
		extern _OverhangTerrainPluginExport const char * NAMES[2][CountMoore3DNeighbors];

		/// @returns The VonNeumannNeighbor mirror "flipped" version of the one specified
		inline VonNeumannNeighbor opposite (const VonNeumannNeighbor enn) 
			{ return static_cast< VonNeumannNeighbor > (enn ^ 3); }
		/// @returns The OrthogonalNeighbor mirror "flipped" version of the one specified
		inline OrthogonalNeighbor opposite (const OrthogonalNeighbor enn) 
			{ return static_cast< OrthogonalNeighbor > (enn ^ (3 ^ ((enn & 4) >> 1))); }
		/// @returns The Moore3DNeighbor mirror "flipped" version of the one specified
		inline Moore3DNeighbor opposite (const Moore3DNeighbor enn) 
			{ return static_cast< Moore3DNeighbor > (OPPOSITE_FLIPPERS[enn][0] - enn + OPPOSITE_FLIPPERS[enn][1]); }

		/** For the Moore3DNeighbor flush to the edge of a cubical space (analogous to a hypotenuse) 
			returns one of the two sets of two orthogonal neighbors (analogous to opposite and adjacent and vice versa)
			that constitute an axis-aligned path to the same Moore3DNeighbor */
		template< int I >
		inline OrthogonalNeighbor orthopath (const Moore3DNeighbor encn)
		{
			OgreAssert(encn >= CountOrthogonalNeighbors && encn < BeginMoore3DCorners, "Only valid for non-orthogonal neighbor types of two steps to the moore3");
			return ALT_NEIGHBOR_PATH[encn][I];
		}

		/// @returns NeighborFlag corresponding to the specified VonNeumannNeighbor
		inline NeighborFlag flag(const VonNeumannNeighbor enn) { return static_cast< NeighborFlag > (1 << enn); }
		/// @returns NeighborFlag corresponding to the specified OrthogonalNeighbor
		inline NeighborFlag flag(const OrthogonalNeighbor enn) { return static_cast< NeighborFlag > (1 << enn); }
		/// @returns NeighborFlag corresponding to the specified Moore3DNeighbor
		inline NeighborFlag flag(const Moore3DNeighbor enn) { return static_cast< NeighborFlag > (1 << enn); }

		/// @returns The string name of the specified VonNeumannNeighbor
		inline const char * name (const VonNeumannNeighbor enn) { return enn == VonNaN ? "?" : NAMES[1][enn]; }
		/// @returns The string name of the specified OrthogonalNeighbor
		inline const char * name (const OrthogonalNeighbor enn) { return enn == OrthoNaN ? "?" : NAMES[1][enn]; }
		/// @returns The string name of the specified Moore3DNeighbor
		inline const char * name (const Moore3DNeighbor enn) { return enn == Moore3NaN ? "?" : NAMES[1][enn]; }

		/// @returns The string abbreviated name of the specified VonNeumannNeighbor
		inline const char * abbrev (const VonNeumannNeighbor enn) { return enn == VonNaN ? "?" : NAMES[0][enn]; }
		/// @returns The string abbreviated name of the specified OrthogonalNeighbor
		inline const char * abbrev (const OrthogonalNeighbor enn) { return enn == OrthoNaN ? "?" : NAMES[0][enn]; }
		/// @returns The string abbreviated name of the specified Moore3DNeighbor
		inline const char * abbrev (const Moore3DNeighbor enn) { return enn == Moore3NaN ? "?" : NAMES[0][enn]; }

		_OverhangTerrainPluginExport std::string sflags (const size_t nFlags, char cSep = '/');
	}
}

#endif