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
#ifndef __ISOSURFACESHAREDTYPES_H__
#define __ISOSURFACESHAREDTYPES_H__

#include <vector>

#include <boost/thread.hpp>

#include <OgreLog.h>
#include <OgreException.h>

#include "Util.h"
#include "Neighbor.h"

namespace Ogre
{
	typedef unsigned long IsoVertexIndex;
	// Use of unsigned short potentially results in overflow for 32x32x32 cube data regions, but only if half the isovertices are used, which is probably impossible
	typedef unsigned short HWVertexIndex;

	typedef std::vector< IsoVertexIndex > IsoVertexVector;

	typedef unsigned short DimensionType;	// As a stipulation, dimension-type shall not exceed 32
	typedef signed char FieldStrength;

	/** A special data-type that by nature is ordinal but the source benefits from strict compile-time 
		type enforcements so that they are used correctly and in the correct contexts.
		(e.g. assigning a voxel index to a cell index would result in an error whereas without this special
		type relying on a simple "int" primitive would generate no such error.)
	*/
	template< typename Subclass >
	class VoxelGridIndexTemplate
	{
	private:
		unsigned short _index;

	public:
		VoxelGridIndexTemplate () {}
		VoxelGridIndexTemplate(const unsigned short nIndex)
			: _index(nIndex) {}
		VoxelGridIndexTemplate(VoxelGridIndexTemplate && move)
			: _index(move._index) {}

		inline
		operator unsigned short () const { return _index; }

		inline
		Subclass & operator = (const unsigned short nIndex)
		{
			_index = nIndex;
			return *static_cast< Subclass * > (this);
		}

		inline
		Subclass & operator += (const Subclass & delta)
		{
			_index += delta._index;
			return *static_cast< Subclass * > (this);
		}
	};

	/// index of a regular or transition cell in a 3D voxel grid or voxel cube region
	class CellIndex : public VoxelGridIndexTemplate< CellIndex >
	{
	public:
		CellIndex () : VoxelGridIndexTemplate() {}
		CellIndex(const unsigned short nIndex)
			: VoxelGridIndexTemplate(nIndex) {}
		CellIndex(CellIndex && move)
			: VoxelGridIndexTemplate(move) {}
	};

	/// Index to a voxel or grid point in a 3D voxel grid or voxel cube region
	class VoxelIndex : public VoxelGridIndexTemplate< VoxelIndex >
	{
	public:
		VoxelIndex () : VoxelGridIndexTemplate() {}
		VoxelIndex(const unsigned short nIndex)
			: VoxelGridIndexTemplate(nIndex) {}
		VoxelIndex(VoxelIndex && move)
			: VoxelGridIndexTemplate(move) {}
	};

	namespace Voxel
	{
		static const FieldStrength 
			/// Voxel value completely within solid space
			FS_MaxClosed = -127, 
			/// Voxel value completely within open space
			FS_MaxOpen = 127,
			/// Mantissa
			FS_Mantissa = 0x7F;
		static const signed short
			/// Ordinal difference between the completely solid and completely open voxels
			FS_Span = signed short(
				signed short(FS_MaxOpen) 
				- 
				signed short (FS_MaxClosed)
				+
				1
			);
		/// Maximum dimension size of a voxel grid or voxel cube region
		static const DimensionType MAX_DIM = 32;
	}

	struct Matrix3x21
	{
		struct XYD
		{
			unsigned char x, y;
			unsigned short d;
		} x, y, z;

		inline const XYD & operator [] (const unsigned idx) const { return reinterpret_cast< const XYD * > (this) [idx]; }
	};
	/// Maps transition cell coordinates to 3D coordinates compatible with GridCell
	const extern Matrix3x21 Mat2D3D[CountOrthogonalNeighbors];

	/// Maps an orthogonal neighbor to a 3D vector coordinate index
	const extern unsigned OrthogonalNeighbor_to_ComponentIndex[CountOrthogonalNeighbors];

	/// Maps 3D vector coordinate index to the associated orthogonal neighbors
	const extern unsigned ComponentIndex_to_OrthogonalNeighbor[3][2];

	struct Simplex2xSimplex3
	{
		unsigned char simplex[2];
	};
	/// Maps 2-dimensional coordinates to 3-dimensional flags or something
	const extern Simplex2xSimplex3 Simplex2D3D[CountOrthogonalNeighbors];

