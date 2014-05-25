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

#include "pch.h"

#include<strstream>

#include "IsoSurfaceRenderable.h"
#include "IsoSurfaceBuilder.h"
#include "MetaWorldFragment.h"
#include "Util.h"
#include "RenderManager.h"
#include "DebugTools.h"

namespace Ogre
{
	using namespace Voxel;
	using namespace HardwareShadow;

	Ogre::String IsoSurfaceRenderable::TYPE = "IsoSurface";

	IsoSurfaceRenderable::IsoSurfaceRenderable(RenderManager * pRendMan, VertexDeclaration * pVtxDecl, MetaFragment::Container * pMWF, const size_t nLODLevels, const Real fPixelError, const Ogre::String & sName /*= ""*/)
	:	DynamicRenderable(pVtxDecl, RenderOperation::OT_TRIANGLE_LIST, true, nLODLevels, fPixelError, sName), 
		_pRendMan(pRendMan), _pMWF(pMWF), _bbox(pMWF->acquireBasicInterface().block->getBoxSize()), _pShadow(new HardwareIsoVertexShadow(nLODLevels)), 
		_vtxdata0(pVtxDecl), _t3df0(T3DS_None), _t3df(T3DS_None), _lod0(-1), _lod(-1)
	{
#ifdef _DISPDBG
		_bBoxDisplay = false;
#endif
	}

	IsoSurfaceRenderable::~IsoSurfaceRenderable()
	{
		oht_assert_threadmodel(ThrMdl_Main);
		OHT_DBGTRACE("Delete " << this);
	}

#ifdef _DEBUG
#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceRenderable::populateBuffers( HardwareShadow::HardwareIsoVertexShadow::ConsumerLock::QueueAccess & queue )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		// TODO: Check return value of prepare vertex buffer call
		prepareVertexBuffer(queue.requiredVertexCount(), queue.resetHWBuffers);
		prepareIndexBuffer(queue.meshOp.resolution->lod, queue.stitches, queue.indexQueue.size());

		HardwareVertexBufferSharedPtr pVtxBuffer = getVertexData()->getVertexBuffer();
		HardwareIndexBufferSharedPtr pIdxBuffer = getIndexData(queue.meshOp.resolution->lod, queue.stitches)->getIndexData()->indexBuffer;

		const MetaVoxelFactory::VertexDeclarationElements * pVtxDeclElems = _pMWF->factory->getVertexDeclarationElements();
		const size_t nVertexByteSize = _pMWF->factory->getVertexSize();

		if (!queue.vertexQueue.empty())
		{
			unsigned char * pOffset = static_cast< unsigned char * > (
				pVtxBuffer->lock(
					queue.vertexBufferOffset() * nVertexByteSize,
					queue.vertexQueue.size() * nVertexByteSize,
					HardwareBuffer::HBL_DISCARD
				)
			);

			for (BuilderQueue::VertexElementList::const_iterator i = queue.vertexQueue.begin(); i != queue.vertexQueue.end(); ++i)
			{
				Real * pReal;

				pVtxDeclElems->position->baseVertexPointerToElement(pOffset, &pReal);
				*pReal++ = i->position.x;
				*pReal++ = i->position.y;
				*pReal++ = i->position.z;

				if (pVtxDeclElems->normal != NULL)
				{
					pVtxDeclElems->normal->baseVertexPointerToElement(pOffset, &pReal);
					*pReal++ = i->normal.x;
					*pReal++ = i->normal.y;
					*pReal++ = i->normal.z;
				}
				if (pVtxDeclElems->diffuse != NULL)
				{
					uint32 * uColour;
					pVtxDeclElems->diffuse->baseVertexPointerToElement(pOffset, &uColour);
					*uColour = i->colour;
				}
				if (pVtxDeclElems->texcoords != NULL)
				{
					pVtxDeclElems->texcoords->baseVertexPointerToElement(pOffset, &pReal);
					*pReal++ = i->texcoord.x;
					*pReal++ = i->texcoord.y;
				}
				pOffset += nVertexByteSize;
			}

			pVtxBuffer->unlock();
		}

