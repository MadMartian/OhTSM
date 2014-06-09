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

#ifndef _ISO_SURFACE_BUILDER_H_
#define _ISO_SURFACE_BUILDER_H_

#include "OverhangTerrainPrerequisites.h"
#include "IsoVertexElements.h"
#include "IsoSurfaceSharedTypes.h"
#include "HardwareIsoVertexShadow.h"
#include "CubeDataRegion.h"
#include "CubeDataRegionDescriptor.h"
#include "Neighbor.h"
#include "Util.h"
#include "TransvoxelTables.h"
#include "DebugTools.h"

#include <stack>
#include <vector>
#include <set>
#include <ostream>
#include <bitset>

#include <OgreHardwareVertexBuffer.h>
#include <OgreHardwareIndexBuffer.h>
#include <OgreWorkQueue.h>

#ifdef _OHT_ISB_TRACE
	#define OHT_ISB_DBGTRACE(x) OHT_DBGTRACE(x)
#else
	#define OHT_ISB_DBGTRACE(x)
#endif

namespace Ogre
{
	/** Algorithm builder pattern for extracting 2-dimensional iso-surfaces from discretely 
		sampling a 3-dimensional region of voxels using the marching cubes algorithm and 
		a multi-resolution stitching algorithm provided by Transvoxel.
		NOTE: This algorithm makes references to illustrations in Eric Lengyel's 2010 dissertation
	*/
	class IsoSurfaceBuilder
	{
	public:
		/** Channel-specific properties that apply to the kind of surface that the builder is currently working-on */
		class ChannelParameters
		{
		public:
			/// The pre-calculated transition cell vertex transforms for transforming vertices along the full resolution face onto the half
			struct TransitionCellTranslators
			{
				IsoFixVec3 side[CountTouch3DSides];

			} * const _txTCHalf2Full;

			/// Flags determining what kind of vertex properties are supported by the hardware buffer for this channel
			size_t surfaceFlags;
			/// Number of levels of details supported for renderables of this channel
			unsigned short clod;
			/// Maximum error in pixels for renderables of this channel
			Real maxPixelError;
			/// Flip normals of these surfaces
			bool flipNormals;
			/// The method used for normal generation on surfaces of this channel
			NormalsType normalsType;

			/**
			@param fTCWidthRatio The ratio width of a normal cell that makes-up the width of a transition cell
			@param nSurfaceFlags Flags determining what kind of vertex properties are supported by the hardware buffer for this channel
			@param nLODCount Number of levels of details supported for renderables of this channel
			@param fMaxPixelError Maximum error in pixels for renderables of this channel
			@param bFlipNormals Flip normals of these surfaces
			@param enNormalType The method used for normal generation on surfaces of this channel
			*/
			ChannelParameters(
				const Real fTCWidthRatio = 0.5f,
				const size_t nSurfaceFlags = 0,
				const unsigned short nLODCount = 5,
				const Real fMaxPixelError = 8,
				const bool bFlipNormals = false,
				const NormalsType enNormalType = NT_None
			);
			~ChannelParameters();

		private:
			static TransitionCellTranslators * createTransitionCellTranslators(const unsigned short nLODCount, const Real fTCWidthRatio);
		};

#if defined(_DEBUG) || defined(_OHT_LOG_TRACE)
		class DebugInfo
		{
		public:
			String name;
			Vector3 center;

			DebugInfo () {}
			DebugInfo (const IsoSurfaceRenderable * pISR);
		};
#endif
		/**
		@param cubemeta The meta-information singleton that describes all cubical voxel regions in a scene
		@param chanopts The channel options index describing surfaces in all channels
		*/
		IsoSurfaceBuilder(
			const Voxel::CubeDataRegionDescriptor & cubemeta, 
			const Channel::Index< ChannelParameters > & chanparams
		);

		virtual ~IsoSurfaceBuilder();

		/** Generates a combination of surface flags (IsoVertexElements::SurfaceFlags) from the specified channel options
		@param chanopts Channel options providing the details necessary to determine what surface flags reflect them
		@returns Combination of IsoVertexElements::SurfaceFlags flags
		*/
		static size_t genSurfaceFlags( const OverhangTerrainOptions::ChannelOptions & chanopts );

		/** Performs a ray query on the specified surface in the specified channel
		@remarks Performs a ray query on the surface represented by pShadow and stores the result in walker
		@param limit Limit of the ray query relative to the beginning of the isosurface
		@param channel Channel that the surface belongs to
		@param pDataGrid The cube voxel region that occupies the space of the surface
		@param ray Ray in voxel cube space relative to the voxel cube's position
		@param pShadow The shadow object of the surface being interrogated
		@param nLOD The LOD to test the surface, affects the size of the cells interrogated
		@param enTouchFlags The sides of the cubical region that have adjacent stitching information
		@returns True and the distance from the walker's ray origin that intersected if there was an intersection, false otherwise
		*/
		std::pair< bool, Real > rayQuery (
			const Real limit,
			const Channel::Ident channel,
			const Voxel::CubeDataRegion * pDataGrid, 
			const Ray & ray, 
			const SharedPtr< HardwareShadow::HardwareIsoVertexShadow > & pShadow, 
			const unsigned nLOD, 
			const Touch3DFlags enTouchFlags
		);

		/** Builds an isosurface
		@remarks Extracts an isosurface from a voxel cube region and updates the specified surface when finished
		@param pCube The voxel cube data to extract the surface from
		@param pISR The renderable that will be updated with the extracted surface in the main thread
		@param nLOD The LOD of the surface to build
		@param enStitches Indicates what sides to generate transition cells for
		*/
		void build (const Voxel::CubeDataRegion * pCube, IsoSurfaceRenderable * pISR, const unsigned nLOD, const Touch3DFlags enStitches);

		/** Builds an isosurface and queues the data for later propagation to GPU
		@remarks Extracts an isosurface from a voxel cube region and queues the data.  It is the caller's responsibility to manually propagate the data from the queue
			to the GPU hardware.  This may only be called by the OverhangTerrainGroup.  
		@param pMF The meta-fragment that contains the voxel cube data to extract the surface from, its life-cycle must persist for the concurrent extraction workflow here.
		@param pShadow The hardware shadow of the corresponding renderable, hosts the queue for data that will be propagated to the GPU in the main thread
		@param lod The LOD of the surface to build
		@param nSurfaceFlags Flags used to tell the surface-builder what is awesome and hip, in other words I _used_ to know what this was for
		@param enStitches Indicates what sides to generate transition cells for
		@param nVertexBufferCapacity The current capacity of the GPU vertex buffer, used to determine if it must be resized post-surface-generation */
		void queueBuild( 
			const MetaFragment::Container * pMF, 
			SharedPtr< HardwareShadow::HardwareIsoVertexShadow > pShadow, 
			const Channel::Ident channel, 
			const unsigned lod, 
			const size_t nSurfaceFlags, 
			const Touch3DFlags enStitches, 
			const size_t nVertexBufferCapacity 
		);

	protected:
		/// The algorithm for extracting isosurfaces
		void buildImpl(
#if defined(_DEBUG) || defined(_OHT_LOG_TRACE)
			const DebugInfo & debugs,
#endif // _DEBUG
			const Channel::Ident channel,
			HardwareShadow::MeshOperation * pMeshOp,
			const Voxel::CubeDataRegion * pDataGrid, 
			SharedPtr< HardwareShadow::HardwareIsoVertexShadow > & pShadow, 
			const size_t nSurfaceFlags,
			const Touch3DFlags enStitches, 
			const size_t nVertexBufferCapacity
		);

	private:
		OGRE_MUTEX(mMutex);

		/// Populate the queue with information stored in this object
		void fillShadowQueues( HardwareShadow::HardwareIsoVertexShadow::ProducerQueueAccess & queue, const Real fVertScale );

		/// The index of per-channel parameters
		Channel::Index< ChannelParameters > _chanparams;

		/// Identifies the channel parameters being used for the current surface the builder is processing at the moment
		ChannelParameters * _pCurrentChannelParams;

		/** A logical representation of a cell of arbitrary size in 3D space to discretely sample voxels */
		class GridCell
		{
		private:		
			/// The meta-information singleton describing all cubical voxel regions in the scene
			const Voxel::CubeDataRegionDescriptor & _cubemeta;
			/// The LOD of the grid-cell which determines the size of the cell
			const unsigned short _lod;
			
		public:
			/// Location of the cell in the voxel cube region
			DimensionType 
				x, y, z;

			GridCell (const GridCell & copy);
			GridCell (const Voxel::CubeDataRegionDescriptor & cubemeta, const unsigned short nLOD);
			GridCell (const Voxel::CubeDataRegionDescriptor & cubemeta, const unsigned short nLOD, const DimensionType x, const DimensionType y, const DimensionType z);
			GridCell (const Voxel::CubeDataRegionDescriptor & cubemeta, const unsigned short nLOD, const GridCellCoords & gcc);

			/** Maps to/from regular cell corner index and voxel point coordinates */
			class CornerLocator
			{
			private:
				/// Dependent grid cell
				const GridCell * _gc;

				/// Dependency injection
				inline void setParent (const GridCell * pThis)
				{
					_gc = pThis;
				}

			public:
				CornerLocator ()
					: _gc(NULL) {}
				
				/// Retrieve the 3D voxel point coordinates for the specified corner index of the grid cell
				inline
				GridPointCoords coords (const unsigned char nCornerIdx) const
				{
					OgreAssert(nCornerIdx < 8, "Corner index was out of bounds");
					OgreAssert(_gc->x < _gc->_cubemeta.dimensions && _gc->y <_gc-> _cubemeta.dimensions && _gc->z < _gc->_cubemeta.dimensions, "Grid cell is out of bounds");
					return 
						GridPointCoords(
							_gc->x + (((nCornerIdx >> 0) & 1) << _gc->_lod),
							_gc->y + (((nCornerIdx >> 1) & 1) << _gc->_lod),
							_gc->z + (((nCornerIdx >> 2) & 1) << _gc->_lod)
						);
				}

				/// Retrieves the voxel index in the 3D cube voxel region for the specified voxel point coordinates
				inline
				VoxelIndex index (const GridPointCoords & coords) const
				{
					OgreAssert(coords.i <= _gc->_cubemeta.dimensions && coords.j <=_gc-> _cubemeta.dimensions && coords.k <= _gc->_cubemeta.dimensions, "Grid cell is out of bounds");
					return _gc->_cubemeta.getGridPointIndex(coords);
				}
				
				/// Retrieves the voxel index in the cube for the specified corner index of the grid cell
				inline 
				VoxelIndex operator [] (const unsigned char nCornerIdx) const
				{
					OgreAssert(nCornerIdx < 8, "Corner index was out of bounds");
					return _gc->_cubemeta.getGridPointIndex(coords(nCornerIdx));
				}

				friend GridCell;
			} corners;
			friend CornerLocator;

			/// Returns the cell-index of this
			inline CellIndex index () const { return _cubemeta.getGridCellIndex(x, y, z); }

			/// Compares GridCells by LOD and x,y,z coordinates
			inline bool operator != (const GridCell & other) const 
				{ return _lod != other._lod || x != other.x || y != other.y || z != other.z; }

			/// Presuming this cell is flush to the specified cube side, this returns the respective 2D side coordinates
			inline CubeSideCoords get2DCoords (const OrthogonalNeighbor on) const
			{
				return CubeSideCoords::from3D(on, x, y, z);
			}

			/// Changes the location of this grid cell to the one at the specified index
			inline void operator = (const CellIndex idx)
			{
				// TODO: Profile, this may not be as efficient as placing it at class member-level
				static GridCellCoords gcc(0);	// DEPS: Single-threaded
				
				_cubemeta.computeGridCell(gcc, idx);
				operator = (gcc);
			}
			/// Changes the location of this grid cell
			inline void operator = (const GridCellCoords & gcc)
			{
				x = gcc.i;
				y = gcc.j;
				z = gcc.k;
			}

