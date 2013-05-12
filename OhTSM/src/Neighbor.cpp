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
#include "pch.h"

#include "Neighbor.h"

namespace Ogre
{
	namespace Neighborhood
	{
		unsigned short OPPOSITE_FLIPPERS[CountMoore3DNeighbors][2] = 
		{
			CountVonNeumannNeighbors - 1, 0,
			CountVonNeumannNeighbors - 1, 0,
			CountVonNeumannNeighbors - 1, 0,
			CountVonNeumannNeighbors - 1, 0,

			CountOrthogonalNeighbors - 1, CountVonNeumannNeighbors,
			CountOrthogonalNeighbors - 1, CountVonNeumannNeighbors,

			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors,
			CountMoore3DNeighbors - 1, CountOrthogonalNeighbors
		};

		OrthogonalNeighbor ALT_NEIGHBOR_PATH[CountMoore3DNeighbors][2] = 
		{
			OrthoNaN,OrthoNaN,	OrthoNaN,OrthoNaN,	OrthoNaN,OrthoNaN,	OrthoNaN,OrthoNaN,
			OrthoNaN,OrthoNaN,	OrthoNaN,OrthoNaN,

			OrthoN_ABOVE, OrthoN_NORTH,
			OrthoN_ABOVE, OrthoN_SOUTH,
			OrthoN_ABOVE, OrthoN_EAST,
			OrthoN_ABOVE, OrthoN_WEST,
			OrthoN_NORTH, OrthoN_EAST,
			OrthoN_NORTH, OrthoN_WEST,
			OrthoN_SOUTH, OrthoN_EAST,
			OrthoN_SOUTH, OrthoN_WEST,
			OrthoN_BELOW, OrthoN_EAST,
			OrthoN_BELOW, OrthoN_WEST,
			OrthoN_BELOW, OrthoN_NORTH,
			OrthoN_BELOW, OrthoN_SOUTH,

			OrthoNaN, OrthoNaN,
			OrthoNaN, OrthoNaN,

			OrthoNaN, OrthoNaN,
			OrthoNaN, OrthoNaN
		};

		const char * NAMES[2][CountMoore3DNeighbors] = 
		{
			{
				"N",
				"E",
				"W",
				"S",
				"K",
				"A",
				"KN",
				"KS",
				"KE",
				"KW",
				"NE",
				"NW",
				"SE",
				"SW",
				"AE",
				"AW",
				"AN",
				"AS",
				"KNW",
				"KNE",
				"KSW",
				"KSE",
				"ANW",
				"ANE",
				"ASW",
				"ASE"					
			},
			{
				"NORTH",
				"EAST",
				"WEST",
				"SOUTH",
				"SKY",
				"ABYSS",
				"SKYNORTH",
				"SKYSOUTH",
				"SKYEAST",
				"SKYWEST",
				"NORTHEAST",
				"NORTHWEST",
				"SOUTHEAST",
				"SOUTHWEST",
				"ABYSSEAST",
				"ABYSSWEST",
				"ABYSSNORTH",
				"ABYSSSOUTH",
				"SKYNORTHWEST",
				"SKYNORTHEAST",
				"SKYSOUTHWEST",
				"SKYSOUTHEAST",
				"ABYSSNORTHWEST",
				"ABYSSNORTHEAST",
				"ABYSSSOUTHWEST",
				"ABYSSSOUTHEAST"
			}
		};

		std::string sflags (const size_t nFlags, char cSep /*= '/'*/)
		{
			std::strstream ss;
			bool bFirst = true;

			for (unsigned int c = 0; c < CountMoore3DNeighbors; ++c)
			{
				if (nFlags & (1 << c))
				{
					if (bFirst)
						bFirst = false;
					else
						ss << cSep;

					ss << NAMES[0][c];
				}
			}

			ss << std::ends;

			return ss.str();
		}
	}
}