	/// Isovertex fixed-precision vector type used for representing isovertex position
	typedef FixVector3< 10, signed short > IsoFixVec3;

	/// Coordinate type used to represent voxel coordinates on the face of a 3D voxel grid / cube region in two dimensions
	class CubeSideCoords
	{
	public:
		DimensionType x, y;

		CubeSideCoords() : x(0), y(0) {}
		CubeSideCoords(CubeSideCoords && move)
			: x(move.x), y(move.y) {}
		CubeSideCoords(const DimensionType x, const DimensionType y)
			: x(x), y(y) {}

		inline
		static CubeSideCoords from3D(const OrthogonalNeighbor side, const int i, const int j, const int k)
		{
			return from3D((Moore3DNeighbor)side, i, j, k);
		}

		inline
		static CubeSideCoords from3D(const Moore3DNeighbor side, const int i, const int j, const int k)
		{
			typedef DimensionType DT;
			return
				CubeSideCoords(
					(DT)Mat2D3D[side].x.x * (DT)i + (DT)Mat2D3D[side].y.x * j + (DT)Mat2D3D[side].z.x * k,
					(DT)Mat2D3D[side].x.y * (DT)i + (DT)Mat2D3D[side].y.y * j + (DT)Mat2D3D[side].z.y * k
				);
		}

		inline CubeSideCoords operator - (const CubeSideCoords & csc) const
		{
			return CubeSideCoords(
				x - csc.x,
				y - csc.y
			);
		}
		inline CubeSideCoords operator + (const CubeSideCoords & csc) const
		{
			return CubeSideCoords(
				x + csc.x,
				y + csc.y
			);
		}
		inline CubeSideCoords & operator -= (const CubeSideCoords & csc)
		{
			x -= csc.x;
			y -= csc.y;
			return *this;
		}
		inline CubeSideCoords & operator += (const CubeSideCoords & csc)
		{
			x += csc.x;
			y += csc.y;
			return *this;
		}
		inline CubeSideCoords & operator >>= (const unsigned s)
		{
			x >>= s;
			y >>= s;
			return *this;
		}

		inline CubeSideCoords operator & (const DimensionType mask) const
		{
			return CubeSideCoords(
				x & mask,
				y & mask
			);
		}

		inline CubeSideCoords & operator |= (const CubeSideCoords & csc)
		{
			x |= csc.x;
			y |= csc.y;
			return *this;
		}

		inline bool operator == (const CubeSideCoords & csc) const
		{ return x == csc.x && y == csc.y; }

		inline bool operator < (const CubeSideCoords & other) const
		{ return hash() < other.hash(); }

		inline Ogre::Log::Stream & operator << (Ogre::Log::Stream & out) const { return write(out); }
		inline std::ostream & operator << (std::ostream & out) const { return write(out); }

	private:
		template< typename S > inline S & write (S & outs) const 
		{ return outs << "<" << x << "," << y << ">"; }

		inline unsigned long hash() const
		{ return (unsigned long (y) << 16) | x; }