			friend Ogre::Log::Stream & operator << (Ogre::Log::Stream &, const GridCell &);
		};

		/** Iterates through the 8 corners of a voxel region cell depending on the LOD chosen and the starting index chosen.
		*/
		class RegularCaseCodeCompiler
		{
		public:
			struct Result
			{
				VoxelIndex index;
				NonTrivialRegularCase::CodeType casecode;
				bool trivial;

				Result(const Result & copy);
				Result(const VoxelIndex index);
				Result();

				Result & operator = (const Result & copy);
			};

			/** Defines offsets for iterating grid-cell corners and grid cells through a 3D region within a 3D region */
			const struct Advance
			{
				int mx, my, mz;
			} advanceCorners, advanceCells;

		private:
			/// The current iteration state
			Result _result;
			/// The access to the voxels
			const Voxel::const_DataAccessor & _vxaccess;

			/// Computes the Advance for grid-cell corners based on the specified LOD and meta-information singleton
			static Advance computeAdvanceCorners(const unsigned short nLOD, const Voxel::CubeDataRegionDescriptor & cdrd);
			/// Computes the Advance for grid-cells based on the specified LOD and meta-information singleton
			static Advance computeAdvanceCells(const unsigned short nLOD, const Voxel::CubeDataRegionDescriptor & cdrd);

			/** Computes a single bit of a case code for the specified voxel
			@remarks This takes the voxel at the specified voxel-index + corner index into the voxel region, computes the appropriate
				case-code bit for the specified corner index, and modifies the 'casecode' out-parameter value accordingly.
			@param index The base voxel-index analogous to cell index, points to the voxel at corner 0
			@param corner The corner offset from the specified voxel-index that identifies the cell to work with
			@param casecode (out) ORs the appropriate bit in this variable, so it never clears bits
			@param gc7 Stores the bit-result before applying the bit-mask, relevant only at corner 7 to quickly determine a trivial case code (entirely solid or entirely empty) */
			inline void step(const VoxelIndex index, const unsigned corner, unsigned char & casecode, signed char & gc7) const
			{
				casecode |= (gc7 = (_vxaccess.values[index] >> (8 - 1 - corner))) & (1 << corner);
			}

			/// Computes the casecode at the current cell identified by the current voxel index at corner 0
			void process();

		public:
			RegularCaseCodeCompiler(const unsigned short nLOD, const Voxel::const_DataAccessor & vx, const Voxel::CubeDataRegionDescriptor & cdrd);
			RegularCaseCodeCompiler(const VoxelIndex index, const unsigned short nLOD, const Voxel::const_DataAccessor & vx, const Voxel::CubeDataRegionDescriptor & cdrd);
			RegularCaseCodeCompiler(const RegularCaseCodeCompiler & copy);

			inline
				const Result & operator * () const { return _result; }

			inline
				const Result * operator -> () const { return &_result; }

			/// Calculates the case-code at the current index and advances the pointer
			inline
				RegularCaseCodeCompiler & operator ++ ()
			{
				process();
				return *this;
			}

			/// Calculates the case-code at the current index and advances the pointer
			inline
				RegularCaseCodeCompiler operator ++ (int)
			{
				RegularCaseCodeCompiler old = *this;
				process();
				return old;
			}

			/// Advances the pointer by the specified amount, does not perform any case-code calculation
			inline
				RegularCaseCodeCompiler & operator += (const int delta)
			{
				_result.index += delta;
				return *this;
			}

			/// Sets the pointer to the specified index, does not perform any case-code calculation
			inline
				RegularCaseCodeCompiler & operator = (const VoxelIndex & index)
			{
				_result.index = index;
				return *this;
			}
		};

		/** Iterates through all cells and voxel points of each cell throughout a 3D voxel grid by iterating through
			all corners of each grid-cell and then iterating to the next grid-cell until all grid-cells have been visited
			in the voxel-grid */
		class iterator_GridCells
		{
		public:
			/** Each iteration yields current voxel-grid index, grid-cell and corner */
			struct Result : public RegularCaseCodeCompiler::Result
			{
				GridCell gc;

				Result(const GridCell & gc);
				Result(const Result & copy);

				inline
				Result & operator = (const RegularCaseCodeCompiler::Result & base)
				{
					static_cast< RegularCaseCodeCompiler::Result * > (this) -> operator = (base);
					return *this;
				}
			};

		private:
			/// The meta-information singleton describing all cube voxel regions in the scene
			const Voxel::CubeDataRegionDescriptor & _cdrd;
			/// Dimensions of the grid-cell, determined by 2^LOD
			VoxelIndex _span;

			/// The case-code compiler that will be used
			RegularCaseCodeCompiler _ccc;

			/// Coroutine context-switch points
			OHT_CR_CONTEXT;
			enum CRS
			{
				CRS_Default = 1
			};

			/// The current iteration state
			Result _result;

			/** Iterate once 
			@remarks This iterates once through the corners (defined by a grid-cell) of a voxel-grid.  First all eight 
				corners of the first grid-cell are iterated through [0-8) utilizing the iterator_Corners class, then the 
				iteration proceeds to the next grid-cell in the voxel-grid and iterations proceed through the eight corners 
				of that grid-cell and so on until all grid-cells in the voxel-grid have been visited. */
			void process();

		public:
			iterator_GridCells (const unsigned short nLOD, const Voxel::const_DataAccessor & vx, const Voxel::CubeDataRegionDescriptor & cdrd);
			iterator_GridCells (const iterator_GridCells & copy);

			inline
				const Result & operator * () const { return _result; }

			inline
				const Result * operator -> () const { return &_result; }

			/// @returns True if there are more iterations left
			inline operator bool () { return !OHT_CR_TERM; }

			inline
				iterator_GridCells & operator ++ ()
			{
				process();
				return *this;
			}

			inline
				iterator_GridCells operator ++ (int)
			{
				iterator_GridCells old = *this;
				process();
				return old;
			}
		};

		/** Acquire an iterator object to the corners of the voxel-grid for each grid-cell reflecting this one 
		@remarks This does not take into account the current grid-cell position, only the LOD is taken into account. */
		iterator_GridCells iterate_GridCells (const Voxel::const_DataAccessor & vxaccess) const { return iterator_GridCells(_nLOD, vxaccess, _cubemeta); }

		/// Bitset for each side of a cube, used to determine triangle winding order
		const static size_t _triwindflags;

		/** A logical representation of a transition cell of arbitrary size in 3D space flush to a side to discretely
			sample voxels of a high-resolution and a low-resolution surface for stitching */
		class TransitionCell
		{
		private:
			/// The meta-information singleton describing all cubical voxel regions in the scene
			const Voxel::CubeDataRegionDescriptor & _cubemeta;

			/// Common coordinate for the last cell along an axis (common to all coordinate components)
			const DimensionType _lastcell;

			const static struct XY 
			{
				unsigned char x, y;
			} 
				_matci2tcc[13], // Maps corner indices to transition cell coordinates, see figure 4.16
				_matcfi2tcc[9]; // Maps corner flag case index contributers to transition cell coords, see figure 4.17

			/** Retrieves the 3D voxel coordinates for the specified 2D-coordinates taking into account the transition cell's side */
			inline 
			GridPointCoords getGridPointCoords(const DimensionType x, const DimensionType y) const
			{
				typedef DimensionType DT;
				return GridPointCoords
					(	// Since _mattcc23dc represents a diagonal matrix, we use the bitwise OR operation here for a performance boost
						(DT(Mat2D3D[side].x.x) * x) | (DT(Mat2D3D[side].x.y) * y) | (DT(Mat2D3D[side].x.d) & DT(_cubemeta.dimensions)),
						(DT(Mat2D3D[side].y.x) * x) | (DT(Mat2D3D[side].y.y) * y) | (DT(Mat2D3D[side].y.d) & DT(_cubemeta.dimensions)),
						(DT(Mat2D3D[side].z.x) * x) | (DT(Mat2D3D[side].z.y) * y) | (DT(Mat2D3D[side].z.d) & DT(_cubemeta.dimensions))
					);
			}
			
			/** Retrieves the 3D voxel coordinates for the specified 2D-coordinates along the full-resolution face taking into account the transition cell's side */
			inline 
			GridPointCoords getGridPointCoords(const CubeSideCoords & csc) const
				{ return getGridPointCoords(csc.x, csc.y); }
			/** Retrieves the 3D voxel coordinates for this transition cell */
			inline 
			GridPointCoords getGridPointCoords() const
				{ return getGridPointCoords(x, y); }

			/** Retrieves the 3D voxel-grid cell coordinates for the specified 2D-coordinates and LOD taking into account the transition cell's side */
			inline 
			GridCellCoords getGridCellCoords(const DimensionType x, const DimensionType y, const unsigned nLOD) const
			{
				typedef DimensionType DT;
				return GridCellCoords
					(	// Since _mattcc23dc represents a diagonal matrix, we use the bitwise OR operation here for a performance boost
						(DT(Mat2D3D[side].x.x) * x) | (DT(Mat2D3D[side].x.y) * y) | (DT(Mat2D3D[side].x.d) & DT(_lastcell)),
						(DT(Mat2D3D[side].y.x) * x) | (DT(Mat2D3D[side].y.y) * y) | (DT(Mat2D3D[side].y.d) & DT(_lastcell)),
						(DT(Mat2D3D[side].z.x) * x) | (DT(Mat2D3D[side].z.y) * y) | (DT(Mat2D3D[side].z.d) & DT(_lastcell)),
						nLOD
					);
			}
			/** Retrieves the 3D voxel-grid cell coordinates for the specified 2D-coordinates and LOD taking into account the transition cell's side */
			inline 
			GridCellCoords getGridCellCoords(const CubeSideCoords & csc, const unsigned nLOD) const
				{ return getGridCellCoords(csc.x, csc.y, nLOD); }

			/** Retrieves the 3D voxel-grid cell coordinates of this transition cell */
			inline 
			GridCellCoords getGridCellCoords() const
			{
				return getGridCellCoords (x, y, halfLOD);
			}

		public:
			/// The full-resolution and half-resolution LOD
			const unsigned short fullLOD, halfLOD;
			/// The side of the cube voxel region this transition cell resides
			OrthogonalNeighbor side;
			/// The 2D side-coordinates of this cell
			DimensionType
				x, y;

			TransitionCell (const TransitionCell & copy);
			/**
			@param cubemeta The meta-information singleton describing all voxel cube regions in the scene
			@param nLOD The LOD number which affects the size of this transition cell
			@param on The side of the voxel cube region that this transition cell applies to and for which half-to-full-resolution stitching is required
			*/
			TransitionCell (const Voxel::CubeDataRegionDescriptor & cubemeta, const unsigned short nLOD, OrthogonalNeighbor on);
			/**
			@param cubemeta The meta-information singleton describing all voxel cube regions in the scene
			@param nLOD The LOD number which affects the size of this transition cell
			@param x Initial x-location of the transition cell in the voxel grid
			@param y Initial y-location of the transition cell in the voxel grid
			@param on The side of the voxel cube region that this transition cell applies to and for which half-to-full-resolution stitching is required
			*/
			TransitionCell (const Voxel::CubeDataRegionDescriptor & cubemeta, const unsigned short nLOD, const DimensionType x, const DimensionType y, const OrthogonalNeighbor on);

			/** Maps to/from transition cell corner index and voxel point coordinates */
			class CornerLocator
			{
			private:
				/// Dependent transition cell
				const TransitionCell * _tc;
				
				/// Dependency injection
				inline void setParent (const TransitionCell * pThis)
				{
					_tc = pThis;
				}

				CornerLocator (const CornerLocator & copy);