		if (!queue.indexQueue.empty())
		{
			OgreAssert(pIdxBuffer != _idx0.getIndexData()->indexBuffer, "Overwriting index-buffer currently in-use");

			HWVertexIndex * pIndex = static_cast< HWVertexIndex * > (
				pIdxBuffer->lock(HardwareBuffer::HBL_DISCARD)
			);
			for (BuilderQueue::IndexList::const_iterator i = queue.indexQueue.begin(); i != queue.indexQueue.end(); ++i)
				*pIndex++ = *i;

			pIdxBuffer->unlock();
		}

		queue.consume();

	}

#ifdef _DEBUG
#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceRenderable::populateBuffers( IsoVertexElements * pVtxElems, HardwareShadow::HardwareIsoVertexShadow::DirectAccess & direct, const bool bResetHWBuffers, const size_t nNewVertexCount, const size_t nIndexCount )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		prepareVertexBuffer(nNewVertexCount + direct.meshOp.nextVertexIndex(), bResetHWBuffers);
		prepareIndexBuffer(direct.meshOp.resolution->lod, direct.stitches, nIndexCount);
		const MetaVoxelFactory::VertexDeclarationElements * const pVtxDeclElems = _pMWF->factory->getVertexDeclarationElements();
		HardwareVertexBufferSharedPtr pVtxBuffer = getVertexData()->getVertexBuffer();
		HardwareIndexBufferSharedPtr pIdxBuffer = getIndexData(direct.meshOp.resolution->lod, direct.stitches) ->getIndexData()->indexBuffer;

		const size_t nVertexByteSize = _pMWF->factory->getVertexSize();
		const float fVertScale =
			const_cast< const MetaFragment::Container * > (_pMWF)	// Enter "const" context
			->acquire< MetaFragment::Interfaces::const_Basic > ()	// Acquire basic concurrent access
			.block					// Retrieve the fragment's voxel cube region
			->meta					// Access the cube region descriptor
			.scale;					// Finally access the grid cell scale

		if (!pVtxElems->vertexShipment.empty())
		{
			unsigned char * pOffset = static_cast< unsigned char * > (
				pVtxBuffer->lock(
					direct.meshOp.nextVertexIndex() * nVertexByteSize,
					pVtxElems->vertexShipment.size() * nVertexByteSize,
					HardwareBuffer::HBL_DISCARD
				)
			);

			for (IsoVertexVector::const_iterator i = pVtxElems->vertexShipment.begin(); i != pVtxElems->vertexShipment.end(); ++i)
			{
				Real * pReal;

				const IsoFixVec3 & pt = pVtxElems->positions[*i];
				pVtxDeclElems->position->baseVertexPointerToElement(pOffset, &pReal);
				*pReal++ = Real(pt.x) * fVertScale;
				*pReal++ = Real(pt.y) * fVertScale;
				*pReal++ = Real(pt.z) * fVertScale;

				if (pVtxDeclElems->normal != NULL)
				{
					const Vector3 & n = pVtxElems->normals[*i];
					pVtxDeclElems->normal->baseVertexPointerToElement(pOffset, &pReal);
					*pReal++ = n.x;
					*pReal++ = n.y;
					*pReal++ = n.z;
				}
				if (pVtxDeclElems->diffuse != NULL)
				{
					uint32 * uColour;
					pVtxDeclElems->diffuse->baseVertexPointerToElement(pOffset, &uColour);
					Root::getSingleton().convertColourValue(pVtxElems->colours[*i], uColour);
				}
				if (pVtxDeclElems->texcoords != NULL)
				{
					pVtxDeclElems->texcoords->baseVertexPointerToElement(pOffset, &pReal);
					*pReal++ = pVtxElems->texcoords[*i][0];
					*pReal++ = pVtxElems->texcoords[*i][1];
				}
				pOffset += nVertexByteSize;
			}

			pVtxBuffer->unlock();
		}

		if (!pVtxElems->triangles.empty())
		{
			HWVertexIndex * pIndex = static_cast< HWVertexIndex * > (
				pIdxBuffer->lock(HardwareBuffer::HBL_DISCARD)
			);
			for (IsoTriangleVector::const_iterator i = pVtxElems->triangles.begin(); i != pVtxElems->triangles.end(); ++i)
			{
				*pIndex++ = pVtxElems->indices[i->vertices[0]];
				*pIndex++ = pVtxElems->indices[i->vertices[1]];
				*pIndex++ = pVtxElems->indices[i->vertices[2]];
			}

			pIdxBuffer->unlock();
		}

		direct.revmapIVI2HWVIQueue = pVtxElems->vertexShipment;
		pVtxElems->vertexShipment.clear();
	}

	void IsoSurfaceRenderable::deleteGeometry()
	{
		oht_assert_threadmodel(ThrMdl_Main);

		wipeBuffers();
		_pShadow->clearBuffers(IBufferManager::BD_Shadow); // TODO: Can potentially block
	}

	void /*SimpleRenderable::*/ IsoSurfaceRenderable::getRenderOperation( RenderOperation& op )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		DynamicRenderable::getRenderOperation(op);

		SurfaceVertexData * pVtxDataUse = _vtxdata0.isEmpty() ? NULL : &_vtxdata0;
		MeshData::Index * pIdxUse = _idx0.isEmpty() ? NULL : &_idx0;

		if ((_lod != _lod0 || _t3df != _t3df0) && _pRendMan->queue(getRenderQueueGroup()) ->isCurrentRenderStateAvailable() && isConfigurationBuilt(_lod, _t3df))
		{
			pVtxDataUse = getVertexData();
			pIdxUse = getIndexData(_lod, _t3df);

			_vtxdata0 = *pVtxDataUse;
			_idx0 = *pIdxUse;

			_lod0 = _lod;
			_t3df0 = _t3df;
		}

		if (pVtxDataUse != NULL)
		{
			op.vertexData->vertexBufferBinding->setBinding(0, pVtxDataUse->getVertexBuffer());
			op.vertexData->vertexCount = pVtxDataUse->getCount();
		}
		if (pIdxUse != NULL)
		{
			op.useIndexes = true;
			op.indexData = pIdxUse->getIndexData();
		} else
			op.useIndexes = false;

