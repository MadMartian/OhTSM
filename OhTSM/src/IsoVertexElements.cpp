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

#include "IsoVertexElements.h"

namespace Ogre
{
	IsoVertexElements::IsoVertexElements( const size_t nNumElements, const char nSurfaceFlags )
		: 
		count(nNumElements),

		indices( new HWVertexIndex[nNumElements] ),
		positions( new IsoFixVec3[nNumElements] ),

		normals( (nSurfaceFlags & GEN_NORMALS) ? new Vector3[nNumElements] : NULL ),
		colours( (nSurfaceFlags & GEN_VERTEX_COLOURS) ? new ColourValue[nNumElements] : NULL ),
		texcoords( (nSurfaceFlags & GEN_TEX_COORDS) ? new TexCoords[nNumElements] : NULL ),
		surfaceFlags(nSurfaceFlags)
	{
	}

	IsoVertexElements::~IsoVertexElements()
	{
		delete[] indices;
		delete[] positions;
		delete[] normals;
		delete[] colours;
		delete[] texcoords;
	}

	void IsoVertexElements::clear()
	{
		memset(indices, ~0, sizeof(HWVertexIndex) * count);
		vertexShipment.clear();
		triangles.clear();
	}
}