			public:
				CornerLocator ()
					: _tc(NULL) {}

				/// Retrieve the 2D voxel point side coordinates for the specified corner index of the transition cell
				inline
				CubeSideCoords coords (const unsigned char nCornerIdx) const
				{
					OgreAssert(nCornerIdx < 13, "Corner index was out of bounds");
					return CubeSideCoords(
						_tc->x + (_tc->_matci2tcc[nCornerIdx].x << _tc->fullLOD),
						_tc->y + (_tc->_matci2tcc[nCornerIdx].y << _tc->fullLOD)
					);
				}

				/// Retrieves the voxel index in the cube for the specified corner index of the transition cell
				inline
				VoxelIndex index (const CubeSideCoords & coords) const
				{
					OgreAssert(coords.x <= _tc->_cubemeta.dimensions && coords.y <= _tc->_cubemeta.dimensions && _tc->side >= 0 && _tc->side < CountOrthogonalNeighbors, "Transition cell is out of bounds");
					return _tc->_cubemeta.getGridPointIndex(
						_tc->getGridPointCoords(coords)
					);
				}

				/// Retrieves the voxel index (into the voxel grid) of the specified transition cell corner index
				inline VoxelIndex operator [] (const unsigned char nCornerIdx) const
				{
					OgreAssert(nCornerIdx < 13, "Corner index was out of bounds");
					OgreAssert(_tc->x < _tc->_cubemeta.dimensions && _tc->y < _tc->_cubemeta.dimensions && _tc->side >= 0 && _tc->side < CountOrthogonalNeighbors, "Transition cell is out of bounds");
								
					return 
						_tc->_cubemeta.getGridPointIndex(
							_tc->getGridPointCoords (coords(nCornerIdx))
						);
				}

				friend TransitionCell;
			} corners;
			friend CornerLocator;

			/** Maps full-resolution-face transition cell corner indices to voxel indices into the current voxel-grid for
				the current transition cell taking into account the cell's position. */
			class CaseIndexFlagger
			{
			private:
				/// The dependent transition cell
				const TransitionCell * _tc;

				/// Dependency injection
				inline void setParent (const TransitionCell * pThis)
				{
					_tc = pThis;
				}

				CaseIndexFlagger (const CornerLocator & copy);

			public:
				CaseIndexFlagger ()
					: _tc(NULL) {}

				/// Retrieve the data-grid voxel index at the current transition cell location for the specified corner index, see figure 4.16a
				inline VoxelIndex operator [] (const unsigned char nCornerIdx) const
				{
					OgreAssert(nCornerIdx < 9, "Corner index was out of bounds");
					OgreAssert(_tc->x < _tc->_cubemeta.dimensions && _tc->y < _tc->_cubemeta.dimensions && _tc->side >= 0 && _tc->side < CountOrthogonalNeighbors, "Transition cell is out of bounds");
								
					return 
						_tc->_cubemeta.getGridPointIndex(
							_tc->getGridPointCoords (
								_tc->x + (_tc->_matcfi2tcc[nCornerIdx].x << _tc->fullLOD),
								_tc->y + (_tc->_matcfi2tcc[nCornerIdx].y << _tc->fullLOD)
							)
						);
				}

				friend TransitionCell;
			} flags;
			friend CaseIndexFlagger;

			/// Transforms this transition cell to a regular grid cell
			inline operator GridCell () const
				{ return GridCell(_cubemeta, halfLOD, getGridCellCoords()); }

			/// Returns the grid-point coordinates of the transition cell
			inline operator GridPointCoords () const
				{ return getGridPointCoords(); }

			/// Returns the grid-point coordinates of the transition cell
			inline operator GridCellCoords () const
				{ return getGridCellCoords(); }

			/// Returns the grid-point coordinates of the specified 2D coordinates translated by the transition cell's coordinates
			inline GridPointCoords coords(const DimensionType x, const DimensionType y) const { return getGridPointCoords(this->x + x, this->y + y); }

			/// Returns the cell-index of this, which is the same as the grid-index of the minimal corner of the cell
			inline CellIndex index() const
				{ return (y*_cubemeta.dimensions)+x; }

			/** Update the location of this transition cell to the specified 3-dimension grid-cell coordinates.
			@remarks Only the two components of the specified 3D coordinates that are relevant to this transition cell's
				configured side are taken into account.
			*/
			inline void operator = (const GridCellCoords & gcc)
			{
				typedef DimensionType DT;
				// Since _mattcc23dc represents a diagonal matrix, we use the bitwise OR operation here for a performance boost
				x = (DT(Mat2D3D[side].x.x) * gcc.i) | (DT(Mat2D3D[side].y.x) * gcc.j) | (DT(Mat2D3D[side].z.x) * gcc.k);
				y = (DT(Mat2D3D[side].x.y) * gcc.i) | (DT(Mat2D3D[side].y.y) * gcc.j) | (DT(Mat2D3D[side].z.y) * gcc.k);
			}

			/// Updates this transition cell's position with the specified cell index
			inline void operator = (const CellIndex idx)
			{
				x = (unsigned short)idx % _cubemeta.dimensions;
				y = (unsigned short)idx / _cubemeta.dimensions;
			}

			struct CaseCodeResult
			{
				/// Case code computed
				NonTrivialTransitionCase::CodeType casecode;
				/// Whether the computed casecode is trivial or not (entirely solid or entirely empty, no vertices)
				bool trivial;
			} casecode(const Voxel::const_DataAccessor & vx) const;	/// Acquire the casecode at this transition cell from the specified voxel grid

			friend Ogre::Log::Stream & operator << (Ogre::Log::Stream &, const TransitionCell &);
		};

		/** Provides the correct-order for triangle vertices of a given transition cell case.
		@remarks Once a transition cell case has been queried using the '<<' operator overload,
			the '[]' operator overload is used to determine the vertex at an index [0,3).
			This class is used by TransitionTriangleBuilder.
		*/
		class TriangleWinder
		{
		private:
			/// The side relevant to the transition case
			OrthogonalNeighbor _on;
			/// Flags, function of the side
			signed char _onwn;
			/// Flags, function of the side and transition cell case
			signed int _rwoo;

		public:
			/** 
			@param on The side corresponding to the transition cell cases queried
			*/
			TriangleWinder(const OrthogonalNeighbor on);

			/// Queries a particular transition cell case
			inline
			void operator << (const NonTrivialTransitionCase & caze)
			{
				const signed char cTranCellClass = transitionCellClass[caze.casecode];
				_rwoo = ((((cTranCellClass | (cTranCellClass >> 1)) >> 6)) & 3) ^ _onwn;	// Reverses winding-order if transition cell-class id has high-bit set
			}

			/** After a query for a particular transition cell case 
			@param nIndex Triangle vertex number, allowed range [0,3)
			*/
			inline
			size_t operator [] (const size_t nIndex) const
			{
				return (nIndex ^ _rwoo) - (_rwoo & 1);
			}
		};

		/// Retrieves the Transvoxel regular vertex data for the given regular case
		static inline 
		const unsigned short * getVertexData(const NonTrivialRegularCase & caze)
		{
			return regularVertexData[caze.casecode];
		}
		/// Retrieves the Transvoxel transition vertex data for the given transition case
		static inline 
		const unsigned short * getVertexData(const NonTrivialTransitionCase & caze)
		{
			return transitionVertexData[caze.casecode];
		}
		/// Retrieves the number of vertices for the given regular case
		static inline
		const unsigned int getVertexCount(const NonTrivialRegularCase & caze)
		{
			const RegularCellData & oCase = regularCellData[regularCellClass[caze.casecode]];
			const long nVertCount = oCase.GetVertexCount();

			return nVertCount;
		}
		/// Retrieves the number of vertices for the given transition case
		static inline
		const unsigned int getVertexCount(const NonTrivialTransitionCase & caze)
		{
			const signed char cTranCellClass = transitionCellClass[caze.casecode];
			const TransitionCellData & oCase = transitionCellData[cTranCellClass & 0x7F];	// High-bit must be cleared
			const long nVertCount = oCase.GetVertexCount();

			return nVertCount;
		}

		/// Represents the state of the hardware buffer of the currently associated surface
		SharedPtr< HardwareShadow::HardwareIsoVertexShadow > _pShadow;

		/// Represents the LOD-specific state of the shadow and access to the vertices shared by all resolutions
		HardwareShadow::MeshOperation * _pMeshOp;

		size_t 
			/// Keeps track of the current position in the hardware vertex buffer
			_nVertexBufPos, 

			/// Keeps track of the free space left in the index hardware buffer
			_nIndexBufFree;

		/// Tracks the properties of each transition iso-vertex occuring along a half-resolution boundary flush with a cube side
		BorderIsoVertexPropertiesVector _vBorderIVP;

		/// Tracks the properties of each half-resolution transition iso-vertex
		BorderIsoVertexPropertiesVector _vCenterIVP;

		/// Flags indicating what types of elements the current iso-surface supports
		size_t _nSurfaceFlags;

		/// Which cubes adjacent to this cube have higher resolution
		Touch3DFlags _enStitches;

		/// The LOD of the data represented in here
		size_t _nLOD;

		/// Whether the hardware vertex state must be reset before applying new vertices
		bool _bResetVertexBuffer;

		/// Whether the hardware index state must be reset before applying new vertices
		bool _bResetIndexBuffer;

		/// Reference-counted shared pointer to the data grid associated with this isosurface.
		const Voxel::CubeDataRegionDescriptor & _cubemeta;

		/// Abstraction for looking-up or generating regular case codes depending on concrete implementation
		class IRegularCaseLookup
		{
		public:
			virtual NonTrivialRegularCase::CodeType operator [] (const CellIndex index) = 0;
		};

		/// Abstraction for looking-up or generating transition case codes depending on concrete implementation
		class ITransitionCaseLookup
		{
		public:
			virtual NonTrivialTransitionCase::CodeType operator () (const OrthogonalNeighbor neighbor, const CellIndex index) = 0;
		};

		/// Look-up table concrete implementation of a regular case code lookup
		class RegularCaseCache : public IRegularCaseLookup
		{
		private:
			/// A 3D-LUT for the regular cases
			NonTrivialRegularCase::CodeType * _vRegularCases;

		public:
			RegularCaseCache(const HardwareShadow::LOD * pResState, const Voxel::CubeDataRegionDescriptor & cubemeta);
			~RegularCaseCache();

			NonTrivialRegularCase::CodeType operator [] (const CellIndex index);
		};

		/// Look-up table concrete implementation of a transition case code lookup
		class TransitionCaseCache : public ITransitionCaseLookup
		{
		private:
			/// A set of 2D-LUTs for the transition cases
			NonTrivialTransitionCase::CodeType * _vvTrCase[CountOrthogonalNeighbors];

		public:
			TransitionCaseCache (const HardwareShadow::LOD * pResState, const Voxel::CubeDataRegionDescriptor & cubemeta);
			~TransitionCaseCache();

			NonTrivialTransitionCase::CodeType operator () (const OrthogonalNeighbor neighbor, const CellIndex index);
		};

		/// Look-up table concrete implementation of a regular case code extrapolator
		class RegularCaseBuilder : public IRegularCaseLookup
		{
		private:
			/// Access to the 3D voxel data grid
			const Voxel::const_DataAccessor * const _pAccess;
			/// Current coordinates in the 3D voxel data grid
			GridCell _gc;
			/// Business logic for extrapolating the case-code from a cell
			RegularCaseCodeCompiler _ccc;
			
			/// Descriptor of the 3D voxel cube region
			const Voxel::CubeDataRegionDescriptor & _cdrd;

		public:
			/**
			@param lod Level-of-detail determining the size of the cells to extrapolate case codes from
			@param pAccess Access to the 3D voxel data grid
			@param cubemeta Descriptor of the 3D voxel cube region */
			RegularCaseBuilder (const unsigned lod, const Voxel::const_DataAccessor * pAccess, const Voxel::CubeDataRegionDescriptor & cubemeta);

