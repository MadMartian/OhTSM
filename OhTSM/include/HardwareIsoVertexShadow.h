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

#ifndef __OVERHANGTERRAINHARDWAREISOVERTEXSHADOW_H__
#define __OVERHANGTERRAINHARDWAREISOVERTEXSHADOW_H__

#include "OverhangTerrainPrerequisites.h"

#include "Neighbor.h"
#include "IsoSurfaceSharedTypes.h"

#include <OgreSharedPtr.h>
#include <OgreVector2.h>
#include <OgreVector3.h>

#include <vector>

namespace Ogre
{
	namespace HardwareShadow
	{
		/** IsoSurfaceRenderable shadow meta data container for data pertinent to a specific LOD */
		class LOD
		{
		private:
			/// Maps logical isovertex indices to their hardware vertex index counterparts
			IsoVertexVector _revmapIVI2HWVI;
			/// Flag to indicate to the main thread whether the hardware buffers should be reset immediately before render
			bool _bRollbackHWBuffers;

		public:
			/** IsoSurfaceRenderable shadow meta data container for data pertinent to a specific LOD 
				and multi-resolution stitching configuration for a particular side of the cube voxel region */
			class Stitch
			{
			public:
				/** The side of the 3D cube region of space that the stitch applies to, indicates that the adjacent cube
					has a higher resolution (lower ordinal LOD value) */				
				const OrthogonalNeighbor side;
				/// Cache for marching cubes transition triangulation cases along this side (cf. Transvoxel)
				TransitionTriangulationCaseList transCases;

				/** Flags that indicate the state of data of this object (Stitch object), respectively 'shadowed' indicates if IsoSurfaceBuilder
					has finished populating this with data and 'gpued' indicates if the associated triangles have been batched to the GPU
				*/
				bool shadowed, gpued;

				/// Define a configuration for the specified side
				Stitch(const OrthogonalNeighbor side);
			};

			/// Identifies the pertinent level of detail ordinal value (zero for highest resolution, >0 for lower resolutions)
			const unsigned lod;

			/// Cache for marching cubes triangulation cases (cf. Transvoxel)
			RegularTriangulationCaseList regCases;
			
			/** Flags that indicate the state of data of this object (LOD object), respectively 'shadowed' indicates if IsoSurfaceBuilder
				has finished populating this with data and 'gpued' indicates if the associated triangles have been batched to the GPU
			*/
			bool shadowed, gpued;

			/// The set of all 6 stitch shadow meta data containers for each side of the voxel cube
			Stitch * stitches[CountOrthogonalNeighbors];

			/** Precomputed and frequently used data of isovertices that occur along the high-resolution face and low-resolution 
				face of transition cells respectively */
			BorderIsoVertexPropertiesVector borderIsoVertexProperties, middleIsoVertexProperties;

			/// Define a configuration for the specified LOD
			LOD(const unsigned nLOD);
			~LOD();

			/// Returns the next hardware vertex buffer index for new vertices that would be appended to the buffer
			size_t getHWIndexBufferTail() const { return _revmapIVI2HWVI.size(); }
			/// Populates the specified map of isovertex indices to hardware vertex buffer indices from that stored in this object
			void restoreHWIndices(HWVertexIndex * mapIVI2HWVI);
			/// Batch-map the specified set of isovertex indices to new hardware vertex buffer indices
			void addHWIndices(const IsoVertexVector::const_iterator begin, const IsoVertexVector::const_iterator end);
			/// Determines if the hardware buffers should be reset in the main thread immediately before render
			bool isResetHWBuffers() const { return _bRollbackHWBuffers; }
			/// Clears the hardware vertex buffer reset flag, which is done immediately after the buffers are cleared and before render
			void clearResetHWBuffersFlag() { _bRollbackHWBuffers = false; }

			/** Resets all data stored in this cache
			@remarks Clears the following pieces of information:
			- Regular and transition triangulation case lists
			- Transition cell vertex property lists
			- Isovertex to hardware vertex buffer index map
			- Clears all flags except for...
			- Sets the flag signaling that the hardware buffers should be reset in the main thread
			- Resets all flags of each stitch configuration object
			*/
			void clearAll();
			/** Clears the flags, for both this object and its stitch configuration objects,
				indicating that associated triangles have been batched to the GPU, and sets the flag 
				indicating that the hardware buffers should be reset in the main thread */
			void clearHardwareState();

