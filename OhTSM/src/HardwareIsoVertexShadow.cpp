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
#include "HardwareIsoVertexShadow.h"
#include "CubeDataRegionDescriptor.h"

namespace Ogre
{
	using namespace Voxel;

	namespace HardwareShadow
	{
		LOD::Stitch::Stitch( const OrthogonalNeighbor side ) 
			: side(side), shadowed(false), gpued(false)
		{}

		LOD::LOD( const unsigned nLOD ) 
			: lod(nLOD), shadowed(false), gpued(false)
		{
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
				stitches[s] = new Stitch(OrthogonalNeighbor(s));
		}

		LOD::~LOD()
		{
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
				delete stitches[s];
		}

		void MeshOperation::restoreHWIndices( HWVertexIndex * mapIVI2HWVI )
		{
			size_t nHardwareIndex = 0;
			for (IsoVertexVector::const_iterator i = vertices->revmapIVI2HWVI.begin(); i != vertices->revmapIVI2HWVI.end(); ++i)
				mapIVI2HWVI[*i] = nHardwareIndex++;
		}

		void HardwareIsoVertexShadow::StateAccess::updateHardwareState( const IsoVertexVector::const_iterator beginRevMapIVI2HWVI, const IsoVertexVector::const_iterator endRevMapIVI2HWVI )
		{
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
			{
				const Touch3DSide side = OrthogonalNeighbor_to_Touch3DSide[s];

				if (stitches & side)
					this->meshOp.resolution->stitches[s] ->gpued = true;
			}
			this->meshOp.resolution->gpued = true;

			// addHWIndices
			this->meshOp.vertices->revmapIVI2HWVI.insert(this->meshOp.vertices->revmapIVI2HWVI.end(), beginRevMapIVI2HWVI, endRevMapIVI2HWVI);
		}

		HardwareIsoVertexShadow::HardwareIsoVertexShadow( const unsigned nLODCount ) 
		: _vpResolutions( new LOD *[nLODCount]), _nCountResolutions(nLODCount), _pBuilderQueue(NULL)
		{
			for (unsigned i = 0; i < _nCountResolutions; ++i)
				_vpResolutions[i] = new LOD(i);

			_pVertices = new Vertices();
		}

		HardwareIsoVertexShadow::~HardwareIsoVertexShadow()
		{
			{ OGRE_LOCK_RW_MUTEX_WRITE(_mutex);
				 for (unsigned i = 0; i < _nCountResolutions; ++i)
					 delete _vpResolutions[i];
				 delete [] _vpResolutions;
				 delete _pBuilderQueue;
				 delete _pVertices;
			}
		}

		void HardwareIsoVertexShadow::clearBuffers(const IBufferManager::BufferDepth depth /*= IBufferManager::BD_GPU*/)
		{
			{ OGRE_LOCK_RW_MUTEX_WRITE(_mutex);
				switch (depth)
				{
				case IBufferManager::BD_GPU:
					delete _pBuilderQueue;
					_pBuilderQueue = NULL;
					// No break, continue to next clause
				case IBufferManager::BD_Shadow:
					clear(depth);
					break;  // Now break, we're done
				}
			}
		}

		HardwareIsoVertexShadow::ConsumerLock HardwareIsoVertexShadow::requestConsumerLock(const unsigned char nLOD, const Touch3DFlags enStitches)
		{
			return ConsumerLock (
				boost::shared_lock< boost::shared_mutex > (_mutex, boost::try_to_lock), 
				_pBuilderQueue, 
				_vpResolutions[nLOD], 
				_pVertices, 
				this, 
				_pBuilderQueue == NULL ? NULL :
					static_cast< ResetHWBuffersFlag * > (_pBuilderQueue->pResetHWBuffers) 
						->queryInterface< RoleSecureFlag::IClearFlag > (), 
				enStitches
			);
		}
 