			NonTrivialRegularCase::CodeType operator [] (const CellIndex index);
		};

		class TransitionCaseBuilder : public ITransitionCaseLookup
		{
		private:
			TransitionCell _tc;
			const Voxel::const_DataAccessor & _vxaccess;

		public:
			TransitionCaseBuilder(const unsigned nLOD, const Voxel::const_DataAccessor & vxaccess, const Voxel::CubeDataRegionDescriptor & cubemeta)
				: _vxaccess(vxaccess), _tc(cubemeta, nLOD, OrthoNaN) {}

			NonTrivialTransitionCase::CodeType operator () (const OrthogonalNeighbor neighbor, const CellIndex index);
		};

		/// Containers for full-resolution inside, outside, and half-resolution vertices
		BorderIsoVertexPropertiesVector _vTransInfos3[3];

		/** Given a pair of coarsely / sparsely spaced voxel coordinates, this algorithm finds the most refined pair.
			A refined pair of voxel coordinates by definition are adjacent to one another with no other voxels between
			them.  They are separated by the smallest incremental distance permitted by the paradigm.
		*/
		template< typename CornerLocator, typename Coords >
		class IsoVertexIndexRefiner
		{
		private:
			/** In order, these are:
			 * - The coordinates at corner0 and corner1
			 * - The coordinates at the mid-point between corner0 and corner1
			 * - Coordinate flyweight
			 * - Coordinate result corresponding to the iso-vertex index
			 */
			Coords _c0, _c1, _cM, _c, _co;

			/** In order, these are:
			 * - Values at corner0 and corner1
			 * - Value at mid-point between corner0 and corner1
			 * - Masks that choose the primary corner over the center one for both outside corners
			 */
			FieldStrength _v0, _v1, _vm, _m0, _m1, _m0v1;

			/// The two refined corner indexes, these are output results
			VoxelIndex _c0o, _c1o;

			/// Mask identifying whether the value at one of the corners was zero
			unsigned char _mz;

			const static FieldStrength _bsFieldStrength_m1 = (sizeof(FieldStrength) << 3) - 1;

			/**
			Incrementally refines the voxel coordinate pair one-step by choosing a new voxel coordinate
			by testing the two voxel values with one sampled at the mid-point and the voxel matching the
			state of the mid-point becomes updated to it.
			@param corloc Corner locator, which is used to convert coordinates to voxel indices
			@param pDataGridValues Used in conjunction with 'corloc' this is the field used to discretely sample voxels
			*/
			inline
			void refine(const CornerLocator & corloc, const FieldStrength * pDataGridValues)
			{
				_cM = _c1 - _c0;
				_cM >>= 1;
				_cM += _c0;
				_vm = pDataGridValues[corloc.index(_cM)];
				_m0 = (_v0 ^ _vm) >> (sizeof(FieldStrength) << 3);
				_m1 = (_v1 ^ _vm) >> (sizeof(FieldStrength) << 3);

				_c = (_c0 & _m0);
				_c |= (_cM & ~_m0);
				_c0 = _c;

				_c = (_c1 & _m1);
				_c |= (_cM & ~_m1);
				_c1 = _c;

				_v0 = pDataGridValues[corloc.index(_c0)];
				_v1 = pDataGridValues[corloc.index(_c1)];
			}

			/** Starts refinement given the two corner indices.
			@param corloc Knows how to translate the two corner indices into voxel grid indices
			@param data Object for gaining access to the voxel sampling field
			@param c0 First corner index of the pair to be refined
			@param c1 Second corner index of the pair to be refined
			*/
			inline
			void initialize (
				const CornerLocator & corloc, 
				Voxel::const_DataAccessor & data,
				const unsigned char c0, const unsigned char c1
			)
			{
				_c0 = corloc.coords(c0);
				_c1 = corloc.coords(c1);
				_v0 = data.values[corloc.index(_c0)];
				_v1 = data.values[corloc.index(_c1)];
			}

			/// Uses the corner locator to fetch voxel indices from the current corner index pair
			inline
			void cornerAssignments(const CornerLocator & corloc)
			{
				_c0o = corloc.index(_c0);
				_c1o = corloc.index(_c1);
			}

			/// Performs a test for zero value at the mid-point, which complicates the algorithm 
			inline
			void zeroValueStep() 
			{
				//((m - 1) & ~m & 0x8000)
				_m0v1 = ~(((((_v1 - 1) & ~_v1 & (1 << _bsFieldStrength_m1))) >> _bsFieldStrength_m1) - 1);
				_mz = unsigned char (_m0v1 | ~((((_v0 - 1) & ~_v0 & (1 << _bsFieldStrength_m1)) >> _bsFieldStrength_m1) - 1));
			}

			/// Updates the resultant corner coordinates depending on the test resulting in the last step of the refinement influenced by zero-valued voxels
			inline
			void coordAssignment()
			{
				_co = (_c0 & ~_m0v1) + (_c1 & _m0v1);
			}

			/// Perform final steps in refinement process
			inline
			void finish (const CornerLocator & corloc)
			{
				zeroValueStep();
				coordAssignment();
				cornerAssignments(corloc);
			}

		public:
			/** Entry-point for the refinement algorithm provided by this class.
			@param nLOD Specifies how far apart the two corner indices are initially and also indicates how much refinement is necessary
			@param corloc Corner locator object provided by either a transition cell or a regular grid cell
			@param data Provides access to the voxel grid for discrete sampling of voxels
			@param c0 First corner index of the pair to be refined
			@param c1 Second corner index of the pair to be refined
			*/
			void compute( 
				const unsigned nLOD, 
				const CornerLocator & corloc, 
				Voxel::const_DataAccessor & data,
				const unsigned char c0, const unsigned char c1 
			)
			{
				initialize(corloc, data, c0, c1);

				OgreAssert(_c0 < _c1, "Expected first corner coords to be less than second corner coords");
				for (register unsigned iter = nLOD; iter > 0; --iter)
					refine(corloc, data.values);

				finish(corloc);
			}

			/// Performs one more refinement step
			void oneMoreTime( const CornerLocator & corloc, Voxel::const_DataAccessor & data )
			{
				refine(corloc, data.values);
				finish(corloc);
			}

			/// Retrieve the voxel grid index of the first voxel corner index
			inline
			VoxelIndex getGridIndex0 () const { return _c0o; }

			/// Retrieve the voxel grid index of the second voxel corner index
			inline
			VoxelIndex getGridIndex1 () const { return _c1o; }

			/// Retrieves the coordinates of the most significant voxel of the pair, which is significant in the case of encountering voxels of value zero
			inline
			const Coords & getCoords () const { return _co; }

			/// Retrieves the coordinates of the least significant voxel of the pair, may be the same as the most significant one in the case of a zero-value voxel
			inline
			const Coords & getCoords0 () const { return _c0; }

			/// Retrieves a flag indicating whether the refinement encountered zero-valued voxels
			inline
			unsigned char getZeroValueFlag () const { return _mz; }
		};

		/// Refines voxel coordinate pairs in regular cells
		IsoVertexIndexRefiner< TransitionCell::CornerLocator, CubeSideCoords > 
			_trrefiner;

		/// Refines voxel coordinate pairs in transition cells
		IsoVertexIndexRefiner< GridCell::CornerLocator, GridPointCoords >
			_rgrefiner;

		/** Computes the accurate iso-vertex index between two corner indices by continually sub-dividing the corner indices along a path defined
			by the value of the mid-point between two corner indices until the distance between the two indices is highest resolution.  The two
			corner indices and iso-vertex index is then based on this.
		@param gc The current grid cell to refine from
		@param data The voxel-grid for discretely sampling voxel values aiding refinement
		@param vrecacc The Transvoxel vertex code of the pair, see figure 3.8b
		@param ei Receives the new edge-index of the vertex, see figure 3.8a
		@param ci0 Receives the first voxel index of the refined pair 
		@param ci1 Receives the second voxel index of the refined pair 
		@param nLOD The LOD to perform refinement at (in-case it must differ from 'gc')
		@param ivi Receives the resultant isovertex index between the pair of voxels
		@param gpc Receives the resultant most significant refined voxel grid coordinates
		*/
		void computeRefinedRegularIsoVertex (
			const GridCell & gc, 
			Voxel::const_DataAccessor & data, 
			const VRECaCC & vrecacc, 
			unsigned char & ei, 
			VoxelIndex & ci0, VoxelIndex & ci1, 
			const unsigned nLOD, 
			IsoVertexIndex & ivi, 
			GridPointCoords & gpc
		);

		/** Computes the accurate iso-vertex index between two transition corner indices by continually sub-dividing the corner indices along the path
			defined by the value of the mid-point between two corner indices until the distance between them is highest resolution.  The two
			corner indices and iso-vertex index is then based on this.
			@param tc The current transition cell to refine from
			@param data The voxel-grid for discretely sampling voxel values aiding refinement
			@param vrecacc The Transvoxel transition vertex code of the pair, see figure 4.19
			@param ei Receives the new edge-index of the vertex, see figure 4.18a,b
			@param ci0 Receives the first voxel index of the refined pair 
			@param ci1 Receives the second voxel index of the refined pair 
			@param tsx Receives voxel-grid side horizontal border touch status
			@param tsy Receives voxel-grid side vertical border touch status
			@param rside The side (if any) that the resultant refined isovertex is flush with
			@param ivi Receives the resultant isovertex index between the pair of voxels
			@param csc Receives the resultant most significant refined coordinates along the relevant transition cell side
		*/
		void computeRefinedTransitionIsoVertex 
		( 
			const TransitionCell & tc, 
			Voxel::const_DataAccessor & data, 
			const TransitionVRECaCC & vrecacc, 
			unsigned char & ei, 
			VoxelIndex & ci0, VoxelIndex & ci1, 
			TouchStatus & tsx, TouchStatus & tsy, 
			Touch2DSide & rside, 
			IsoVertexIndex & ivi, 
			CubeSideCoords & csc 
		);

		/** Container for isovertices, their elements, and isovertex index mappings */
		class MainVertexElements : public IsoVertexElements
		{
		private:
			/// Configures the offsets for various types of transition isovertex indices
			struct TransitionVertexGroupOffset
			{
				IsoVertexIndex 
					o,			// Group offset
					mx, my,		// Row/col indexing multipliers
					dx, dy;		// Coordinate shift at start of group
#ifdef _DEBUG
				size_t length;
#endif
			};
			/// Offsets of the various types of isovertex indices
			struct 
			{
				// First 7 elements have sub-array of CountOrthogonalNeighbors.
				// Element #8 has sub-array of CountMoore3DNeighbors
				// Elements #9 and #10 have sub-array of CountOrthogonalNeighbors+CountMoore3DEdges
				TransitionVertexGroupOffset * transition[10];
				IsoVertexIndex regular[4];
			} _offsets;

			// Regular edge-index for iso-vertex index multipliers
			const static struct Shift3D
			{
				signed char dx, dy, dz;
			} _regeiivimx[4];
			/** Described in order:
			 - Transitions shifts is a LUT for given an edge-index provides the 2D offset (in full-resolution cell units) from the bottom-left corner, see figure 4.18a,b
			 - The second one given an edge-index provides the 2D offset relative to its cell regardless of resolution */
			const static struct Shift2D
			{
				signed char dx, dy;
			} _transhifts[10], _teidim1[10];

