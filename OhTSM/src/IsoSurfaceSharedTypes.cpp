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

#include "pch.h"

#include "IsoSurfaceSharedTypes.h"
#include "DebugTools.h"

namespace Ogre
{
	const char * Touch3DFlagNames[CountTouch3DSides] = {
		"",
		"W",
		"E",
		"WE",
		"B",
		"WB",
		"EB",
		"WEB",
		"A",
		"WA",
		"EA",
		"WEA",
		"BA",
		"WBA",
		"EBA",
		"WEBA",
		"N",
		"WN",
		"EN",
		"WEN",
		"BN",
		"WBN",
		"EBN",
		"WEBN",
		"AN",
		"WAN",
		"EAN",
		"WEAN",
		"BAN",
		"WBAN",
		"EBAN",
		"WEBAN",
		"S",
		"WS",
		"ES",
		"WES",
		"BS",
		"WBS",
		"EBS",
		"WEBS",
		"AS",
		"WAS",
		"EAS",
		"WEAS",
		"BAS",
		"WBAS",
		"EBAS",
		"WEBAS",
		"NS",
		"WNS",
		"ENS",
		"WENS",
		"BNS",
		"WBNS",
		"EBNS",
		"WEBNS",
		"ANS",
		"WANS",
		"EANS",
		"WEANS",
		"BANS",
		"WBANS",
		"EBANS",
		"WEBANS"
	};
	const signed char Touch3DSide_to_Moore3DNeighbor[CountTouch3DSides] =
	{
		Moore3NaN,
		Moore3N_WEST,
		Moore3N_EAST,
		Moore3NaN,
		Moore3N_BELOW,
		Moore3N_BELOWWEST,
		Moore3N_BELOWEAST,
		Moore3N_BELOW,
		Moore3N_ABOVE,
		Moore3N_ABOVEWEST,
		Moore3N_ABOVEEAST,
		Moore3N_ABOVE,
		Moore3NaN,
		Moore3N_WEST,
		Moore3N_EAST,
		Moore3NaN,
		Moore3N_NORTH,
		Moore3N_NORTHWEST,
		Moore3N_NORTHEAST,
		Moore3N_NORTH,
		Moore3N_BELOWNORTH,
		Moore3N_BELOWNORTHWEST,
		Moore3N_BELOWNORTHEAST,
		Moore3N_BELOWNORTH,
		Moore3N_ABOVENORTH,
		Moore3N_ABOVENORTHWEST,
		Moore3N_ABOVENORTHEAST,
		Moore3N_ABOVENORTH,
		Moore3N_NORTH,
		Moore3N_NORTHWEST,
		Moore3N_NORTHEAST,
		Moore3N_NORTH,
		Moore3N_SOUTH,
		Moore3N_SOUTHWEST,
		Moore3N_SOUTHEAST,
		Moore3N_SOUTH,
		Moore3N_BELOWSOUTH,
		Moore3N_BELOWSOUTHWEST,
		Moore3N_BELOWSOUTHEAST,
		Moore3N_BELOWSOUTH,
		Moore3N_ABOVESOUTH,
		Moore3N_ABOVESOUTHWEST,
		Moore3N_ABOVESOUTHEAST,
		Moore3N_ABOVESOUTH,
		Moore3N_SOUTH,
		Moore3N_SOUTHWEST,
		Moore3N_SOUTHEAST,
		Moore3N_SOUTH,
		Moore3NaN,
		Moore3N_WEST,
		Moore3N_EAST,
		Moore3NaN,
		Moore3N_BELOW,
		Moore3N_BELOWWEST,
		Moore3N_BELOWEAST,
		Moore3N_BELOW,
		Moore3N_ABOVE,
		Moore3N_ABOVEWEST,
		Moore3N_ABOVEEAST,
		Moore3N_ABOVE,
		Moore3NaN,
		Moore3N_WEST,
		Moore3N_EAST,
		Moore3NaN
	};

