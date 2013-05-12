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
			: lod(nLOD), shadowed(false), gpued(false), _bRollbackHWBuffers(false)
		{
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
				stitches[s] = new Stitch(OrthogonalNeighbor(s));
		}

		LOD::~LOD()
		{
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
				delete stitches[s];
		}

		void LOD::clearHardwareState()
		{
			gpued = false;
			_bRollbackHWBuffers = true;
			_revmapIVI2HWVI.clear();
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
				stitches[s] ->gpued = false;
		}

		void LOD::clearAll()
		{
			regCases.clear();
			borderIsoVertexProperties.clear();
			middleIsoVertexProperties.clear();
			_revmapIVI2HWVI.clear();

			_bRollbackHWBuffers = true;
			gpued =
			shadowed = false;
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
			{
				stitches[s] ->gpued = 
				stitches[s] ->shadowed = false;
				stitches[s] ->transCases.clear();
			}
		}

		void LOD::restoreHWIndices( HWVertexIndex * mapIVI2HWVI )
		{
			size_t nHardwareIndex = 0;
			for (IsoVertexVector::const_iterator i = _revmapIVI2HWVI.begin(); i != _revmapIVI2HWVI.end(); ++i)
				mapIVI2HWVI[*i] = nHardwareIndex++;
		}

		void LOD::addHWIndices( const IsoVertexVector::const_iterator begin, const IsoVertexVector::const_iterator end )
		{
			_revmapIVI2HWVI.insert(_revmapIVI2HWVI.end(), begin, end);
		}

		void LOD::updateHardwareState( const Touch3DFlags enStitches, const IsoVertexVector::const_iterator beginRevMapIVI2HWVI, const IsoVertexVector::const_iterator endRevMapIVI2HWVI )
		{
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
			{
				const Touch3DSide side = OrthogonalNeighbor_to_Touch3DSide[s];

				if (enStitches & side)
					stitches[s] ->gpued = true;
			}
			gpued = true;
			addHWIndices(beginRevMapIVI2HWVI, endRevMapIVI2HWVI);
		}

		HardwareIsoVertexShadow::HardwareIsoVertexShadow( const unsigned nLODCount ) 
		: _vpResolutions( new LOD *[nLODCount]), _nCountResolutions(nLODCount), _pBuilderQueue(NULL)
		{
			for (unsigned i = 0; i < _nCountResolutions; ++i)
				_vpResolutions[i] = new LOD(i);
		}

		HardwareIsoVertexShadow::~HardwareIsoVertexShadow()
		{
			{ OGRE_LOCK_MUTEX(_mutex);
				 for (unsigned i = 0; i < _nCountResolutions; ++i)
					 delete _vpResolutions[i];
				 delete [] _vpResolutions;
				 delete _pBuilderQueue;
			}
		}

		void HardwareIsoVertexShadow::reset()
		{
			{ OGRE_LOCK_MUTEX(_mutex);
				delete _pBuilderQueue;
				_pBuilderQueue = NULL;
				for (unsigned i = 0; i < _nCountResolutions; ++i)
					_vpResolutions[i] ->clearHardwareState();
			}
		}

		void HardwareIsoVertexShadow::clear()
		{
			{ OGRE_LOCK_MUTEX(_mutex);
				delete _pBuilderQueue;
				_pBuilderQueue = NULL;
				for (unsigned i = 0; i < _nCountResolutions; ++i)
					_vpResolutions[i] ->clearAll();
			}
		}

		HardwareIsoVertexShadow::ConsumerLock HardwareIsoVertexShadow::requestConsumerLock(const unsigned char nLOD, const Touch3DFlags enStitches)
		{
			return ConsumerLock (boost::recursive_mutex::scoped_try_lock(_mutex), _pBuilderQueue, _vpResolutions[nLOD], enStitches);
		}
 
		HardwareIsoVertexShadow::ProducerQueueAccess HardwareIsoVertexShadow::requestProducerQueue(const unsigned char nLOD, const Touch3DFlags enStitches)
		{
			return ProducerQueueAccess (boost::recursive_mutex::scoped_lock(_mutex), _pBuilderQueue, _vpResolutions[nLOD], enStitches);
		}

		HardwareIsoVertexShadow::ConsumerLock::QueueAccess::~QueueAccess()
		{
			if (_bDeconstruct)
			{
				delete _pBuilderQueue;
				_pBuilderQueue = NULL;
			}
		}

		HardwareIsoVertexShadow::ConsumerLock::QueueAccess::QueueAccess (BuilderQueue *& pBuilderQueue) 
		:	ConcurrentProducerConsumerQueueBase(pBuilderQueue), 
			_bDeconstruct(false) 
		{}

		HardwareIsoVertexShadow::ConsumerLock::QueueAccess::QueueAccess( QueueAccess && move ) 
		: ConcurrentProducerConsumerQueueBase(static_cast< QueueAccess && > (move)), _bDeconstruct(move._bDeconstruct)
		{
			move._bDeconstruct = false;
		}

		void HardwareIsoVertexShadow::ConsumerLock::QueueAccess::consume()
		{
			resolution->updateHardwareState(stitches, revmapIVI2HWVIQueue.begin(), revmapIVI2HWVIQueue.end());
			_bDeconstruct = true;
		}

		HardwareIsoVertexShadow::ProducerQueueAccess::ProducerQueueAccess (
			boost::recursive_mutex::scoped_lock && lock, 
			BuilderQueue *& pBuilderQueue, 
			LOD * pResolution, const Touch3DFlags enStitches
		)
		:	ConcurrentProducerConsumerQueueBase(reallocate(pBuilderQueue, pResolution, enStitches)), 
			_lock(static_cast< boost::recursive_mutex::scoped_lock && > (lock))
		{}

		HardwareIsoVertexShadow::ProducerQueueAccess::ProducerQueueAccess( ProducerQueueAccess && move )
		: ConcurrentProducerConsumerQueueBase(static_cast< ProducerQueueAccess && > (move)), _lock(static_cast< boost::recursive_mutex::scoped_lock && > (move._lock)) {}

		BuilderQueue *& HardwareIsoVertexShadow::ProducerQueueAccess::reallocate( BuilderQueue *& pBuilderQueue, LOD * pResolution, const Touch3DFlags enStitches )
		{
			delete pBuilderQueue;
			return pBuilderQueue = new BuilderQueue(pResolution, enStitches);
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

		HardwareIsoVertexShadow::ConcurrentProducerConsumerQueueBase::ConcurrentProducerConsumerQueueBase( BuilderQueue *& pBuilderQueue ) 
		:	_pBuilderQueue(pBuilderQueue), 
			resolution(pBuilderQueue->resolution), stitches(pBuilderQueue->stitches), 
			vertexQueue(pBuilderQueue->vertexQueue), indexQueue(pBuilderQueue->indexQueue),
			revmapIVI2HWVIQueue(pBuilderQueue->revmapIVI2HWVIQueue)
		{}

		HardwareIsoVertexShadow::ConcurrentProducerConsumerQueueBase::ConcurrentProducerConsumerQueueBase( ConcurrentProducerConsumerQueueBase && move ) 
		:	_pBuilderQueue(move._pBuilderQueue),
			resolution(move.resolution), stitches(move.stitches), 
			vertexQueue(move.vertexQueue), indexQueue(move.indexQueue),
			revmapIVI2HWVIQueue(move.revmapIVI2HWVIQueue)
		{}


		HardwareIsoVertexShadow::ConsumerLock::ConsumerLock
		( 
			boost::recursive_mutex::scoped_try_lock && lock, 
			BuilderQueue *& pBuilderQueue, 
			LOD * pResolution, const Touch3DFlags enStitches 
		) 
		:	_lock(static_cast< boost::recursive_mutex::scoped_try_lock && > (lock)), 
			_pBuilderQueue(pBuilderQueue), 
			_pResolution(pResolution), _enStitches(enStitches)
		{}

		HardwareIsoVertexShadow::ConsumerLock::ConsumerLock( ConsumerLock && move ) 
		:	_pBuilderQueue(move._pBuilderQueue), 
			_pResolution(move._pResolution), _enStitches(move._enStitches), 
			_lock(static_cast< boost::recursive_mutex::scoped_try_lock && > (move._lock))
		{}

	}

}