			/** Convert Touch2DSide and transition side to Moore3DNeighbor
				Ordered according to TouchStatus flags
				- Lower 2-bits: x-axis TouchStatus flags
				- Upper 2-bits: y-axis TouchStatus flags
				
				**Sparse Type Remarks**
				The array size is 11 because we only need to account for the 4 edges, 4 corners, and single face
				amounting to a total of 9 elements.  There are 2 elements that have the impossible
				two-bits set and the remaining 5 elements also have impossible two-bits set and are
				unnecessary.  Thus 11 elements.
			**/
			const static unsigned char
				_2dts2m3n
					[CountOrthogonalNeighbors]
					[Count2DTouchSideElements];

			/** Convert Touch2DSide and transition side to Touch3DSide
				Ordered according to TouchStatus flags
				- Lower 2-bits: x-axis TouchStatus flags
				- Upper 2-bits: y-axis TouchStatus flags

				**Sparse Type Remarks**
				The array size is 11 because we only need to account for the 4 edges, 4 corners, and single face
				amounting to a total of 9 elements.  There are 2 elements that have the impossible
				two-bits set and the remaining 5 elements also have impossible two-bits set and are
				unnecessary.  Thus 11 elements.
			**/
			const static unsigned char
				_2dts23dts
					[CountOrthogonalNeighbors]
					[Count2DTouchSideElements];

			/// Converts transition cell vertex group and side to grid cell vertex group
			const static unsigned char _tcg2gcg[CountOrthogonalNeighbors][10];

			/// The meta-information singleton describing all voxel cube regions in the scene
			const Voxel::CubeDataRegionDescriptor & _cubemeta;

			/// Computes the total number of isovertex indices required by a voxel cube region
			size_t computeTotalElements( const Voxel::CubeDataRegionDescriptor & dgtmpl );

			/** Given a transition cell and vertex code, computes the 2D side coordinates
			@remarks Applies to either full or half resolution isovertices
			@param tc The transition cell to start from
			@param vrec The Transvoxel transition vertex code identifying which vertex to find
			@param x Receives the x-coordinate
			@param y Recieves the y-coordinate
			*/
			inline void computeTransitionVertexCoordinates(const TransitionCell & tc, const TransitionVRECaCC & vrec, DimensionType & x, DimensionType & y) const
			{
				const Shift2D & shift = _transhifts[vrec.getEdgeCode()];
				const register unsigned char cl = vrec.getCellLocator();
				const register unsigned char shrf = ((cl & 0x4) >> 2);
				const DimensionType
					dx = (((shift.dx >> (shrf & (shrf ^ (shift.dx & 0x1)))) << tc.fullLOD) - (((cl >> 0) & 0x1) << tc.halfLOD)),
					dy = (((shift.dy >> (shrf & (shrf ^ (shift.dy & 0x1)))) << tc.fullLOD) - (((cl >> 1) & 0x1) << tc.halfLOD));

				// Vertex location, bounded by 0 <= x <= (2^n)+1
				y = (tc.y + dy);
				x = (tc.x + dx);
			}

			/// LSB flag indicating whether the specified transition vertex group number belongs to the half-resolution face of a transition cell or not
			inline static unsigned getGroupFlag( const register unsigned char ei )
			{
				return ((ei + 1) >> 3) & 0x1;
			}

			/** Clears flags of the specified side that are irrelevant depending on the nature of the specified transition vertex group number.
			 * This method applies to half-resolution vertices only.
			 * 
			 * @param side Inclusive flags indicating that a set of coordinates touch certain sides but does not account for vertex groups occurring along a certain axis
			 * @param ei The transition vertex group number, which must be a half-resolution-face vertex group number
			 * @returns Touch side that is a subset of the parameter specified
			 */
			inline static Touch2DSide refineHalfResTouch2DSide( const Touch2DSide side, const register unsigned char ei )
			{
				//OgreAssert(ei >= 7 && ei <= 9, "Invalid half-resolution iso-vertex group number");
				const register unsigned char		mxy = 12-(ei+2);
				return Touch2DSide(side & (((mxy & 1)*3) | ((mxy & 2)*6)));	// Lower half nibble represents x-axis, upper-half nibble represents y-axis, i = 0 for ei={0,1,2,3,4,5,6}
			}
			/** Clears flags of the specified side that are irrelevant depending on the nature of the specified transition vertex group number.
			 * This method applies to full-resolution vertices only.
			 * 
			 * @param side Inclusive flags indicating that a set of coordinates touch certain sides but does not account for vertex groups occurring along a certain axis
			 * @param ei The transition vertex group number, which must be a full-resolution-face vertex group number
			 * @returns Touch side that is a subset of the parameter specified
			 */
			static inline
			Touch2DSide refineFullResTouch2DSide( const Touch2DSide side, const register unsigned char ei )
			{
				//OgreAssert(ei >= 0 && ei <= 6, "Invalid full-resolution iso-vertex group number");
				const register unsigned char		mxy = unsigned(3-((signed(ei)-1)/2));
				return Touch2DSide(side & (((mxy & 1)*3) | ((mxy & 2)*6)));	// Lower half nibble represents x-axis, upper-half nibble represents y-axis, i = 0 for ei={0,1,2,3,4,5,6}
			}

			/** Clears flags of the specified side that are irrelevant depending on the nature of the specified regular vertex group number.
			 *
			 * @param side Inclusive flags indicating that a set of coordinates touch certain sides but does not account for vertex groups occurring along a certain axis
			 * @param ei The regular vertex group number, 0-3
			 * @returns Touch side that is a subset of the parameter specified
			 */
			static inline
			Touch3DSide refineTouch3DSide( const Touch3DSide side, const register unsigned char ei)
			{
				OgreAssert(ei <= 3, "Invalid edge-index/vertex group number");

				const static unsigned char
					regularVertexGroupCoordinateMasks[4] = {
						literal::B< 111111 >::_,
						literal::B< 110011 >::_,
						literal::B< 111100 >::_,
						literal::B<   1111 >::_,
					};
				
				return Touch3DSide(side & regularVertexGroupCoordinateMasks[ei]);
			}

			/** Determines the actual side an isovertex touches
			@remarks Determines if the isovertex of the specified edge-index would touch the specified side if it were in a cell touching the specified side.
			@param side The side that the imaginary cell an isovertex belongs to is touching
			@param ei The edge-index of the imaginary isovertex we are testing
			@returns The side (if any) that the imaginary isovertex is actually flush with
			*/
			static inline
			Touch2DSide refine2DTouchSide( const Touch2DSide side, const register unsigned char ei)
			{
				const register unsigned 
					gf = getGroupFlag(ei),		// Set if ei={7,8,9}
					mgf = (gf-1);				// All bits set if ei={7,8,9}
				register Touch2DSide 
					rside = Touch2DSide((mgf & refineFullResTouch2DSide(side, ei)) | (~mgf & refineHalfResTouch2DSide(side, ei)));

				return rside;		// Return refined touch-side
			}

		public:
			/// Tracks collection of the transition vertex properties
			BitSet trackFullOutsides;

			/** The following three LUTs are used to optionally map isovertices to other isovertices, which is relevant
			during refinement and multi-resolution stitching.  They are in order described as:
			- Regular-only isovertex index mappings to other isovertices
			- Transition-only isovertex index mappings to other isovertices
			- Isovertex refinements
			*/
			IsoVertexIndex 
				* const remappings,
				* const trmappings,
				* const refinements;
			
			/// LUT for isovertex index to the voxel index pair that wraps the isovertex on either side of its axis
			struct CellIndexPair
			{
				VoxelIndex corner0, corner1;		
			} * const cellindices;

			/** 
			@param cubemeta The meta-information singleton describing all voxel cube regions in the scene
			*/
			MainVertexElements(const Voxel::CubeDataRegionDescriptor & cubemeta);
			~MainVertexElements();

			void rollback();
			virtual void clear();
			
			/** Takes a vertex-reuse edge-code and converts it to an iso-vertex index by adding it to 
				the grid cell coordinates 
			@param ei The edge-index of the isovertex
			@param x The x-coordinate of the cell
			@param y The y-coordinate of the cell
			@param z The z-coordinate of the cell
			@returns The isovertex index at that location
			*/
			inline 
			IsoVertexIndex getRegularVertexIndex( const unsigned char ei, const DimensionType x, const DimensionType y, const DimensionType z) const
			{
				const Shift3D & shift = _regeiivimx[ei];
				return 
					_offsets.regular[ei]
						+ z*(_cubemeta.dimensions+shift.dx)*(_cubemeta.dimensions+shift.dy) 
						+ y*(_cubemeta.dimensions+shift.dx) 
						+ x;
			}
			/** Takes a vertex-reuse edge-code and converts it to an iso-vertex index by adding it to 
				the grid cell coordinates 
			@param ei The edge-index of the isovertex
			@param gpc The cell coordinates
			@returns The isovertex index at that location
			*/
			inline
			IsoVertexIndex getRegularVertexIndex( const unsigned char ei, const GridPointCoords & gpc) const
			{
				return getRegularVertexIndex(ei, gpc.i, gpc.j, gpc.k);
			}

			/** Takes a vertex-reuse edge-code and converts it to an iso-vertex index by adding it to 
				the grid cell's coordinates 
			@param gc The grid-cell to start at
			@param vrecacc The regular isovertex edge code identifying the isovertex in the current grid cell
			@param nLOD The LOD affecting the size of the grid cell (in case the LOD must differ from that of the grid cell)
			@returns The isovertex index at that location
			*/
			inline 
			IsoVertexIndex getRegularVertexIndex(const GridCell & gc, const VRECaCC & vrecacc, const unsigned nLOD) const
			{
				const Shift3D & shift = _regeiivimx[vrecacc.getEdgeCode()];
				const DimensionType 
					dx = (shift.dx << nLOD) - (((vrecacc.getCellLocator() >> 0) & 0x1) << nLOD),
					dy = (shift.dy << nLOD) - (((vrecacc.getCellLocator() >> 1) & 0x1) << nLOD),
					dz = (shift.dz << nLOD) - (((vrecacc.getCellLocator() >> 2) & 0x1) << nLOD);

				return getRegularVertexIndex(
					vrecacc.getEdgeCode(), 
					dx + gc.x, 
					dy + gc.y, 
					dz + gc.z
				);
			}

			/** Given a transition cell and transition vertex edge code, returns the isovertex index at that location
			@remarks This determines the isovertex index by summing offset information through LUTs for the information
				in the transition vertex code and by adding offsets from the location of the specified transition cell.
			@param tc The transition cell to start from
			@param vrec The transition isovertex edge code identifying the isovertex within the transition cell
			@returns The isovertex index identifying the isovertex at the location described above
			*/
			IsoVertexIndex getTransitionIndex( const TransitionCell & tc, const TransitionVRECaCC & vrec ) const;

			/** Given a voxel cube side, 2D side coordinates, and a vertex edge-index, determines the isovertex index at that location 
			@param on The side of the 3D voxel cube to search on
			@param csc The 2D side coordinates on the specified side identifying the transition cell to search in
			@param ei The edge-index of the isovertex in the cell identifying the isovertex
			@returns The isovertex index at the location described above
			*/
			inline
			IsoVertexIndex getTransitionIndex( const OrthogonalNeighbor on, const CubeSideCoords & csc, const unsigned char ei) const;
			/** Given a voxel cube side, 2D side coordinates, and a vertex edge-index, determines the isovertex index at that location 
			@param on The side of the 3D voxel cube to search on
			@param x The 2D x-coordinate on the specified side identifying the transition cell to search in
			@param y The 2D y-coordinate on the specified side identifying the transition cell to search in
			@param ei The edge-index of the isovertex in the cell identifying the isovertex
			@returns The isovertex index at the location described above
			*/
			IsoVertexIndex getTransitionIndex( const OrthogonalNeighbor on, const DimensionType x, const DimensionType y, const unsigned char ei) const;