			/** Sets the flags indicating that associated triangles have just been batched to the GPU
			@remarks Sets the flags for both this object and the relevant stitch configuration objects that triangles have been batched.
				Also maps (appends) the specified isovertex indices to hardware vertex buffer indices
			@param enStitches Flags identifying whitch stitch configuration objects have been updated and to set the gpued flag for
			@param beginRevMapIVI2HWVI Iterator pattern pointer to beginning of isovertex indices to map to hardware vertex buffer indices
			@param endRevMapIVI2HWVI Iterator pattern pointer to end of isovertex indices to map to hardware vertex buffer indices
			*/
			void updateHardwareState(const Touch3DFlags enStitches, const IsoVertexVector::const_iterator beginRevMapIVI2HWVI, const IsoVertexVector::const_iterator endRevMapIVI2HWVI);
		};

		/** An object that can be used to update a hardware vertex buffer */
		class BuilderQueue
		{
		public:
			typedef std::vector< HWVertexIndex > IndexList;

			/** Contains necessary hardware vertex buffer information for a single vertex element */
			class VertexElement
			{
			public:
				/// Vertex position and normal
				Vector3 position, normal;
				/// Vertex colour
				uint32 colour;
				/// Vertex texture coordinate
				Vector2 texcoord;

				VertexElement() {}
				/// Initialize this object's members to these specified
				VertexElement(const Vector3 & vPos, const Vector3 & vNorm, const ColourValue & cColour, const Vector2 & vTexCoord);

				/// Set the colour according to the specified ColourValue object
				void setColour(const ColourValue & cColour);
			};
			typedef std::vector< VertexElement > VertexElementList;

			/// Identifies which sides of the renderable transition cells apply
			const Touch3DFlags stitches;
			/// The LOD and meta data for the rendered surface
			LOD * const resolution;

			/// Vertex elements to be flushed to the appropriate hardware vertex buffer
			VertexElementList vertexQueue;
			/// Vertex indices to be flushed to the appropriate hardware index buffer
			IndexList indexQueue;
			/// Identifies the new vertices to be appended to the hardware buffers, maps isovertex indices to their respective hardware vertex indices
			IsoVertexVector revmapIVI2HWVIQueue;

			BuilderQueue(LOD * pResolution, const Touch3DFlags enStitches)
				: resolution(pResolution), stitches(enStitches) {}
		};

		/** IsoSurfaceRenderable shadow meta data container, precomputed and cached frequently used data, synchronized with hardware buffers */
		class HardwareIsoVertexShadow
		{
		private:
			OGRE_MUTEX(_mutex);

			/** Container for the geometry batch data and associated shadow meta data for a pending GPU batch operation */
			class ConcurrentProducerConsumerQueueBase
			{
			protected:
				/// Pointer to the geometry batch data container
				BuilderQueue *& _pBuilderQueue;

			public:
				/// The stitch configuration of the batch
				const Touch3DFlags stitches;	
				/// The LOD meta data corresponding to the batch
				LOD * const resolution;

				/// Buffer of vertex elements to batch
				BuilderQueue::VertexElementList & vertexQueue;
				/// Buffer of the triangle index list to batch
				BuilderQueue::IndexList & indexQueue;
				/// Maps new isovertices to hardware vertex indices corresponding to the batch operation pending
				IsoVertexVector & revmapIVI2HWVIQueue;

				ConcurrentProducerConsumerQueueBase(BuilderQueue *& pBuilderQueue);
				ConcurrentProducerConsumerQueueBase(ConcurrentProducerConsumerQueueBase && move);
			};

			/// Responsible for holding the instance of the BuilderQueue, receives the pointer generated by ProducerQueueAccess
			BuilderQueue * _pBuilderQueue;

		public:

			/** Class that encapsulates a shared lock on the hardware buffer batch data and provides access to it via openQueue() */
			class ConsumerLock
			{
			private:
				/// Disable copy constructor
				ConsumerLock(const ConsumerLock &);

				boost::recursive_mutex::scoped_try_lock _lock;

				/// Reference pointer to the hardware buffer batch data container
				BuilderQueue *& _pBuilderQueue;
				/// The LOD meta data to which this batch data applies
				const LOD * const _pResolution;
				/// The stitch flags of which this batch data applies
				const Touch3DFlags _enStitches;