		friend Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const CubeSideCoords & csc);
		friend std::ostream & operator << (std::ostream & outs, const CubeSideCoords & csc);
	};
	inline Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const CubeSideCoords & csc)
		{ return csc.write(outs); }
	inline std::ostream & operator << (std::ostream & outs, const CubeSideCoords & csc)
		{ return csc.write(outs); }

	/// @returns Flags indicating whether the the coordinates are perfectly aligned according to the specified LOD
	inline
	signed int COARSENESS(const int nLOD, const CubeSideCoords & csc)
	{
		return bitmanip::testZero(signed int((csc.x | csc.y) & ((1 << nLOD) - 1)));
	}

	/// Coordinate type used to represent voxel coordinates / grid point coordinates of a 3D voxel grid / cube region in three dimensions
	class GridPointCoords : public CellCoords< DimensionType, GridPointCoords >
	{
	public:
		GridPointCoords () : CellCoords() {}
		GridPointCoords (GridPointCoords && move)
			: CellCoords(move) {}
		GridPointCoords (const DimensionType i, const DimensionType j, const DimensionType k)
			: CellCoords(i, j, k) {}

		inline 
		operator Vector3 () const {
			return Vector3(
				(Real)i,
				(Real)j,
				(Real)k
			);
		}
	};

	inline Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const GridPointCoords & gpc)
		{ return gpc.write(outs); }
	inline std::ostream & operator << (std::ostream & outs, const GridPointCoords & gpc)
		{ return gpc.write(outs); }

	/// Coordinate type used to represent the coordinates of a grid cell of a 3D voxel grid / cube region in three dimensions
	class GridCellCoords : public CellCoords< DimensionType, GridCellCoords >
	{
	public:
		unsigned lod;

		GridCellCoords (const unsigned nLOD) : CellCoords(), lod(nLOD) {}
		GridCellCoords (GridCellCoords && move)
			: CellCoords(move), lod(move.lod) {}
		GridCellCoords (const DimensionType i, const DimensionType j, const DimensionType k, const unsigned nLOD)
			: CellCoords(i, j, k), lod(nLOD) {}

	private:
		template< typename S > inline S & write (S & outs) const 
			{ return outs << "<" << i << "," << j << "," << k << ';' << lod << ">"; }

		friend Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const GridCellCoords & vc);
		friend std::ostream & operator << (std::ostream & outs, const GridCellCoords & vc);
	};

	inline Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const GridCellCoords & gcc)
		{ return gcc.write(outs); }
	inline std::ostream & operator << (std::ostream & outs, const GridCellCoords & gcc)
		{ return gcc.write(outs); }

	/// Coordinate type used to represent 3D world coordinates in the same units as grid cell coordinates of a 3D voxel grid / cube region
	class WorldCellCoords : public CellCoords< signed int, WorldCellCoords >
	{
	public:
		WorldCellCoords () : CellCoords() {}
		WorldCellCoords (WorldCellCoords && move)
			: CellCoords(move) {}
		WorldCellCoords (const signed int i, const signed int j, const signed int k)
			: CellCoords(i, j, k) {}
	};

	inline Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const WorldCellCoords & wcc)
		{ return wcc.write(outs); }
	inline std::ostream & operator << (std::ostream & outs, const WorldCellCoords & wcc)
		{ return wcc.write(outs); }

	/// Represents the case code of a marching cube configuration
	template< typename T >
	struct BaseNonTrivialCase
	{
		/// The type used for the case-code
		typedef T CodeType;
		/// The cell representing this case
		CellIndex cell;
		/// Combination of flags for all the voxels in the cell
		T casecode;
	};

	typedef BaseNonTrivialCase< unsigned char > NonTrivialRegularCase;
	typedef std::vector< NonTrivialRegularCase > RegularTriangulationCaseList;

	typedef BaseNonTrivialCase< unsigned short > NonTrivialTransitionCase;
	typedef std::vector< NonTrivialTransitionCase > TransitionTriangulationCaseList;

	/// Wrapper class for the Transvoxel regular vertex code type
	class VRECaCC
	{
	protected:
		unsigned char 
			/// Edge-index
			ei, 
			/// Cell locator
			cl, 
			/// First voxel corner of the pair
			c0, 
			/// Second voxel corner of the pair
			c1;

	public:
		inline
		VRECaCC() {}

		inline
		VRECaCC(const unsigned short nVRECaCC)
			: ei((nVRECaCC & 0x0F00) >> 8), cl((nVRECaCC & 0xF000) >> 12), c0 ((nVRECaCC & 0x00F0) >> 4), c1((nVRECaCC & 0x000F) >> 0)
		{}

		inline
		VRECaCC(const VRECaCC & copy)
			: ei(copy.ei), cl(copy.cl), c0(copy.c0), c1(copy.c1) {}

		inline
		VRECaCC(VRECaCC && move)
			: ei(move.ei), cl(move.cl), c0(move.c0), c1(move.c1) {}

		inline
		void operator = (const unsigned short nVRECaCC) 
		{ 
			ei = (nVRECaCC & 0x0F00) >> 8;
			cl = (nVRECaCC & 0xF000) >> 12;
			c0 = (nVRECaCC & 0x00F0) >> 4;
			c1 = (nVRECaCC & 0x000F) >> 0;
		}

		inline
		void operator = (const VRECaCC & vrecacc) 
		{ 
			ei = vrecacc.ei;
			cl = vrecacc.cl;
			c0 = vrecacc.c0;
			c1 = vrecacc.c1;
		}

		inline
		unsigned char getEdgeCode () const { return ei; }

		inline
		void setEdgeCode (const unsigned char ei) { this->ei = ei; }

		inline
		unsigned char getCorner0 () const { return c0; }

		inline
		unsigned char getCorner1 () const { return c1; }

		inline
		unsigned char getCellLocator () const { return cl; }

		friend Ogre::Log::Stream & operator << (Ogre::Log::Stream & s, const VRECaCC & vrecacc);
	};

	/// Wrapper class for the Transvoxel transition vertex code type
	class TransitionVRECaCC : public VRECaCC
	{
	public:
		/// Used to cache transition cell vertices of three types, full-res inside and out and half-res
		/// Values explicitly chosen for bit-fiddling optimization, first-bit denotes half-resolution or not
		enum Type
		{
			TVT_FullInside = 2,
			TVT_FullOutside = 0,
			TVT_Half = 1
		};

		inline
		TransitionVRECaCC() {}

		inline
		TransitionVRECaCC(const unsigned short nVRECaCC)
			: VRECaCC(nVRECaCC) {}

		inline
		TransitionVRECaCC(const TransitionVRECaCC & copy)
			: VRECaCC(copy) {}

		inline
		TransitionVRECaCC(TransitionVRECaCC && move)
			: VRECaCC(move) {}

		/** Retrieve the associated half-resolution edge-index
		@remarks This only applies for vertex codes pointing to an isovertex on the full-resolution face
		@returns The half-resolution edge-index for the full-resolution one noted here-in
		*/
		inline
		unsigned char getHalfResEdgeCode () const 
		{ 
			OgreAssert(getType() != TVT_Half, "Must be a full-resolution transition vertex");
			return 7+((getEdgeCode()-1)/2); 
		}

		/** Retrieve the associated full-resolution edge-index
		@remarks This only applies for vertex codes pointing to an isovertex on the half-resolution face
		@returns The full-resolution edge-index for the half-resolution one noted here-in
		*/
		inline
		unsigned char getFullResEdgeCode () const
		{
			OgreAssert(getType() == TVT_Half, "Must be a half-resolution transition vertex");
			return ((getEdgeCode()-7) << 1)+1;
		}

		/// @returns The type (half, full, or middle) of isovertex determined by the edge code and cell locator
		inline
		Type getType () const 
			{ return Type(((getCellLocator() >> 1) & 0x2) | isfHalfRes()); }

		/// @returns A flag indicating whether this points to a half-resolution isovertex or not using the edge-index
		inline
		unsigned char isfHalfRes() const
			{ return (getEdgeCode() + 1) >> 3; }

		friend Ogre::Log::Stream & operator << (Ogre::Log::Stream & s, const TransitionVRECaCC & vrecacc);
	};

	inline
	Ogre::Log::Stream & operator << (Ogre::Log::Stream & s, const VRECaCC & vrecacc)
	{
		return s << "< EI/C0/C1/CL " << (int)vrecacc.ei << '/' << (int)vrecacc.c0 << '/' << (int)vrecacc.c1 << '/' << (int)vrecacc.cl << " >";
	}
	inline
	Ogre::Log::Stream & operator << (Ogre::Log::Stream & s, const TransitionVRECaCC & vrecacc)
	{
		return s << static_cast< const VRECaCC & > (vrecacc);
	}

	/// Set of properties of an isovertex on a transition cell's full resolution face
	struct BorderIsoVertexProperties 
	{
		/// The side of the 3D voxel grid that the transition cell resides
		OrthogonalNeighbor neighbor;
		/// The isovertex index of the isovertex
		IsoVertexIndex index;
		/// The vertex code of the isovertex
		TransitionVRECaCC vrec;
		/// The side of the 3D voxel grid that the isovertex is flush to
		Touch3DSide touch;
		/// The index of the transition cell that the isovertex is contained by
		CellIndex cell;
#ifdef _DEBUG
		/// The 2-dimensional coordinates of the transition cell
		CubeSideCoords coords;
#endif
	};

	typedef std::vector< BorderIsoVertexProperties > BorderIsoVertexPropertiesVector;

	/// A ray using fixed precision point-origin and direction
	class IsoFixRay
	{
	public:
		IsoFixVec3 origin, direction;

		IsoFixRay(const IsoFixVec3 & orig, const IsoFixVec3 & dir)
			: origin(orig), direction(dir) {}
	};

}

#endif