			/** Determines the 3D side of the voxel cube a supposed isovertex touches
			@remarks Given a transition cell side and supposed border a supposed transition cell touches and imaginary edge-index of the isovertex,
				determines the actual side in 3-dimensions that the isovertex is flush with
			@param on The supposed transition cell side
			@param side The border the supposed transition cell touches
			@param ei The edge-index of an isovertex in the transition cell
			@returns The isovertex index at the location described above
			*/
			inline 
			Touch3DSide getTouch3DSide (const OrthogonalNeighbor on, const Touch2DSide side, const unsigned char ei) const
			{
				const static unsigned char
					transitionVertexGroupCoordinateMasks[10] = {
						literal::B< 1111 >::_,
						literal::B< 1111 >::_,
						literal::B< 1111 >::_,
						literal::B< 1100 >::_,
						literal::B< 1100 >::_,
						literal::B<   11 >::_,
						literal::B<   11 >::_,
						literal::B< 1111 >::_,
						literal::B< 1100 >::_,
						literal::B<   11 >::_
				};

				OgreAssert(ei < 10, "Edge-group out of bounds");

				return Touch3DSide(_2dts23dts[on][side & transitionVertexGroupCoordinateMasks[ei]]);
			}

			/** Determines the 3D side of the voxel cube a supposed isovertex touches
			@remarks Given a set of 3D coordinates in a voxel cube, determines which side that the isovertex is flush with
			@param x The x-coordinate of the nearest grid point in the voxel cube
			@param y The y-coordinate of the nearest grid point in the voxel cube
			@param z The z-coordinate of the nearest grid point in the voxel cube
			@param ei The edge-index of the isovertex in its cell
			@returns The isovertex index at the location described above
			*/
			inline
			Touch3DSide getTouch3DSide (const DimensionType x, const DimensionType y, const DimensionType z, const unsigned char ei) const
			{
				return refineTouch3DSide(_cubemeta.getTouchSide(x, y, z), ei);				
			}

			/** Computes useful properties of a transition vertex index
			@remarks Given various information about a transition cell and vertex, yields some useful information on it
			@param tcside The transition cell side of a transition cell containing an isovertex to find
			@param x The x-coordinate of the nearest grid point
			@param y The y-coordinate of the nearest grid point
			@param ei The edge-index of the isovertex in the transition cell
			@param tsx The 2D side border the x-coordinate of the grid point touches
			@param tsy The 2D side border the y-coordinat eof the grid point touches
			@param side Same as 'tsx' and 'tsy' combined
			@param ivi Receives the isovertex index at the location described above
			@param rside Receives the side of the voxel cube that the isovertex is flush with
			*/
			void computeTransitionIndexProperties( 
				const OrthogonalNeighbor tcside, 
				const DimensionType x, const DimensionType y,
				const register unsigned char ei, 
				const TouchStatus tsx, const TouchStatus tsy, const Touch2DSide side, 
				IsoVertexIndex & ivi, Touch2DSide & rside
			) const;

			/** Computes useful properties of a transition vertex index
			@remarks Given a transition cell and vertex code computes the relevant isovertex index and side touch information
			@param tc The transition cell containing the isovertex
			@param vrec The vertex code identifying the isovertex
			@param ivi Recieves the isovertex index
			@param rside Receives the 3D cube side the isovertex is flush with
			*/
			void computeTransitionIndexProperties( const TransitionCell & tc, const TransitionVRECaCC & vrec, IsoVertexIndex & ivi, Touch2DSide & rside ) const;

		} * _pMainVtxElems;

		/** Iterator pattern for triangles and vertices in correct batching order for a particular triangulation case */
		template< typename Indexer >
		class TriangulationTriangleIterator
		{
		private:
			/// LUT for translating from triangulation case vertex index to top-level isovertex index
			const IsoVertexIndex * _vertices;
			/// Array of triangulation case vertex indices that make-up a case triangulation
			const unsigned char * _indices;
			/// The number of vertices in a triangulation case
			const size_t _count;
			/// Knows how to wind triangle vertices in the correct batching direction
			const Indexer & _indexer;
			/// The current offset into the array of triangulation case vertex indices, analogous to a pointer to the current triangle in the case triangulation
			size_t _offset;

		public:
			/** 
			@param pIsoVertices LUT for translating from triangulation case vertex index to top-level isovertex index
			@param pTriIndices Array of triangulation case vertex indices that make-up a case triangulation
			@param nVertCount The number of vertices in a triangulation case
			@param indexer Knows how to wind triangle vertices in the correct batching direction
			*/
			TriangulationTriangleIterator(
				const IsoVertexIndex * pIsoVertices, 
				const unsigned char * pTriIndices, 
				const size_t nVertCount, 
				const Indexer & indexer
			)
				: _indexer(indexer), _vertices(pIsoVertices), _count(nVertCount), _offset(0), _indices(pTriIndices)
			{
				OgreAssert(_count % 3 == 0, "Triangle vertex count must be a multiple of 3");
			}

			/** Retrieves the n-th vertex in the triangulation case, the first 3 always make-up one of the triangles
			@remarks Use the '++' overloaded operator to advance to the next triangle
			@param nVertex The vertex index into the array of case triangulation vertices
			*/
			inline
			IsoVertexIndex operator [] (const size_t nVertex) const 
			{ 
				OgreAssert(_offset + nVertex < _count, "Index out of bounds");
				return _vertices[_indices[_offset + _indexer[nVertex]]];
			}

			/// Determines if two or more vertices in the current triangle are the same and constitute an empty set
			inline
			bool collapsed () const
			{
				const TriangulationTriangleIterator & self = *this;
				return 
					self[0] == self[1] || self[1] == self[2] || self[2] == self[0];
			}

			/// Advances to the next triangle by increasing the current vertex index into the array of case triangulation vertices by 3
			inline
			void operator ++ ()
			{
				OgreAssert(_offset < _count, "Iteration past end");
				_offset += 3;
			}

			/// @returns False if all vertices have been iterated through
			inline
			operator bool () const
			{
				return _offset < _count;
			}
		};

		/** Base class for marching cubes triangulation case triangle builders */
		class TriangleBuilderProps
		{
		protected:
			/// The array of isovertex indices used in a triangulation case, populated later
			IsoVertexIndex _vertices[12];
			/// The array of vertex indices into the set of Transvoxel vertices used in a triangulation case, populated later
			const unsigned char * _indices;
			/// The number of vertices used by a triangulation case, populated later
			size_t _vcount;
			/// The main vertex elements that provides information to everything
			const MainVertexElements * _vtxelems;

			TriangleBuilderProps(const MainVertexElements * pVtxElems)
				: _vtxelems(pVtxElems) {}
		};

		typedef TriangulationTriangleIterator< TriangleWinder > TransitionTriangleIterator;
		/// Triangle builder pattern for transition triangulation cases
		class TransitionTriangleBuilder : private TriangleBuilderProps
		{
		private:
			/// The current transition cell of the triangulation
			TransitionCell _tc;
			/// Object that knows how to wind triangle vertices in the correct batching order
			TriangleWinder _winder;

		public:
			/** 
			@param cubemeta The meta-information singleton describing every voxel cube region in the scene
			@param vtxelems The main vertex elements that provides information to everything
			@param nLOD The LOD used for all triangulations, affects the size of the cells
			@param on The side of the voxel cube these transition triangulations reside
			*/
			TransitionTriangleBuilder(const Voxel::CubeDataRegionDescriptor & cubemeta, const MainVertexElements * vtxelems, const unsigned nLOD, const OrthogonalNeighbor on)
				: TriangleBuilderProps(vtxelems), _tc(cubemeta, nLOD, on), _winder(on) {}

			/// Applies a transition triangulation case, first step before acquiring an iterator
			void operator << (const NonTrivialTransitionCase & caze);

			/// Acquires a triangle and vertex iterator to the current triangulation case (applied by '<<' operator overload)
			inline 
			TransitionTriangleIterator iterator () const
				{ return TransitionTriangleIterator (_vertices, _indices, _vcount, _winder); }
		};

		/// A triangle vertex winding algorithm that does no special winding, the input is a function of itself
		class NoOpIndexer
		{
		public:
			inline
			size_t operator [] (const size_t nIndex) const
				{ return nIndex; }
		};

		typedef TriangulationTriangleIterator< NoOpIndexer > RegularTriangleIterator;
		/// Triangle builder pattern for regular transition cases
		class RegularTriangleBuilder : private TriangleBuilderProps
		{
		private:
			/// The LOD used for all triangulations, affects the size of the cells
			const unsigned _lod;
			/// The current grid cell of the triangulation
			GridCell _gc;

		public:
			/** 
			@param cubemeta The meta-information singleton describing every voxel cube region in the scene
			@param vtxelems The main vertex elements that provides information to everything
			@param nLOD The LOD used for all triangulations, affects the size of the cells
			*/
			RegularTriangleBuilder(const Voxel::CubeDataRegionDescriptor & cubemeta, const MainVertexElements * pVtxElems, const unsigned nLOD)
				: TriangleBuilderProps(pVtxElems), _lod(nLOD), _gc(cubemeta, nLOD) {}

			/// Applies a regular triangulation case, first step before acquiring an iterator
			void operator << (const NonTrivialRegularCase & caze);

			/// Acquires a triangle and vertex iterator to the current triangulation case (applied by '<<' operator overload)
			inline
			RegularTriangleIterator iterator () const
				{ return RegularTriangleIterator (_vertices, _indices, _vcount, NoOpIndexer()); }
		};

		/** Configures a vertex in preparation for batching
			@remarks Configures all hardware vertex elements (normal [if using gradient], position, colours, texture coordinates) for an isovertex
			@param pVtxElems The main vertex elements that provides information to everything
			@param pDataGrid Used to provide access to a vector basis for vertex positioning
			@param data Access to the voxel grid for discrete voxel sampling
			@param nIsoVertexIdx The isovertex index identifying the vertex element to configure
			@param t The percentage distance between the two corner voxels where the vertex position shall be
			@param corner0 Index of the first data grid value associated with the iso vertex
			@param corner1 Index of the second data grid value associated with the iso vertex
			@param dv Added to the position of the vertex */
		void configureIsoVertex( 
			IsoVertexElements * pVtxElems, 
			const Voxel::CubeDataRegion * pDataGrid, 
			Voxel::const_DataAccessor & data, 
			const IsoVertexIndex nIsoVertexIdx, 
			const IsoFixVec3::PrecisionType t, 
			const VoxelIndex corner0, const VoxelIndex corner1, 
			const IsoFixVec3 & dv = IsoFixVec3(signed short(0),signed short(0),signed short(0)) 
		);

		/** Computes the percentage distance between two voxels that an isovertex should occur based on the two voxel values
		@param pValues Field of voxels to discretely sample the pair
		@param corner0 Voxel index into the field of voxels for the first of the pair
		@param corner1 Voxel index into the field of voxels for the second of the pair
		@returns A percentage distance between the two voxels
		*/
		inline IsoFixVec3::PrecisionType computeIsoVertexPosition(const FieldStrength * pValues, const VoxelIndex corner0, const VoxelIndex corner1) const;

		/// Create isovertex index group offsets 
		void createIsoVertexGroupOffsets();

		/** Utility for iterating through the isovertices of a case in a cell
		@param caze The triangulation case
		@param cw The iterator used for walking through cells
		@param fn Callback lambda function for each iteration passing the cell walker, vertex code, and LOD as formal parameters
		*/
		template< typename Case, typename CellWalker, typename Fn >
		void walkCellVertices(const Case & caze, CellWalker & cw, Fn fn);

		/** Utility for iterating through the isovertices of a set of cells
		@param begin Iterator pattern head of the set of triangulation cases to iterate vertices of
		@param end Iterator pattern end of the set of triangulation cases to iterate vertices of
		@param cw The iterator used for walking through cells
		@param fn Callback lambda function for each iteration passing the cell walker, vertex code, and LOD as formal parameters
		*/
		template< typename Iter, typename CellWalker, typename Fn >
		void walkCellVertices(const Iter begin, const Iter end, CellWalker & cw, Fn fn);