	const Touch3DSide OrthogonalNeighbor_to_Touch3DSide[CountOrthogonalNeighbors] =
	{
		T3DS_North /*OrthoN_NORTH*/,
		T3DS_East /*OrthoN_EAST*/,
		T3DS_West /*OrthoN_WEST*/,
		T3DS_South /*OrthoN_SOUTH*/,
		T3DS_Aether /*OrthoN_ABOVE*/,
		T3DS_Nether /*OrthoN_BELOW*/		
	};

	const Matrix3x21  
		Mat2D3D[CountOrthogonalNeighbors] =
	{
		{ // North
			{ 1, 0,		0 },
			{ 0, 1,		0 },
			{ 0, 0,		0 }
		},
		{ // East
			{ 0, 0,		0xFFFF },
			{ 0, 1,		0 },
			{ 1, 0,		0 }
		},
		{ // West
			{ 0, 0,		0 },
			{ 0, 1,		0 },
			{ 1, 0,		0 }
		},
		{ // South
			{ 1, 0,		0 },
			{ 0, 1,		0 },
			{ 0, 0,		0xFFFF }
		},
		{ // Above
			{ 1, 0,		0 },
			{ 0, 0,		0xFFFF },
			{ 0, 1,		0 }
		},
		{ // Below
			{ 1, 0,		0 },
			{ 0, 0,		0 },
			{ 0, 1,		0 }
		}
	};

	const Simplex2xSimplex3 
		Simplex2D3D[CountOrthogonalNeighbors] =
	{
		{ 0, 1 },		// North
		{ 2, 1 },		// East
		{ 2, 1 },		// West
		{ 0, 1 },		// South
		{ 0, 2 },		// Above
		{ 0, 2 }		// Below
	};
	
	void RayCellWalk::updateLOD( const unsigned nLOD )
	{
		OgreAssert(nLOD >= lod._lod, "Cannot go backwards with LOD");
		const unsigned m = ((1 << nLOD) - 1);
		const unsigned ispan0 = 1 << lod._lod;
		_ispan = 1 << nLOD;
		_fspan = (Real)_ispan;
		
		// Adjust the walker by the spatial difference between cells of different LOD at the wcell coordinates
		_walker.x += Real(((_cell.i & m) & ~_delta.mx) | (((_ispan - ((_cell.i + ispan0) & m)) % _ispan) & _delta.mx));
		_walker.y += Real(((_cell.j & m) & ~_delta.my) | (((_ispan - ((_cell.j + ispan0) & m)) % _ispan) & _delta.my));
		_walker.z += Real(((_cell.k & m) & ~_delta.mz) | (((_ispan - ((_cell.k + ispan0) & m)) % _ispan) & _delta.mz));

		_cell &= ~m;
	}

	Ogre::Vector3 RayCellWalk::getPosition() const
	{
		Vector3 v = DiscreteRayIterator::getPosition();

		v.x += (Real)((_delta.mx & 1) << lod._lod);
		v.y += (Real)((_delta.my & 1) << lod._lod);
		v.z += (Real)((_delta.mz & 1) << lod._lod);

		return v;
	}

	RayCellWalk::RayCellWalk( const Vector3 & ptOrigin, const Vector3 & direction, const Real limit /*= 0*/ )
		: DiscreteRayIterator(ptOrigin, direction, limit)
	{
		lod._pWalker = this;

		_delta.mx = (((_delta.x + 1) >> 1) - 1);
		_delta.my = (((_delta.y + 1) >> 1) - 1);
		_delta.mz = (((_delta.z + 1) >> 1) - 1);
	}

	unsigned RayCellWalk::LOD::operator = ( const unsigned nLOD )
	{
		_pWalker->updateLOD(nLOD);
		return _lod = nLOD;
	}

	RayCellWalk::LOD::LOD() 
		: _lod(0) {}
}