		HardwareIsoVertexShadow::ProducerQueueAccess HardwareIsoVertexShadow::requestProducerQueue(const unsigned char nLOD, const Touch3DFlags enStitches)
		{
			ResetHWBuffersFlag * pFlag = new ResetHWBuffersFlag();
			return ProducerQueueAccess (
				boost::unique_lock< boost::shared_mutex > (_mutex),
				_pBuilderQueue, 
				_vpResolutions[nLOD], 
				_pVertices, 
				this, 
				enStitches,
				pFlag->queryInterface< RoleSecureFlag::ISetFlag > (),
				pFlag
			);
		}

		HardwareIsoVertexShadow::ReadOnlyAccess HardwareIsoVertexShadow::requestReadOnlyAccess( const unsigned char nLOD )
		{
			return ReadOnlyAccess (boost::shared_lock< boost::shared_mutex > (_mutex), _vpResolutions[nLOD], _pVertices, this);
		}

		HardwareIsoVertexShadow::DirectAccess HardwareIsoVertexShadow::requestDirectAccess( const unsigned char nLOD, const Touch3DFlags enStitches )
		{
			return DirectAccess(_pBuilderQueue, 
				_vpResolutions[nLOD], 
				_pVertices, 
				this, 
				enStitches
			);
		}

		void HardwareIsoVertexShadow::clear( const BufferDepth depth, signed nLOD /*= -1*/ )
		{
			if (depth == BD_GPU)
			{
				clear(BD_GPU);
			}
			else
			{
				clearLOD(depth, _vpResolutions[nLOD]);
				clearVerts(depth);
			}
		}

		void HardwareIsoVertexShadow::clear( const BufferDepth depth )
		{
			for (LOD ** ppLOD = _vpResolutions, ** ppX = &_vpResolutions[_nCountResolutions];
				ppLOD < ppX;
				++ppLOD
			)
			{
				clearLOD(depth, *ppLOD);
			}

			clearVerts(depth);
		}

		void HardwareIsoVertexShadow::clearLOD( const BufferDepth depth, LOD * resolution )
		{
			switch (depth)
			{
			case BD_Shadow:
				resolution->regCases.clear();
				resolution->borderIsoVertexProperties.clear();
				resolution->middleIsoVertexProperties.clear();
				resolution->shadowed = false;
				for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
				{
					resolution->stitches[s] ->shadowed = false;
					resolution->stitches[s] ->transCases.clear();
				}
				// Do not break, continue to BD_GPU clause:
			case BD_GPU:
				resolution->gpued = false;
				for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
				{
					resolution->stitches[s] ->gpued = false;
				}
				// TODO: Verify that this is safe, looks like HardwareIsoVertexShadow::clearLOD is always called in-tandem 
				// from the main thread with code that directly clears the hardware buffers.
				//_fResetHWFlag++;
				break;	// Now break, we're done
			}
		}

		void HardwareIsoVertexShadow::clearVerts( BufferDepth depth )
		{
			_pVertices->revmapIVI2HWVI.clear();
		}

		HardwareIsoVertexShadow::ConsumerLock::QueueAccess::~QueueAccess()
		{
			if (_bDeconstruct)
			{
				delete _pBuilderQueue;
				_pBuilderQueue = NULL;
			}
		}

		HardwareIsoVertexShadow::ConsumerLock::QueueAccess::QueueAccess (BuilderQueue *& pBuilderQueue, RoleSecureFlag::IClearFlag * pClearResetHWBuffersFlag) 
		:	ConcurrentProducerConsumerQueueBase(pBuilderQueue), 
			resetHWBuffers(*pClearResetHWBuffersFlag),
			_bDeconstruct(false) 
		{}

		HardwareIsoVertexShadow::ConsumerLock::QueueAccess::QueueAccess( QueueAccess && move ) 
		: ConcurrentProducerConsumerQueueBase(static_cast< QueueAccess && > (move)), _bDeconstruct(move._bDeconstruct), resetHWBuffers(move.resetHWBuffers)
		{
			move._bDeconstruct = false;
		}