		/** Walks through all refined mapped isovertices of a set of regular triangulation cases
		@param begin Iterator pattern head of the set of triangulation cases to iterate vertices of
		@param end Iterator pattern end of the set of triangulation cases to iterate vertices of
		@param gc The cell walker
		@param fn Callback lambda function for each iteration passing the cell walker, vertex code, and LOD as formal parameters
		*/
		template< typename Fn > 
		void processRegularIsoVertices (const RegularTriangulationCaseList::const_iterator begin, const RegularTriangulationCaseList::const_iterator end, GridCell & gc, Fn fn);

		/** Refines the isovertex specified by the meta-information 
		@remarks Refines the isovertex and calls the callback function given a transition cell and vertex code to identify it
		@param data Provides access to the voxel field for discrete sampling and isovertex refinement
		@param tc The cell walker
		@param vrecacc The Transvoxel vertex code of the isovertex in the transition cell
		@param nLOD The LOD affecting the size of the transition cell
		@param fn The callback function which will pass the refined isovertex index, the vertex code, the voxel indices of the voxels immediately surrounding the refined isovertex, and any 3D side the isovertex is flush with
		*/
		template< typename Fn >
		void collectTransitionVertexProperties(Voxel::const_DataAccessor & data, const TransitionCell & tc, const TransitionVRECaCC & vrecacc, const unsigned nLOD, Fn fn);

		/** Refines all isovertices specified by the meta-information 
		@remarks Refines all isovertices of the transition triangulation case and calls the callback function for each isovertex given a transition cell and vertex code to identify it
		@param data Provides access to the voxel field for discrete sampling and isovertex refinement
		@param caze The transition triangulation case to walk isovertices of
		@param tc The cell walker
		@param fn The callback function which will pass each refined isovertex index, its vertex code, the voxel indices of the voxels immediately surrounding the current refined isovertex, and any 3D side the current isovertex is flush with
		*/
		template< typename Fn >
		void collectTransitionVertexProperties(Voxel::const_DataAccessor & data, const NonTrivialTransitionCase & caze, TransitionCell & tc, Fn fn);

		/** Refines all isovertices specified by the meta-information 
		@remarks Refines all isovertices of all triangulation cases specified and calls the callback function for each isovertex given a transition cell and vertex code to identify it
		@param data Provides access to the voxel field for discrete sampling and isovertex refinement
		@param begin Iterator pattern head of the set of triangulation cases to iterate vertices of
		@param end Iterator pattern end of the set of triangulation cases to iterate vertices of
		@param tc The cell walker
		@param fn The callback function which will pass each refined isovertex index, its vertex code, the voxel indices of the voxels immediately surrounding the current refined isovertex, and any 3D side the current isovertex is flush with
		*/
		template< typename Fn >
		void collectTransitionVertexProperties(Voxel::const_DataAccessor & data, const TransitionTriangulationCaseList::const_iterator begin, const TransitionTriangulationCaseList::const_iterator end, TransitionCell & tc, Fn fn);

		/** Walks through all refined mapped isovertices of a set of transition triangulation cases
		@param begin Iterator pattern head of the set of triangulation cases to iterate vertices of
		@param end Iterator pattern end of the set of triangulation cases to iterate vertices of
		@param tc The cell walker
		@param fn Callback lambda function for each iteration passing the refined isovertex index, its vertex code, the voxel indices of the voxels immediately surrounding the current refined isovertex, and any 3D side the current isovertex is flush with
		*/
		template< typename Fn >
		void processTransitionIsoVertices (const TransitionTriangulationCaseList::const_iterator begin, const TransitionTriangulationCaseList::const_iterator end, TransitionCell & tc, Fn fn);

		/** Populates the coarse/refined iso-vertex index LUT for regular iso-vertices
		@remarks Computes the refined isovertex index for an isovertex contained within a regular grid cell given a vertex code
		@param data Voxel grid used for discrete sampling of voxels during refinement
		@param gc The grid-cell within which the isovertex resides
		@param nVRECaCC Vertex code identifying the isovertex within the cell
		@param fn Callback function which will pass the coarse isovertex index, refined isovertex, the voxel indices of the voxels immediately surrounding the refined isovertex, and the current builder LOD as actual parameters
		*/
		template< typename Fn >
		void computeRegularRefinements(Voxel::const_DataAccessor & data, const GridCell & gc, const unsigned short nVRECaCC, Fn fn);
		
		/** Populates the coarse/refined iso-vertex index LUT for regular iso-vertices
		@remarks Computes the refined isovertex indices for all regular isovertices
		@param data Voxel grid used for discrete sampling of voxels during refinement
		*/
		void computeRegularRefinements (Voxel::const_DataAccessor & data);

		/** Populates the coarse/refined iso-vertex index LUT for regular iso-vertices
		@remarks Computes the refined isovertex index for all isovertices in a regular grid cell
		@param data Voxel grid used for discrete sampling of voxels during refinement
		@param gc The grid-cell to walk all isovertices of
		@param caze The regular triangulation case for the cell whose isovertices will be refined
		@param fn Callback function which will pass the coarse isovertex index, refined isovertex, the voxel indices of the voxels immediately surrounding the refined isovertex, and the current builder LOD as actual parameters
		*/
		template< typename Fn >
		void computeRegularRefinements (Voxel::const_DataAccessor & data, const GridCell & gc, const NonTrivialRegularCase & caze, Fn fn);

		/** Populates the coarse/refined iso-vertex index LUT for transition iso-vertices
		@remarks Computes the refined isovertex index an isovertex in a transition cell identified by a vertex code
		@param data Voxel grid used for discrete sampling of voxels during refinement
		@param tc The transition cell to walk all isovertices of
		@param nVRECaCC Vertex code identifying the isovertex within the cell
		@param nLOD LOD of the transition cell (overrides LOD of 'tc')
		@param fn Callback function which will pass the coarse isovertex index, refined isovertex, the TransitionVRECaCC::Type, the voxel indices of the voxels immediately surrounding the refined isovertex, and any 3D side the isovertex is flush with
		*/
		template< typename Fn >
		void computeTransitionRefinements(Voxel::const_DataAccessor & data, const TransitionCell & tc, const unsigned short nVRECaCC, const unsigned nLOD, Fn fn);

		/** Populates the coarse/refined iso-vertex index LUT for transition iso-vertices of the specified side
		@remarks Computes the refined isovertex index for all isovertices in all transition cells on a side of a voxel cube
		@param on The side of the voxel cube to refine transition isovertices of
		@param data Voxel grid used for discrete sampling of voxels during refinement
		*/
		void computeTransitionRefinements (const OrthogonalNeighbor on, Voxel::const_DataAccessor & data);

		/** Populates the coarse/refined iso-vertex index LUT for transition iso-vertices of the specified side
		@remarks Computes the refined isovertex index for all isovertices in a transition cell
		@param data Voxel grid used for discrete sampling of voxels during refinement
		@param tc The transition cell to walk all isovertices of
		@param caze The transition triangulation case for the cell whose isovertices will be refined
		@param fn Callback function which will pass the coarse isovertex index, refined isovertex, the TransitionVRECaCC::Type, the voxel indices of the voxels immediately surrounding the refined isovertex, and any 3D side the isovertex is flush with
		*/
		template< typename Fn >
		void computeTransitionRefinements (Voxel::const_DataAccessor & data, const TransitionCell & tc, const NonTrivialTransitionCase & caze, Fn fn);

		/// Clears all transition information vectors, always done before population
		void clearTransitionInfo();

		/// Builds the iso surface by triangulating from the linear array of cases previously attained
		void triangulateRegulars();

		/// Configures and loads vertices into a shipping container
		void marshalRegularVertexElements( const Voxel::CubeDataRegion * pDataGrid, Voxel::const_DataAccessor & data );

		/// Loops through all the grid cells collecting non-trivial triangulation cases into a linear array
		void attainRegularTriangulationCases( Voxel::const_DataAccessor & data );

		/// Loops through all relevant (stitched) transition cells collecting non-trivial triangulation cases into a linear array
		void attainTransitionTriangulationCases (Voxel::const_DataAccessor & data, OrthogonalNeighbor on);

		/// Applies all transition vertex element details and marshals them for flushing to the hardware buffer
		void marshalTransitionVertexElements( const Voxel::CubeDataRegion * pDataGrid, Voxel::const_DataAccessor & data, const OrthogonalNeighbor on );

		/// Collects information about the iso-vertices flush along cube boundaries that coincide with cube edges
		void collectTransitionVertexProperties( Voxel::const_DataAccessor & data, const OrthogonalNeighbor on );

		/// Restores all iso-vertex mappings that result in inserted transition cells between the regular mesh and the border(s)
		void restoreTransitionVertexMappings( Voxel::const_DataAccessor & data );
		/** Restores iso-vertex mappings that result in inserted transition cells between the regular mesh and the border(s)
		@remarks Refines and restores isovertex index mappings for a specific transition isovertex index given a transition cell and vertex code and coarse isovertex index
		@param data Voxel grid used for discrete sampling of voxels during refinement
		@param vrecacc Vertex code identifying the isovertex within the cell
		@param touch The side of the 3D cube the isovertex touches
		@param tc The transition cell to walk all isovertices of
		@param index The coarse isovertex index
		*/
		void restoreTransitionVertexMappings( Voxel::const_DataAccessor & data, TransitionVRECaCC vrecacc, const Touch3DSide touch, TransitionCell tc, const IsoVertexIndex index );
		
		/// Collects and restores transition vertex properties and mappings for a particular transition triangulation case in a particular transition cell
		void computeTransitionVertexMappings( Voxel::const_DataAccessor & data, const NonTrivialTransitionCase & caze, TransitionCell & tc);

		/// Builds the iso surface by triangulating from the linear array of transition cases previously attained
		void triangulateTransitions();

		/// For vertices that occur between transition cells and regular cells of a block, align them according to the vertex normal of the corresponding vertex on the full-resolution side
		void alignTransitionVertices ();

		/// Aligns a specific isovertex that occurs between a transition cell and regular cell of a block given its index and the sides of the cube it touches
		void alignTransitionVertex( const IsoVertexIndex index, const Touch3DSide t3ds );

		/// Adds an iso-triangle to an array in preparation for flushing it to the hardware buffers
		void addIsoTriangle(const IsoTriangle& isoTriangle);

		/** Performs a ray triangle collision test
		Tests for ray intersection with a triangle given three isovertex indices and a ray, isovertices must be already configured
		See the following links for details:
		* - http://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld017.htm
		* - http://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld018.htm
		* - http://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld019.htm
		@param pDist Receives the distance from the ray point of origin to the triangle intersection point if applicable
		@param ray The ray
		@param a Isovertex index of the first triangle vertex
		@param b Isovertex index of the first triangle vertex
		@param c Isovertex index of the first triangle vertex
		@returns True if the ray intersected the triangle at some point, false if the ray does not intersect the triangle
		*/
		bool rayCollidesTriangle(Real * pDist, const Ray & ray, const IsoVertexIndex a, const IsoVertexIndex b, const IsoVertexIndex c) const;
		
// Temporary for debugging until this stitching thing works
#if defined(_DEBUG) || defined(_OHT_LOG_TRACE)
		DebugInfo _debugs;
		Vector3 getWorldPos(const IsoVertexIndex nIsoVertex) const;
		Vector3 getWorldPos( const IsoFixVec3 & fv ) const;

		String drawGridCell( const size_t nCornerFlags, const int nIndent = 1 ) const;
		String drawTransitionCell( const size_t nCornerFlags, const int nIndent = 1 ) const;

		template< typename CellWalker >
		String drawCell (const size_t nCornerFlags, const int nIndent) const;
		
