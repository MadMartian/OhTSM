/*
-----------------------------------------------------------------------------
This source file is part of the OverhangTerrainSceneManager
Plugin for OGRE
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2007 Martin Enge. Based on code from DWORD, released into public domain.
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

#ifndef __OVERHANGTERRAINTEMPLATE4DATAGRID_H__
#define __OVERHANGTERRAINTEMPLATE4DATAGRID_H__

#include "OverhangTerrainPrerequisites.h"

#include <OgreException.h>

#include "Util.h"
#include "Neighbor.h"

#include "IsoSurfaceSharedTypes.h"

namespace Ogre
{
	namespace Voxel 
	{
		/** Data container for a cubical region of voxels including voxel values, gradient, and colours */
		class _OverhangTerrainPluginExport DataBase
		{
		private:
			DataBase (const DataBase &);

		public:
			const size_t count;
			FieldStrength * values;
			signed char * dx, *dy, *dz;
			unsigned char * red, * green, * blue, * alpha;

			DataBase(const size_t nCount);
			virtual ~DataBase();
		};

		/** A memory pool pattern for DataBase instances to eliminate allocation/deallocation 
			and improve performance */
		class _OverhangTerrainPluginExport DataBaseFactory
		{
		private:
			typedef std::list< DataBase * > DataBaseList;

			mutable boost::mutex _mutex;

			/// How much to grow the pool each iteration and the voxel count per DataBase instance
			const size_t _nGrowBy, _nBucketElementCount;
			/// Unused and checked-out pools
			DataBaseList _pool, _leased;

			/// Expands the pool by an amount
			void growBy(const size_t nAmt);

			// Copying a factory is nonsensical
			DataBaseFactory(const DataBaseFactory &);

		public:
			/** Exception class used to enforce consistency, thrown from destructor when objects are still
				checked-out of the pool and thrown during retire if the specified instance was not previously
				checked-out. */
			class LeaseEx : public std::exception 
			{
			public:
				LeaseEx(const char * szMsg);
			};

			DataBaseFactory(const size_t nBucketElementCount, const size_t nInitialPoolCount = 4, const size_t nGrowBy = 1);

			/// Check-out an instance
			DataBase * lease ();
			/// Check-in an instance
			void retire (const DataBase * pDataBase);

			~DataBaseFactory();
		};

		/** Singleton pattern meta-information for a cubical region of voxels in the scene */
		class _OverhangTerrainPluginExport CubeDataRegionDescriptor
		{
		public:
			/// Translation vector for converting voxel or cell coordinates into an index
			const struct IndexTx
			{
			public:
				size_t mz, my, mx;
			} coordsIndexTx, cellIndexTx;

			/// Flags describing what data is stored in the data grid.
			enum GridFlags
			{
				/// The data grid stores gradient vectors.
				HAS_GRADIENT = 0x01,
				/// The data grid stores colour values.
				HAS_COLOURS = 0x02
			};

			/// Height, width, and depth of the grid
			const DimensionType dimensions;
			/// Total voxels and cells per region, total voxels and cells on a side of a region
			const DimensionType gpcount, cellcount, sidegpcount, sidecellcount;
			/// The scale of grid cells; this influences the position of grid vertices.
			const Real scale;

			mutable DataBaseFactory factoryDB;

			/** 
			@param nVertexDimensions The number of voxels in one direction, nTileSize^3 defines the total number of voxels per cube
			@param gridScale World-size of a single cell, defines the world-size of a cube
			@param nFlags OR'd GridFlags
			*/
			CubeDataRegionDescriptor(const DimensionType nVertexDimensions, const Real gridScale, const int nFlags);

			/** Computes whether a coordinate is flush with a minimal or maximal edge as bounded by the dimensions
			or neither.  The result is a pair of bit flags indicating the result that also conveniently double
			as an index described by the TouchStatus enumeration:
			- Bit #0: Set if coordinate is flush with minimal edge
			- Bit #1: Set if coordinate is flush with maximal edge
			Obviously it is impossible for both bits to be set for a non-zero bounded range.
			If all bits are cleared then the coordinate is not flush with either bound/edge.
			*/
			inline 
				TouchStatus getTouchStatus (const DimensionType v) const
			{
				const register DimensionType m = (dimensions - 1) & v;
				static_assert(sizeof(DimensionType) == 2, "Expected DimensionType to be a WORD type");
				return
					TouchStatus(
					((m - 1) & ~m & 0x8000) >> (14 + (0x1 ^ (v >> _nDimOrder2)))
					);
			}

			/** Computes whether a 2D pair of coordinates are flush with a minimal or maximal edge or corner as bounded by the dimensions
			or neither.  This method calls computeTouchStatus for each of the two coordinates and combines the results into one nibble,
			the higher two bits represent the y-component and the lower two bits represent the x-component.
			*/
			inline 
				Touch2DSide getTouchSide (const DimensionType x, const DimensionType y) const
			{
				const TouchStatus 
					tsx = getTouchStatus(x),
					tsy = getTouchStatus(y);

				return getTouchSide (tsx, tsy);
			}

			/** Combines the results of the two TouchStatus indicators into a Touch2DSide */
			inline 
				Touch2DSide getTouchSide (const TouchStatus tsx, const TouchStatus tsy) const
			{
				return Touch2DSide ((tsy << 2) | tsx);
			}

			/** Determines the 3-dimensional touch side status at the specified voxel coordinates */
			inline 
				Touch3DSide getTouchSide (const GridPointCoords & gpc) const
			{
				return getTouchSide(gpc.i, gpc.j, gpc.k);
			}

			/** Determines the 3-dimensional touch side status at the specified cell coordinates */ 
			inline 
				Touch3DSide getCellTouchSide (const GridCellCoords & gcc) const
			{
				return getCellTouchSide(gcc.i, gcc.j, gcc.k, gcc.lod);
			}

			/** Determines the 3-dimensional touch side status at the specified voxel index */
			inline 
				Touch3DSide getTouchSide (const VoxelIndex idx) const
			{
				GridPointCoords gpc;
				computeGridPoint(gpc, idx);
				return getTouchSide(gpc);
			}

			/** Determines the 3-dimensional touch side status at the specified cell index at level of detail
			@param idx The cell index regardless of LOD (implies highest level of detail: zero)
			@param nLOD The level of detail that defines the size of the cell to test for touch status
			*/
			inline 
				Touch3DSide getCellTouchSide (const CellIndex idx, const unsigned nLOD) const
			{
				GridCellCoords gcc(nLOD);
				computeGridCell(gcc, idx);
				return getCellTouchSide(gcc);
			}

			/** Computes whether a 3D triplet of coordinates are flush with a minimal or maximal edge or corner as bounded by the dimensions
			or neither.  This method calls computeTouchStatus for each of the three coordinates and combines the results into one and one-half nibble,
			the highest two bits represent the z-component, next lower two the y-component and the lowest two the x-component.
			*/
			inline 
				Touch3DSide getTouchSide (const DimensionType x, const DimensionType y, const DimensionType z) const
			{
				const TouchStatus 
					tsx = getTouchStatus(x),
					tsy = getTouchStatus(y),
					tsz = getTouchStatus(z);

				return getTouchSide (tsx, tsy, tsz);
			}

			/** Determines the sides in 3D space that the cell at the specified coordinates touch 
			@remarks The cell coordinates are regardless of LOD (implies highest level of detail: zero)
			@param x The x-coordinate of the cell
			@param y The y-coordinate of the cell
			@param z The z-coordinate of the cell
			@param nLOD The level-of detail that defines the size of the cell to test for touch status
			*/
			inline
				Touch3DFlags getCellTouchSide (const DimensionType x, const DimensionType y, const DimensionType z, const unsigned nLOD) const
			{
				const DimensionType	span = 1 << nLOD;

				return 
					Touch3DFlags(
						getTouchSide (
							getTouchStatus(x), 
							getTouchStatus(y), 
							getTouchStatus(z)
						)
						|
						getTouchSide (
							getTouchStatus(x + span), 
							getTouchStatus(y + span), 
							getTouchStatus(z + span)
						)
					);
			}

			/// Combines the 3 3-dimensional touch status indicators into a Touch3DSide
			inline 
				Touch3DSide getTouchSide (const TouchStatus xts, const TouchStatus yts, const TouchStatus zts) const
			{
				return Touch3DSide ((zts << 4) | (yts << 2) | xts);
			}

			/// Determines touch status and side information from the specified 2-dimensional coordinates
			inline 
				void computeTouchProperties(const DimensionType x, const DimensionType y, TouchStatus & tsx, TouchStatus & tsy, Touch2DSide & side) const
			{
				OgreAssert(x <= dimensions && y <= dimensions, "The transition coordinates were out of bounds");
				tsx = getTouchStatus(x);
				tsy = getTouchStatus(y);
				side = getTouchSide(tsx, tsy);
			}

			/// Returns a pointer to the (const) array of grid vertices.
			inline 
				const IsoFixVec3* getVertices() const {return _vertexPositions; }
			/// Returns the index of the specified grid point.
			inline 
				VoxelIndex getGridPointIndex(const DimensionType x, const DimensionType y, const DimensionType z) const 
			{
				OgreAssert(x <= dimensions && y <= dimensions && z <= dimensions, "Dimensions were out of bounds");
				return z*coordsIndexTx.mz + y*coordsIndexTx.my + x*coordsIndexTx.mx; 
			}

			/// Returns the index of the specified grid cell.
			inline 
				CellIndex getGridCellIndex(const DimensionType x, const DimensionType y, const DimensionType z) const 
			{
				OgreAssert(x < dimensions && y < dimensions && z < dimensions, "Dimensions were out of bounds");
				return z*cellIndexTx.mz + y*cellIndexTx.my + x*cellIndexTx.mx; 
			}

			/// Returns the index of the specified grid point
			inline 
				VoxelIndex getGridPointIndex(const GridPointCoords & c) const { return getGridPointIndex(c.i, c.j, c.k); }
			/// Returns the grid point at the specified index
			inline 
				void computeGridPoint(GridPointCoords & gpc, const VoxelIndex idx) const 
			{ 
				gpc.i = ((unsigned short)idx % coordsIndexTx.my) / coordsIndexTx.mx;
				gpc.j = ((unsigned short)idx % coordsIndexTx.mz) / coordsIndexTx.my;
				gpc.k = (unsigned short)idx / coordsIndexTx.mz;
			}
			/// Returns the grid cell at the specified index
			inline 
				void computeGridCell(GridCellCoords & gcc, const CellIndex idx) const 
			{ 
				gcc.i = ((unsigned short)idx % cellIndexTx.my) / cellIndexTx.mx;
				gcc.j = ((unsigned short)idx % cellIndexTx.mz) / cellIndexTx.my;
				gcc.k = (unsigned short)idx / cellIndexTx.mz;
			}

			/// Returns the voxel coordinates at the specified index
			inline
				GridPointCoords getGridPoint (const VoxelIndex idx) const
			{
				GridPointCoords gpc;
				computeGridPoint(gpc, idx);
				return gpc;
			}
			/// Return the world size of the cube region
			const AxisAlignedBox & getBoxSize() const {return mBoxSize;}

			/// Returns true if the grid stores gradient vectors.Z
			bool hasGradient() const {return (_flags & HAS_GRADIENT) != 0; }
			/// Returns true if the grid stores colour values.
			bool hasColours() const {return (_flags & HAS_COLOURS) != 0; }

			~CubeDataRegionDescriptor();

		private:
			CubeDataRegionDescriptor(const CubeDataRegionDescriptor &);

			/// Vertex positions of the grid points.
			IsoFixVec3* _vertexPositions;

			/// The base-2 logarithmic order of dimension
			unsigned char _nDimOrder2;

			/// Flags describing what data is stored in the data grid (see CubeDataRegion::GridFlags).
			int _flags;

			AxisAlignedBox mBoxSize;

			/// Convenience x^2 (square) function
			inline static size_t SQ(const int n) { return static_cast< size_t > (n*n); }

			/// Initializes the voxel coordindates index transform
			static IndexTx computeCoordsIndexTx(const size_t nTileSize);
			/// Initializes the cell coordindates index transform
			static IndexTx computeCellIndexTx(const size_t nTileSize);
		};
	}
}

#endif