				/// Determines if the shared lock was acquired and the hardware buffer batch data container is the right one according to LOD and stitch information
				inline
				bool isValid () const { return _lock && _pBuilderQueue != NULL && _pBuilderQueue->resolution == _pResolution && _pBuilderQueue->stitches == _enStitches; }

			public:
				ConsumerLock(boost::recursive_mutex::scoped_try_lock && lock, BuilderQueue *& pBuilderQueue, LOD * pResolution, const Touch3DFlags enStitches);
				ConsumerLock(ConsumerLock && move);

				/// @returns True if the shared lock was NOT acquired
				inline bool operator ! () const { return !isValid(); }
				/// @returns True if the shared lock WAS acquired
				inline operator bool ()  const { return isValid(); }

				/** Provides access to operations pertinent to the hardware buffer batch data */
				class QueueAccess : public ConcurrentProducerConsumerQueueBase
				{
				private:
					/// Used by rvalue constructor
					bool _bDeconstruct;

				public:
					/// Thrown if an attempt was made to open the queue without a lock
					class AccessEx : public std::exception {};

					QueueAccess(BuilderQueue *& pBuilderQueue);
					QueueAccess(QueueAccess && move);
					~QueueAccess();

					/// Returns the number of required vertices to store in the hardware vertex buffer for the batch operation pending
					inline
					size_t getRequiredHardwareVertexCount() const { return resolution->getHWIndexBufferTail() + vertexQueue.size(); }
					/// Updates the hardware state of the shadow meta data signalling that geometry has been batched
					void consume();
				} openQueue () /// Provides access to the geometry batch data container
				{ 
					if (isValid())
						return QueueAccess(_pBuilderQueue); 
					else
						throw QueueAccess::AccessEx();
				}
			};

			/** Used to prepare the geometry batch data, implies an exclusive lock */
			class ProducerQueueAccess : public ConcurrentProducerConsumerQueueBase
			{
			private:
				ProducerQueueAccess (const ConcurrentProducerConsumerQueueBase &);

				boost::recursive_mutex::scoped_lock _lock;

				/// Deletes and reconstructs the specified geometry bach data container object
				BuilderQueue *& reallocate(BuilderQueue *& pBuilderQueue, LOD * pResolution, const Touch3DFlags enStitches);

			public:
				/** Begin producer access for preparing geometry batch data.
				@remarks Implies an existing lock and allocates a new BuilderQueue based on the specified LOD resolution and stitch configuration
				@param lock An existing exclusive lock
				@param pBuilderQueue A new instance of BuilderQueue will be constructed, this will be set with the pointer to that object
				@param pResolution Identifies the LOD of the batch operation
				@param enStitches Identifies the stitch configuration of the batch operation
				*/
				ProducerQueueAccess(boost::recursive_mutex::scoped_lock && lock, BuilderQueue *& pBuilderQueue, LOD * pResolution, const Touch3DFlags enStitches);
				ProducerQueueAccess(ProducerQueueAccess && move);
			};

			/// Initialize with the specified maximum quantity of resolutions allowed from maximum resolution
			HardwareIsoVertexShadow(const unsigned nLODCount);
			~HardwareIsoVertexShadow();

			/** Request a shared read-only lock on shadow data pertinent to the specified LOD and stitch configuration.
			@remarks Tries to acquire a shared lock on the resource, if it fails the ConsumerLock object overloaded boolean operator
				will return false (or the overloaded '!' operator will return true).  API consumers must check this flag before attempting
				to open the queue or a QueueAccess::AccessEx exception will result.
			*/
			ConsumerLock requestConsumerLock(const unsigned char nLOD, const Touch3DFlags enStitches);
			/// Request an exclusive read/write lock on shadow data pertinent to the specified LOD and stitch configuration
			ProducerQueueAccess requestProducerQueue(const unsigned char nLOD, const Touch3DFlags enStitches);
			/// Retrieve the LOD meta data container for the specified LOD ordinal (zero is highest resolution, >0 is lower resolution)
			LOD * getDirectAccess(const unsigned char nLOD) { return _vpResolutions[nLOD]; }

			/// Calls LOD::clearAll() for all LOD shadow meta data containers
			void clear ();
			/// Clears the hardware state for all LOD shadow meta data containers
			void reset ();

		private:
			/// Maximum quantity of detail levels
			const unsigned char _nCountResolutions;
			/// A shadow LOD meta data container object for each level of detail
			LOD ** const _vpResolutions;

		};
	}
}

#endif
