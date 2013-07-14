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