#if defined(_DISPDBG) && defined(_CHECK_HW_BUFFERS)
		if (pVtxDataUse != NULL && pIdxUse != NULL)
		{
			if (!_pRendMan->checkBuffers(op.indexData, pVtxDataUse->getVertexBuffer(), vertexDeclaration(), const_cast< const MetaFragment::Container * > (_pMWF)->acquire< MetaFragment::Interfaces::const_Basic > () .block->meta.scale * float(1 << _lod)))
			{
				if (!_bBoxDisplay)
				{
					OHTDD_Cube(const_cast< const MetaFragment::Container * > (_pMWF)->acquire< MetaFragment::Interfaces::const_Basic > ().block->getBoundingBox());
					_bBoxDisplay = true;
				}
			}
		}
#endif

		OgreAssert(op.indexData != NULL, "Index data was null");
	}

	bool IsoSurfaceRenderable::determineRenderState()
	{
		auto fragment = _pMWF->acquire< MetaFragment::Interfaces::Upgradable >();
		_lod = getEffectiveRenderLevel();
		_t3df = fragment.getNeighborFlags(_lod);

		MetaFragment::Interfaces::Upgraded upgraded = fragment.upgrade();

		if (upgraded.requestConfiguration(_lod, _t3df))
			return true;

		return _idx0.isEmpty() || _vtxdata0.isEmpty();
	}

	void IsoSurfaceRenderable::computeMinimumLevels2Distances( const Real & fErrorFactorSqr, Real * pfMinLev2DistSqr, const size_t nCount )
	{
		pfMinLev2DistSqr[0] = 0;
		const CubeDataRegion * pDG = _pMWF->acquireBasicInterface().block;
		for (size_t i = 1; i < nCount; ++i)
			pfMinLev2DistSqr[i] = fErrorFactorSqr * Math::Sqr(2*pDG->getGridScale()) * Real(1 << (i - 1));
	}

	void IsoSurfaceRenderable::wipeBuffers()
	{
		DynamicRenderable::wipeBuffers();
		_t3df0 = T3DS_None;
		_lod0 = -1;
		_pShadow->clearBuffers(); // TODO: Can potentially block
	}

	bool IsoSurfaceRenderable::prepareVertexBuffer( const size_t vertexCount, bool bClearIndicesToo )
	{
		_t3df0 = T3DS_None;
		_lod0 = -1;
		return DynamicRenderable::prepareVertexBuffer(vertexCount, bClearIndicesToo);
	}

	void IsoSurfaceRenderable::detachFromParent( void )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		DynamicRenderable::detachFromParent();
		// TODO: MetaWorldFragment::unlinkNeighbors();
	}

}/// namespace Ogre
