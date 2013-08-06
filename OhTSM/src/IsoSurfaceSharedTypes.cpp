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

}