		template<>
		String drawCell< GridCell > (const size_t nCornerFlags, const int nIndent) const 
			{ return drawGridCell(nCornerFlags, nIndent); }

		template<>
		String drawCell< TransitionCell > (const size_t nCornerFlags, const int nIndent) const 
			{ return drawTransitionCell(nCornerFlags, nIndent); }

		template<>
		String drawCell< GridCell const > (const size_t nCornerFlags, const int nIndent) const 
			{ return drawGridCell(nCornerFlags, nIndent); }

		template<>
		String drawCell< TransitionCell const > (const size_t nCornerFlags, const int nIndent) const 
			{ return drawTransitionCell(nCornerFlags, nIndent); }
#endif

		friend Ogre::Log::Stream & operator << (Ogre::Log::Stream &, const GridCell &);
		friend Ogre::Log::Stream & operator << (Ogre::Log::Stream &, const TransitionCell &);
	};

	Ogre::Log::Stream & operator << (Ogre::Log::Stream & s, const IsoSurfaceBuilder::GridCell & gc);
	Ogre::Log::Stream & operator << (Ogre::Log::Stream & s, const IsoSurfaceBuilder::TransitionCell & tc);

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	template< typename Case, typename CellWalker, typename Fn >
	void IsoSurfaceBuilder::walkCellVertices(const Case & caze, CellWalker & cw, Fn fn)
	{
#if defined(_DEBUG) || defined(_OHT_LOG_TRACE)
		OHT_ISB_DBGTRACE("Cell " << cw << ": " << drawCell< CellWalker >(caze.casecode, 1));
#endif

		const unsigned short * vVertices = getVertexData(caze);
		const unsigned int nVertCount = getVertexCount(caze);

		for (unsigned int c = 0; c < nVertCount; ++c)
		{
			const unsigned short nVRECaCC = vVertices[c];
			fn(cw, nVRECaCC, _nLOD);
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	template< typename Iter, typename CellWalker, typename Fn >
	void IsoSurfaceBuilder::walkCellVertices(const Iter begin, const Iter end, CellWalker & cw, Fn fn)
	{
		for (Iter i = begin; i != end; ++i)
		{
			cw = i->cell;
			walkCellVertices(*i, cw, fn);
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	template< typename Fn >
	void IsoSurfaceBuilder::collectTransitionVertexProperties(Voxel::const_DataAccessor & data, const TransitionCell & tc, const TransitionVRECaCC & vrecacc, const unsigned nLOD, Fn fn)
	{
#ifdef _DEBUG
		static const char VRECaCC_TypeNames[3] = { 'O', 'H', 'I' };
#endif

		IsoVertexIndex refined, coarse;
		VoxelIndex c0, c1;
		CubeSideCoords csc;
		Touch2DSide rside;
		TouchStatus tsx, tsy;
		unsigned char ei;

		coarse = _pMainVtxElems->getTransitionIndex(tc, vrecacc);

		if (!_pMainVtxElems->trackFullOutsides[coarse])
		{
			computeRefinedTransitionIsoVertex(tc, data, vrecacc, ei, c0, c1, tsx, tsy, rside, refined, csc);
#ifdef _DEBUG
			OHT_ISB_DBGTRACE("\tIso-Vertex: " << coarse << " -> " << refined << ", ei=" << (int)ei << ", corner pairs=< " << c0 << '/' << _cubemeta.getVertices()[c0] << 'x' << c1 << '/' << _cubemeta.getVertices()[c1] << " >, " << csc << ", type=" << VRECaCC_TypeNames[vrecacc.getType()]);
#endif // _DEBUG

			fn(
				refined, 
				vrecacc,
				c0, c1, 
				_pMainVtxElems->getTouch3DSide(tc.side, rside, ei)
			);
			_pMainVtxElems->trackFullOutsides[coarse] = 1;
		}
	}

	
#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	template< typename Fn >
	void IsoSurfaceBuilder::collectTransitionVertexProperties(Voxel::const_DataAccessor & data, const NonTrivialTransitionCase & caze, TransitionCell & tc, Fn fn)
	{
		walkCellVertices(caze, tc, [&] (const TransitionCell & tc, const unsigned short nVRECaCC, const unsigned nLOD)
		{
			collectTransitionVertexProperties(data, tc, nVRECaCC, nLOD, fn);
		});
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	template< typename Fn >
	void IsoSurfaceBuilder::collectTransitionVertexProperties(Voxel::const_DataAccessor & data, const TransitionTriangulationCaseList::const_iterator begin, const TransitionTriangulationCaseList::const_iterator end, TransitionCell & tc, Fn fn)
	{
		walkCellVertices(begin, end, tc, [&] (const TransitionCell & tc, const unsigned short nVRECaCC, const unsigned nLOD)
		{
			collectTransitionVertexProperties(data, tc, nVRECaCC, nLOD, fn);
		});
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	template< typename Fn >
	void IsoSurfaceBuilder::processTransitionIsoVertices (const TransitionTriangulationCaseList::const_iterator begin, const TransitionTriangulationCaseList::const_iterator end, TransitionCell & tc, Fn fn)
	{
		walkCellVertices(begin, end, tc, [&] (const TransitionCell & tc, const unsigned short nVRECaCC, const unsigned nLOD)
		{
			const TransitionVRECaCC vrecacc(nVRECaCC);
			IsoVertexIndex coarse, refined;
			Touch2DSide rside;
			
			_pMainVtxElems->computeTransitionIndexProperties(tc, vrecacc, coarse, rside),
			refined = _pMainVtxElems->refinements[coarse];
			OgreAssert(refined != IsoVertexIndex(~0), "Unmapped refinement");

			static_assert(sizeof(int) == 4, "Expected a 4-byte DWORD");

			OHT_ISB_DBGTRACE("\tIso-Vertex: " << coarse << " -> " << refined);

			if (_pMainVtxElems->indices[refined] == HWVertexIndex(~0))
			{
				const IsoSurfaceBuilder::MainVertexElements::CellIndexPair & pair
					= _pMainVtxElems->cellindices[refined];

				/// Returns a mask if both corners are the same
				const int m = bitmanip::testZero(int(pair.corner0) - int(pair.corner1));

				/// Determines the 3D touch side based on two alternatives
				const Touch3DSide side3d =
					Touch3DSide(
						(_cubemeta.getTouchSide(pair.corner0) & m)
						|
						(_pMainVtxElems->getTouch3DSide(tc.side, rside, vrecacc.getEdgeCode()) & ~m)
					);

				fn(
					refined, 
					vrecacc.getType(),
					pair.corner0, pair.corner1, 
					side3d
				);
			}
		});
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	template< typename Fn >
	void IsoSurfaceBuilder::processRegularIsoVertices (const RegularTriangulationCaseList::const_iterator begin, const RegularTriangulationCaseList::const_iterator end, GridCell & gc, Fn fn)
	{
		walkCellVertices(begin, end, gc, [&] (const GridCell & gc, const unsigned short nVRECaCC, const unsigned nLOD)
		{
			const VRECaCC vrecacc(nVRECaCC);
			const IsoVertexIndex coarse = _pMainVtxElems->getRegularVertexIndex(gc, vrecacc, _nLOD);
			const IsoVertexIndex refined = _pMainVtxElems->refinements[coarse];

			OgreAssert(refined != IsoVertexIndex(~0), "Unmapped refinement");
			if (_pMainVtxElems->indices[refined] == HWVertexIndex(~0))
			{
				const IsoSurfaceBuilder::MainVertexElements::CellIndexPair & pair = _pMainVtxElems->cellindices[refined];
				fn(
					refined,
					pair.corner0, 
					pair.corner1
				);
			}
		});
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	template< typename Fn >
	void IsoSurfaceBuilder::computeRegularRefinements( Voxel::const_DataAccessor & data, const GridCell & gc, const unsigned short nVRECaCC, Fn fn )
	{
		VoxelIndex c0, c1;
		GridPointCoords gpc;
		unsigned char ei;
		VRECaCC vrecacc(nVRECaCC);
		IsoVertexIndex coarse = _pMainVtxElems->getRegularVertexIndex(gc, vrecacc, _nLOD);

		if (_pMainVtxElems->refinements[coarse] == IsoVertexIndex(~0))
		{
			IsoVertexIndex ivi;
			computeRefinedRegularIsoVertex(
				gc, data, vrecacc, ei, c0, c1, _nLOD, 
				ivi, 
				gpc
			);
			_pMainVtxElems->refinements[coarse] = ivi;
			fn(coarse, ivi, c0, c1, _nLOD);
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	template< typename Fn >
	void IsoSurfaceBuilder::computeRegularRefinements( Voxel::const_DataAccessor & data, const GridCell & gc, const NonTrivialRegularCase & caze, Fn fn )
	{
		walkCellVertices
		(
			caze,
			gc, 
			[&] (const GridCell & gc, const unsigned short nVRECaCC, const unsigned nLOD)
			{
				computeRegularRefinements(data, gc, nVRECaCC, fn);
			}
		);
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	template< typename Fn >
	void IsoSurfaceBuilder::computeTransitionRefinements( Voxel::const_DataAccessor & data, const TransitionCell & tc, const unsigned short nVRECaCC, const unsigned nLOD, Fn fn )
	{
		VoxelIndex c0, c1;
		CubeSideCoords csc;
		Touch2DSide rside;
		TouchStatus tsx, tsy;
		unsigned char ei;

		TransitionVRECaCC vrecacc(nVRECaCC);
		IsoVertexIndex coarse = _pMainVtxElems->getTransitionIndex(tc, vrecacc);

		if (_pMainVtxElems->refinements[coarse] == IsoVertexIndex(~0))
		{
			IsoVertexIndex ivi;
			computeRefinedTransitionIsoVertex(
				tc, data, vrecacc, ei, c0, c1, tsx, tsy, rside, 
				ivi, 
				csc
			);
			OHT_ISB_DBGTRACE("Transition Refinement: " << coarse << " -> " << ivi << ", vertex pair={" << c0 << '/' << getWorldPos(_cubemeta.getVertices()[c0]) << 'x' << c1 << '/' << getWorldPos(_cubemeta.getVertices()[c1]) << "}");
			_pMainVtxElems->refinements[coarse] = ivi;
			fn(coarse, ivi, vrecacc.getType(), c0, c1, _pMainVtxElems->getTouch3DSide(tc.side, rside, ei));
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	template< typename Fn >
	void IsoSurfaceBuilder::computeTransitionRefinements( Voxel::const_DataAccessor & data, const TransitionCell & tc, const NonTrivialTransitionCase & caze, Fn fn )
	{
		walkCellVertices(
			caze,
			tc,
			[&] (const TransitionCell & tc, const unsigned short nVRECaCC, const unsigned nLOD)
			{
				computeTransitionRefinements(data, tc, nVRECaCC, nLOD, fn);
			}
		);
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	inline IsoFixVec3::PrecisionType IsoSurfaceBuilder::computeIsoVertexPosition( const FieldStrength * pValues, const VoxelIndex corner0, const VoxelIndex corner1 ) const
	{
		// Calculate the transition of the iso vertex between the two corners
		OgreAssert(pValues[corner1] - pValues[corner0] != 0 || corner1 == corner0, "Cannot divide by zero");
		const long m = bitmanip::testZero(long(corner1 - corner0));
		IsoFixVec3::PrecisionType t = IsoFixVec3::PrecisionType(signed short(pValues[corner1]));
		// We treat this like projective geometry that axiomatically states there is no such thing as parallel planes or lines, they always eventually meet somewhere
		t /= ((pValues[corner1] - pValues[corner0]) | (m & 1));	
		t &= ~m;
		return t;
	}

}/// namespace Ogre
#endif //_ISO_SURFACE_BUILDER_H_
