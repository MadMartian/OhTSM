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
					has finished populating this with data and 'gpued' indicates if the associated triangles have been batched to the GPU */
				bool shadowed, gpued;

				/// Define a configuration for the specified side
				Stitch(const OrthogonalNeighbor side);
			};

			/// Identifies the pertinent level of detail ordinal value (zero for highest resolution, >0 for lower resolutions)
			const unsigned lod;

			/// Cache for marching cubes triangulation cases (cf. Transvoxel)
			RegularTriangulationCaseList regCases;
			
			/** Flags that indicate the state of data of this object (LOD object), respectively 'shadowed' indicates if IsoSurfaceBuilder
				has finished populating this with data and 'gpued' indicates if the associated triangles have been batched to the GPU */
			bool shadowed, gpued;

			/// The set of all 6 stitch shadow meta data containers for each side of the voxel cube
			Stitch * stitches[CountOrthogonalNeighbors];

			/** Precomputed and frequently used data of isovertices that occur along the high-resolution face and low-resolution 
				face of transition cells respectively */
			BorderIsoVertexPropertiesVector borderIsoVertexProperties, middleIsoVertexProperties;

			/// Define a configuration for the specified LOD
			LOD(const unsigned nLOD);
			~LOD();
		};

		/** Simply houses shadow information for the vertices shared by all LODs and LOD configurations */
		class Vertices
		{
		public:
			/// Maps logical isovertex indices to their hardware vertex index counterparts
			IsoVertexVector revmapIVI2HWVI;
		};

		/** Simply houses shadow information for the indices shared by all LODs and LOD configurations */
		class Indices
		{
		public:
			/// The number of units (a single triangle vertex reference) currently used in the hardware buffer
			size_t allocated;
			/// The total number of units (a single triangle vertex reference) the hardware buffer stores
			size_t capacity;

			/// Returns the free space available in number of units (a single triangle vertex reference)
			inline size_t free() const { return capacity - allocated; }
			/// Resets the state such that both the capacity and allocated are zero
			inline void clear()
			{
				allocated = capacity = 0;
			}
		};

		/** Houses the algorithm for clearing the shadow and GPU buffers */
		class IBufferManager
		{
		public:
			enum BufferDepth
			{
				BD_Shadow = 1,		// Clears everything, both GPU and shadow
				BD_GPU = 1 | 2		// Clears GPU only, shadow is left alone
			};

			/** Clears either the GPU, shadow, or both for vertex information
 			 * @param depth Indicates how deep to clear */
			virtual void clearVertices(const BufferDepth depth) = 0;

			/** Clears either the GPU, shadow, or both for index information
 			 * @param depth Indicates how deep to clear */
			virtual void clearIndices(const BufferDepth depth) = 0;
		};

		/** Houses the vertex and triangle data necessary for batching a single mesh operation to the hardware buffers */
		class MeshOperation
		{
		private:
			IBufferManager * _pBuffMan;

		public:
			// Indicates how deep to clear the buffers (currently only used for the clear method)
			/// The LOD and meta data for the rendered surface
			LOD * const resolution;
			/// The vertices shared by all configurations
			Vertices * const vertices;
			/// The triangle list index information shared by all configurations
			Indices * const indices;

			/// Initializes the members
			MeshOperation(LOD * pResolution, Vertices * pVertices, Indices * pIndices, IBufferManager * pBuffMan);

			/** Clears either the GPU, shadow, or both for vertex information
			 * @param depth Indicates how deep to clear */
			inline void clearVertices(const IBufferManager::BufferDepth depth) { _pBuffMan->clearVertices(depth); }

			/** Clears either the GPU, shadow, or both for index information
			 * @param depth Indicates how deep to clear */
			inline void clearIndices(const IBufferManager::BufferDepth depth) { _pBuffMan->clearIndices(depth); }

			/// Returns the next hardware vertex buffer index for new vertices that would be appended to the buffer
			size_t nextVertexIndex() const { return vertices->revmapIVI2HWVI.size(); }
			/// Populates the specified map of isovertex indices to hardware vertex buffer indices from that stored in this object
			void restoreHWIndices(HWVertexIndex * mapIVI2HWVI);

			inline bool operator == (const MeshOperation & other) const
			{
				return resolution == other.resolution && vertices == other.vertices;
			}
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
			/// The mesh operation to perform on the vertices and the associated vertex index LOD index
			MeshOperation meshOp;
			/// Flag indicating that the hardware vertex buffer must be reset, generated by the producer queue and consumed in the main thread
			RoleSecureFlag::Flag * pResetVertexHWBuffer;
			/// Flag indicating that the hardware index buffer must be reset, generated by the producer queue and consumed in the main thread
			RoleSecureFlag::Flag * pResetIndexHWBuffer;

			/// Vertex elements to be flushed to the appropriate hardware vertex buffer
			VertexElementList vertexQueue;
			/// Vertex indices to be flushed to the appropriate hardware index buffer
			IndexList indexQueue;
			/// Identifies the new vertices to be appended to the hardware buffers, maps isovertex indices to their respective hardware vertex indices
			IsoVertexVector revmapIVI2HWVIQueue;

			BuilderQueue(
				LOD *					pResolution, 
				Vertices *				pVertexStuff, 
				Indices *				pIndexStuff, 
				IBufferManager *		pBuffMan, 
				const Touch3DFlags		enStitches, 
				RoleSecureFlag::Flag *	pResetVertexHWBuffer,
				RoleSecureFlag::Flag *	pResetIndexHWBuffer
			);
			~BuilderQueue();
		};

		/** IsoSurfaceRenderable shadow meta data container, precomputed and cached frequently used data, synchronized with hardware buffers */
		class HardwareIsoVertexShadow : private IBufferManager
		{
		private:
			OGRE_RW_MUTEX(_mutex);

			class StateAccess
			{
			protected:
				/// Pointer to the geometry batch data container
				BuilderQueue *& _pBuilderQueue;

				/** Sets the flags indicating that associated triangles have just been batched to the GPU
				@remarks Sets the flags for both this object and the relevant stitch configuration objects that triangles have been batched.
					Also maps (appends) the specified isovertex indices to hardware vertex buffer indices
				@param beginRevMapIVI2HWVI Iterator pattern pointer to beginning of isovertex indices to map to hardware vertex buffer indices
				@param endRevMapIVI2HWVI Iterator pattern pointer to end of isovertex indices to map to hardware vertex buffer indices */
				void updateHardwareState(const IsoVertexVector::const_iterator beginRevMapIVI2HWVI, const IsoVertexVector::const_iterator endRevMapIVI2HWVI);

			public:
				/// The stitch configuration of the batch
				const Touch3DFlags stitches;	
				/// The mesh operation to perform on the vertices and the associated vertex index LOD index
				MeshOperation & meshOp;

				/// Maps new isovertices to hardware vertex indices corresponding to the batch operation pending
				IsoVertexVector & revmapIVI2HWVIQueue;

				StateAccess(BuilderQueue *& pBuilderQueue);
				StateAccess(StateAccess && move);
			};

			/** Container for the geometry batch data and associated shadow meta data for a pending GPU batch operation */
			class ConcurrentProducerConsumerQueueBase : public StateAccess
			{
			public:
				/// Buffer of vertex elements to batch
				BuilderQueue::VertexElementList & vertexQueue;
				/// Buffer of the triangle index list to batch
				BuilderQueue::IndexList & indexQueue;

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

				boost::shared_lock< boost::shared_mutex > _lock;

				/// Reference pointer to the hardware buffer batch data container
				BuilderQueue *& _pBuilderQueue;
				/// The mesh operation to perform on the vertices and the associated vertex index LOD index
				const MeshOperation _meshOp;
				/// The stitch flags of which this batch data applies
				const Touch3DFlags _enStitches;
				/// Access to clear (consume) the reset vertex buffer flag
				RoleSecureFlag::IClearFlag * _pClearResetVertexBufferFlag;
				/// Access to clear (consume) the reset index buffer flag
				RoleSecureFlag::IClearFlag * _pClearResetIndexBufferFlag;

				/// Determines if the shared lock was acquired and the hardware buffer batch data container is the right one according to LOD and stitch information
				inline
				bool isValid () const
				{
					return _lock &&
						_pBuilderQueue != NULL &&
						_pClearResetVertexBufferFlag != NULL &&
						_pClearResetIndexBufferFlag != NULL && 
						_pBuilderQueue->meshOp == _meshOp &&
						_pBuilderQueue->stitches == _enStitches; 
				}

			public:
				ConsumerLock(
					boost::shared_lock< boost::shared_mutex > &&	lock, 
					BuilderQueue *&									pBuilderQueue, 
					LOD *											pResolution, 
					Vertices *										pVertices, 
					Indices *										pIndices, 
					IBufferManager *								pBuffMan, 
					RoleSecureFlag::IClearFlag *					pClearResetVertexBufferFlag, 
					RoleSecureFlag::IClearFlag *					pClearResetIndexBufferFlag, 
					const Touch3DFlags								enStitches
				);
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
					/// Access to clear the flag for resetting the vertex hardware buffer
					RoleSecureFlag::IClearFlag & resetVertexBuffer;
					/// Access to clear the flag for resetting the index hardware buffer
					RoleSecureFlag::IClearFlag & resetIndexBuffer;

					/// Thrown if an attempt was made to open the queue without a lock
					class AccessEx : public std::exception {};

					QueueAccess(
						BuilderQueue *&					pBuilderQueue, 
						RoleSecureFlag::IClearFlag *	pClearResetVertexBufferFlag, 
						RoleSecureFlag::IClearFlag *	pClearResetIndexBufferFlag
					);
					QueueAccess(QueueAccess && move);
					~QueueAccess();

					/// Returns the number of required vertices to store in the hardware vertex buffer for the batch operation pending including vertices already present in the hardware buffer
					inline
					size_t requiredVertexCount() const { return meshOp.nextVertexIndex() + vertexQueue.size(); }

					/// Returns the actual number of vertices required for storage in the hardware buffer accounting for buffer-resize and hence reset
					inline
					size_t actualVertexCount() const { return (!resetVertexBuffer ? meshOp.nextVertexIndex() : 0) + vertexQueue.size(); }

					/// Returns the offset into the vertex buffer to begin populating vertex data at, will be zero if the vertex buffer was recently resized
					inline
					size_t vertexBufferOffset () const { return !resetVertexBuffer ? meshOp.nextVertexIndex() : 0; }

					/// Returns the number of required indices to store in the hardware index buffer for the LOD configuration pending including configurations already present in the hardware buffer
					inline
					size_t requiredIndexCount() const { return meshOp.indices->allocated + indexQueue.size(); }

					/// Returns the actual number of indices required for storage in the hardware buffer accounting for buffer-resize and hence reset
					inline
					size_t actualIndexCount() const { return (!resetIndexBuffer ? meshOp.indices->allocated : 0) + indexQueue.size(); }

					/// Returns the offset into the index buffer to begin populating index data at, will be zero if the index buffer was recently resized
					inline
					size_t indexBufferOffset () const { return !resetIndexBuffer ? meshOp.indices->allocated : 0; }

					/// Updates the hardware state of the shadow meta data signaling that geometry has been batched
					void consume();
				} openQueue () /// Provides access to the geometry batch data container
				{ 
					if (isValid())
						return QueueAccess(_pBuilderQueue, _pClearResetVertexBufferFlag, _pClearResetIndexBufferFlag); 
					else
						throw QueueAccess::AccessEx();
				}
			};

			/** Used to prepare the geometry batch data, implies an exclusive lock */
			class ProducerQueueAccess : public ConcurrentProducerConsumerQueueBase
			{
			private:
				ProducerQueueAccess (const ConcurrentProducerConsumerQueueBase &);

				boost::unique_lock< boost::shared_mutex > _lock;

				/// Deletes and reconstructs the specified geometry batch data container object
				BuilderQueue *& reallocate(
					BuilderQueue *&						pBuilderQueue, 
					LOD *								pResolution, 
					Vertices *							pVertices, 
					Indices *							pIndices, 
					IBufferManager *					pBuffMan, 
					const Touch3DFlags					enStitches, 
					RoleSecureFlag::Flag *				pResetVertexBufferFlag, 
					RoleSecureFlag::Flag *				pResetIndexBufferFlag
				);

			public:
				/// Provides the capability to set and clear the reset vertex and index buffers flags
				RoleSecureFlag::ISetFlag 
					& resetVertexBuffer, 
					& resetIndexBuffer;

				/** Begin producer access for preparing geometry batch data.
				@remarks Implies an existing lock and allocates a new BuilderQueue based on the specified LOD resolution and stitch configuration
				@param lock An existing exclusive lock
				@param pBuilderQueue A new instance of BuilderQueue will be constructed, this will be set with the pointer to that object
				@param pResolution Identifies the LOD of the batch operation
				@param pVertices Vertex information defining the entire mesh and all of its resolutions
				@param pIndices Index information defining all triangle list configurations of the entire mesh
				@param pBuffMan The buffer manager that knows how to clear the buffers
				@param enStitches Identifies the stitch configuration of the batch operation
				@param pSetResetVertexBufferFlag Access to a flag to set the hardware vertex buffer flag
				@param pResetVertexBufferFlag The opaque reset hardware vertex buffer flag
				@param pSetResetIndexBufferFlag Access to a flag to set the hardware index buffer flag
				@param pResetIndexBufferFlag The opaque reset hardware index buffer flag
				*/
				ProducerQueueAccess(
					boost::unique_lock< boost::shared_mutex > &&	lock, 
					BuilderQueue *&									pBuilderQueue, 
					LOD *											pResolution, 
					Vertices *										pVertices, 
					Indices *										pIndices, 
					IBufferManager *								pBuffMan, 
					const Touch3DFlags								enStitches, 
					RoleSecureFlag::ISetFlag *						pSetResetVertexBufferFlag, 
					RoleSecureFlag::Flag *							pResetVertexBufferFlag,
					RoleSecureFlag::ISetFlag *						pSetResetIndexBufferFlag, 
					RoleSecureFlag::Flag *							pResetIndexBufferFlag
				);
				ProducerQueueAccess(ProducerQueueAccess && move);
			};

			/** Used to read geometry without mutation, implies a read-lock */
			class ReadOnlyAccess
			{
			private:
				boost::shared_lock< boost::shared_mutex > _lock;

			public:
				/// The mesh operation to perform on the vertices and the associated vertex index LOD index
				const MeshOperation meshOp;

				/** Begin read-only access for reading resolution data.
				@remarks Implies an existing lock providing const-access to the specified resolution
				@param lock An existing read-lock
				@param pResolution The resolution to expose
				@param pVertices Vertex information defining the entire mesh and all of its resolutions
				@param pIndices Index information defining all triangle list configurations for the entire mesh
				@param pBuffMan The manager for handling the clearance of buffers
				*/
				ReadOnlyAccess(
					boost::shared_lock< boost::shared_mutex > &&		lock, 
					const LOD *											pResolution, 
					const Vertices *									pVertices, 
					const Indices *										pIndices, 
					IBufferManager *									pBuffMan
				);
				ReadOnlyAccess(ReadOnlyAccess && move);
			};

			/** Provides direct access to the shadow state by-passing the producer/consumer model.  No thread-locking model is applied */
			class DirectAccess : public StateAccess
			{
			private:
				/// Deletes and reconstructs the specified geometry batch data container object
				BuilderQueue *& reallocate(BuilderQueue *& pBuilderQueue, LOD * pResolution, Vertices * pVertices, Indices * pIndices, IBufferManager * pBuffMan, const Touch3DFlags enStitches);

			public:
				/** Begin direct access for interacting with the resolution data (read/write) without concurrency 
				@remarks The client for this API is responsible for clearing the shadow data in tandem with the actual hardware buffers
				@param pBuilderQueue A new instance of BuilderQueue will be constructed, this will be set with the pointer to that object
				@param pResolution Identifies the LOD of the batch operation
				@param pVertices Vertex information defining the entire mesh and all of its resolutions
				@param pIndices Index information defining all triangle list configurations for the entire mesh
				@param pBuffMan The buffer manager that knows how to clear the buffers
				@param enStitches Identifies the stitch configuration of the batch operation
				*/
				DirectAccess(
					BuilderQueue *&			pBuilderQueue, 
					LOD *					pResolution, 
					Vertices *				pVertices, 
					Indices *				pIndices, 
					IBufferManager *		pBuffMan, 
					const Touch3DFlags		enStitches
				);
				DirectAccess(DirectAccess && move);
				~DirectAccess();
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
			/// Requests read-only access on shadow data pertinent to the specified LOD
			ReadOnlyAccess requestReadOnlyAccess(const unsigned char nLOD);
			/// Requests direct access on the shadow data pertinent to the specified LOD and stitch configuration without locking or concurrency support
			DirectAccess requestDirectAccess(const unsigned char nLOD, const Touch3DFlags enStitches);
			/// Retrieve the LOD meta data container for the specified LOD ordinal (zero is highest resolution, >0 is lower resolution)
			LOD * getDirectAccess(const unsigned char nLOD) { return _vpResolutions[nLOD]; }

			/** Clears either the GPU, shadow, or both for all resolutions
 			 * @param depth Indicates how deep to clear */
			void clearBuffers (const BufferDepth depth = IBufferManager::BD_GPU);

		private:
			class ResetIndexBufferFlag : public RoleSecureFlag::Flag
			{
			public:
				template< typename INTERFACE > INTERFACE * queryInterface () { return RoleSecureFlag::Flag::queryInterface< INTERFACE > (); }
			};

			class ResetVertexBufferFlag : public RoleSecureFlag::Flag
			{
			private:
				RoleSecureFlag::ISetFlag & _fResetIndexBufferFlag;

			public:
				/// Initialize with an instance of ResetIndexBufferFlag
				ResetVertexBufferFlag(ResetIndexBufferFlag * pIdxBuffFlag);

				/// Override set flag implementation, this custom implementation also automatically sets the reset index buffer flag associated herein
				virtual bool operator ++ ();

				template< typename INTERFACE > INTERFACE * queryInterface () { return RoleSecureFlag::Flag::queryInterface< INTERFACE > (); }
			};

			/// Maximum quantity of detail levels
			const unsigned char _nCountResolutions;
			/// A shadow LOD meta data container object for each level of detail
			LOD ** const _vpResolutions;
			/// The information to the vertex stuff
			Vertices * _pVertices;
			/// Indices meta
			Indices * _pIndices;

			/// Implemented from IBufferManager, this implicitly clears vertex-specific data
			void clearVertices( const BufferDepth depth);

			/// Implemented from IBufferManager, this implicitly clears index-specific data
			void clearIndices( const BufferDepth depth);

			/// Clears your mom
			void clearYourMom( const BufferDepth depth );
		};
	}
}

#endif