		void HardwareIsoVertexShadow::ConsumerLock::QueueAccess::consume()
		{
			if (resetHWBuffers)
			{
				meshOp.clear( IBufferManager::BD_GPU );
				--resetHWBuffers;
			}
			this->updateHardwareState(revmapIVI2HWVIQueue.begin(), revmapIVI2HWVIQueue.end());
			_bDeconstruct = true;
		}

		HardwareIsoVertexShadow::ReadOnlyAccess::ReadOnlyAccess( boost::shared_lock< boost::shared_mutex > && lock, const LOD * pResolution, const Vertices * pVertices, IBufferManager * pBuffMan )
			:	_lock(static_cast< boost::shared_lock< boost::shared_mutex > && > (lock)),
				meshOp(
					const_cast< LOD * > (pResolution), 
					const_cast< Vertices * > (pVertices),
					pBuffMan
				)
		{}

		HardwareIsoVertexShadow::ReadOnlyAccess::ReadOnlyAccess( ReadOnlyAccess && move )
			: _lock(static_cast< boost::shared_lock< boost::shared_mutex > && > (move._lock)), meshOp(move.meshOp)
		{}

		HardwareIsoVertexShadow::ProducerQueueAccess::ProducerQueueAccess (
			boost::unique_lock< boost::shared_mutex > && lock, 
			BuilderQueue *& pBuilderQueue, 
			LOD * pResolution, Vertices * pVertices, IBufferManager * pBuffMan, 
			const Touch3DFlags enStitches,
			RoleSecureFlag::ISetFlag * pSetResetHWBuffersFlag,
			RoleSecureFlag::Flag * pResetHWBuffersFlag
		)
		:	ConcurrentProducerConsumerQueueBase(reallocate(pBuilderQueue, pResolution, pVertices, pBuffMan, enStitches, pResetHWBuffersFlag)), 
			resetHWBuffers(*pSetResetHWBuffersFlag),
			_lock(static_cast< boost::unique_lock<boost::shared_mutex> && > (lock))
		{}

		HardwareIsoVertexShadow::ProducerQueueAccess::ProducerQueueAccess( ProducerQueueAccess && move )
		: ConcurrentProducerConsumerQueueBase(static_cast< ProducerQueueAccess && > (move)), _lock(static_cast< boost::unique_lock<boost::shared_mutex> && > (move._lock)),
			resetHWBuffers(move.resetHWBuffers)
		{}

		BuilderQueue *& HardwareIsoVertexShadow::ProducerQueueAccess::reallocate( BuilderQueue *& pBuilderQueue, LOD * pResolution, Vertices * pVertices, IBufferManager * pBuffMan, const Touch3DFlags enStitches, RoleSecureFlag::Flag * pResetHWBuffersFlag )
		{
			delete pBuilderQueue;
			return pBuilderQueue = new BuilderQueue(pResolution, pVertices, pBuffMan, enStitches, pResetHWBuffersFlag);
		}

		BuilderQueue::VertexElement::VertexElement( 
			const Vector3 & vPos, 
			const Vector3 & vNorm, 
			const ColourValue & cColour, 
			const Vector2 & vTexCoord 
		) : position(vPos), normal(vNorm), texcoord(vTexCoord)
		{
			setColour(cColour);
		}

		void BuilderQueue::VertexElement::setColour( const ColourValue & cColour )
		{
			Root::getSingleton().convertColourValue(cColour, &colour);
		}

		HardwareIsoVertexShadow::StateAccess::StateAccess( BuilderQueue *& pBuilderQueue ) 
		:	_pBuilderQueue(pBuilderQueue), 
			meshOp(pBuilderQueue->meshOp),
			stitches(pBuilderQueue->stitches), 
			revmapIVI2HWVIQueue(pBuilderQueue->revmapIVI2HWVIQueue)
		{}

		HardwareIsoVertexShadow::StateAccess::StateAccess( StateAccess && move ) 
		:	_pBuilderQueue(move._pBuilderQueue),
			meshOp(move.meshOp),
			stitches(move.stitches), 
			revmapIVI2HWVIQueue(move.revmapIVI2HWVIQueue)
		{}

