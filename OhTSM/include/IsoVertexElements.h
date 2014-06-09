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
#ifndef __ISOVERTEXELEMENTS_H__
#define __ISOVERTEXELEMENTS_H__

#include "OverhangTerrainPrerequisites.h"
#include "IsoSurfaceSharedTypes.h"
#include "Neighbor.h"
#include "CubeDataRegion.h"
#include "Util.h"

#include <vector>

namespace Ogre
{
	/// Definition of a triangle in an iso surface.
	struct IsoTriangle
	{
#ifdef _OHT_LOG_TRACE
		size_t id;
#endif
		/// Iso vertex indices defining the triangle.
		IsoVertexIndex vertices[3];
	};

	typedef std::vector< IsoTriangle > IsoTriangleVector;

	typedef Real TexCoords[2];

	/// Aggregate container type for vertex elements indexed by isovertex index as well as preparation for batching to GPU
	class IsoVertexElements
	{
	public:
		/// Flags describing what data is generated for rendering the iso surface.
		enum SurfaceFlags
		{
			/// Generate vertex normals by interpolating the gradient stored in the data grid.
			GEN_NORMALS = 0x01,
			/// Generate vertex colours by interpolating the colours stored in the data grid.
			GEN_VERTEX_COLOURS = 0x02,
			/// Generate texture coordinates.
			GEN_TEX_COORDS = 0x04,
		};

		/** Hardware vertex buffer indices for all iso vertices.
			@remarks
				A value of 0xFFFF means that the iso vertex is not used. During iso surface generation all
				indices are reset to this value. On the first use of an iso vertex, its parameters are
				calculated, and it is assigned the next index in the hardware vertex buffer. */
		HWVertexIndex* const indices;
		/** Positions of all iso vertices.
			@remarks
				Positions are valid only for used iso vertices. */
		IsoFixVec3* const positions;
		/** Normals for all iso vertices.
			@remarks
				This array is only allocated if GEN_NORMALS is set in IsoSurface::mSurfaceFlags,
				and normals are valid only for used iso vertices. */
		Vector3* const normals;
		/** Vertex colours for all iso vertices.
			@remarks
				This array is only allocated if GEN_VERTEX_COLOURS is set in IsoSurface::mSurfaceFlags,
				and colours are valid only for used iso vertices. */
		ColourValue* const colours;
		/** Texture coordinates for all iso vertices.
			@remarks
				This array is only allocated if GEN_TEX_COORDS is set in IsoSurface::mSurfaceFlags,
				and texture coordinates are valid only for used iso vertices. */
		TexCoords* const texcoords;

		/// Total number of elements
		const size_t count;

		// Maps hardware buffer indices to iso-vertex indices
		IsoVertexVector vertexShipment;

		/** Vector to which all generated iso triangles are added.
		@remarks
			This vector is iterated when filling the hardware index buffer. */
		IsoTriangleVector triangles;
			
		/** Creates the iso vertex arrays.
		@remarks
			The function calls the abstract function getNumIsoVertices() to get the length
			of the arrays to be created. */
		IsoVertexElements(const size_t nNumElements);
		~IsoVertexElements();

		/// Clears the indices member and clears the vertex shipment and triangle queues
		virtual void clear();

		/// Counts the number of triangles from the "triangles" member and multiplies it by three
		virtual size_t indexCount() const;
	};
}

#endif