		HardwareIsoVertexShadow::ConcurrentProducerConsumerQueueBase::ConcurrentProducerConsumerQueueBase( BuilderQueue *& pBuilderQueue ) 
		:	StateAccess(pBuilderQueue),
			vertexQueue(pBuilderQueue->vertexQueue), indexQueue(pBuilderQueue->indexQueue)
		{}

		HardwareIsoVertexShadow::ConcurrentProducerConsumerQueueBase::ConcurrentProducerConsumerQueueBase( ConcurrentProducerConsumerQueueBase && move ) 
		:	StateAccess(move),
			vertexQueue(move.vertexQueue), indexQueue(move.indexQueue)
		{}

		HardwareIsoVertexShadow::ConsumerLock::ConsumerLock
		( 
			boost::shared_lock< boost::shared_mutex > && lock, 
			BuilderQueue *& pBuilderQueue, 
			LOD * pResolution, Vertices * pVertices,
			IBufferManager * pBuffMan, 
			RoleSecureFlag::IClearFlag * pClearResetHWBuffersFlag,
			const Touch3DFlags enStitches 
		) 
		:	_lock(static_cast< boost::shared_lock< boost::shared_mutex > && > (lock)), 
			_pBuilderQueue(pBuilderQueue), 
			_meshOp(pResolution, pVertices, pBuffMan),
			_enStitches(enStitches),
			_pClearResetHWBuffersFlag(pClearResetHWBuffersFlag)
		{}

		HardwareIsoVertexShadow::ConsumerLock::ConsumerLock( ConsumerLock && move ) 
		:	_pBuilderQueue(move._pBuilderQueue), 
			_meshOp(move._meshOp), 
			_enStitches(move._enStitches), 
			_lock(static_cast< boost::shared_lock< boost::shared_mutex > && > (move._lock)),
			_pClearResetHWBuffersFlag(move._pClearResetHWBuffersFlag)
		{}

		MeshOperation::MeshOperation( LOD * pResolution, Vertices * pVertices, IBufferManager * pBuffMan ) 
			: resolution(pResolution), vertices(pVertices), _pBuffMan(pBuffMan) {}

		void MeshOperation::clear( const IBufferManager::BufferDepth depth )
		{
			_pBuffMan->clear(depth, resolution->lod);
		}

		BuilderQueue::BuilderQueue( LOD * pResolution, Vertices * pVertexStuff, IBufferManager * pBuffMan, const Touch3DFlags enStitches, RoleSecureFlag::Flag * pResetHWBuffers )
			: meshOp(pResolution, pVertexStuff, pBuffMan), stitches(enStitches), pResetHWBuffers(pResetHWBuffers)
		{

		}

		BuilderQueue::~BuilderQueue()
		{
			delete pResetHWBuffers;
		}


		HardwareIsoVertexShadow::DirectAccess::DirectAccess( BuilderQueue *& pBuilderQueue, LOD * pResolution, Vertices * pVertices, IBufferManager * pBuffMan, const Touch3DFlags enStitches ) 
			: StateAccess(reallocate(pBuilderQueue, pResolution, pVertices, pBuffMan, enStitches, new RoleSecureFlag::Flag))
		{

		}

		HardwareIsoVertexShadow::DirectAccess::DirectAccess( DirectAccess && move ) 
			: StateAccess(static_cast< StateAccess && > (move))
		{

		}

		BuilderQueue *& HardwareIsoVertexShadow::DirectAccess::reallocate( BuilderQueue *& pBuilderQueue, LOD * pResolution, Vertices * pVertices, IBufferManager * pBuffMan, const Touch3DFlags enStitches, RoleSecureFlag::Flag * pResetHWBuffersFlag )
		{
			delete pBuilderQueue;
			return pBuilderQueue = new BuilderQueue(pResolution, pVertices, pBuffMan, enStitches, pResetHWBuffersFlag);
		}

		HardwareIsoVertexShadow::DirectAccess::~DirectAccess()
		{
			this->updateHardwareState(revmapIVI2HWVIQueue.begin(), revmapIVI2HWVIQueue.end());
		}

	}

}
