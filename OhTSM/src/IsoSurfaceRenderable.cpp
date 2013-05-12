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
#include "DebugTools.h"

namespace Ogre
{
	using namespace Voxel;
	using namespace HardwareShadow;

	Ogre::String IsoSurfaceRenderable::TYPE = "IsoSurface";

	IsoSurfaceRenderable::IsoSurfaceRenderable(VertexDeclaration * pVtxDecl, MetaFragment::Container * pMWF, const size_t nLODLevels, const Real fPixelError, const Ogre::String & sName /*= ""*/)
	:	DynamicRenderable(pVtxDecl, RenderOperation::OT_TRIANGLE_LIST, true, nLODLevels, fPixelError, sName), 
		_pMWF(pMWF), _bbox(pMWF->acquireBasicInterface().block->getBoxSize()), _pShadow(new HardwareIsoVertexShadow(nLODLevels)), _t3df0(T3DS_None), _lod0(-1), _pIdxData0(NULL)
	{
	}

	IsoSurfaceRenderable::~IsoSurfaceRenderable()
	{
		oht_assert_threadmodel(ThrMdl_Main);
		OHT_DBGTRACE("Delete " << this);
	}

	void IsoSurfaceRenderable::populateBuffers( const IsoSurfaceBuilder * pBuilder, HardwareShadow::HardwareIsoVertexShadow::ConsumerLock::QueueAccess & queue )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		prepareVertexBuffer(queue.resolution->lod, queue.getRequiredHardwareVertexCount(), queue.resolution->isResetHWBuffers());
		queue.resolution->clearResetHWBuffersFlag();
		prepareIndexBuffer(queue.resolution->lod, queue.stitches, queue.indexQueue.size());
		auto pair = getMeshData(queue.resolution->lod, queue.stitches);		
		pBuilder->populateHardwareBuffers(pair.first->getVertexBuffer(), pair.second->getIndexData()->indexBuffer, queue);
	}

	void IsoSurfaceRenderable::directlyPopulateBuffers( const IsoSurfaceBuilder * pBuilder, HardwareShadow::LOD * pResolution, const Touch3DFlags enStitches, const size_t nNewVertexCount, const size_t nIndexCount )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		prepareVertexBuffer(pResolution->lod, nNewVertexCount + pResolution->getHWIndexBufferTail(), pResolution->isResetHWBuffers());
		pResolution->clearResetHWBuffersFlag();
		prepareIndexBuffer(pResolution->lod, enStitches, nIndexCount);
		auto pair = getMeshData(pResolution->lod, enStitches);		
		pBuilder->directlyPopulateHardwareBuffers(pair.first->getVertexBuffer(), pair.second->getIndexData()->indexBuffer, pResolution);
	}

	void IsoSurfaceRenderable::deleteGeometry()
	{
		oht_assert_threadmodel(ThrMdl_Main);

		wipeBuffers();
		_pShadow->clear(); // TODO: Can potentially block
	}

	void /*SimpleRenderable::*/ IsoSurfaceRenderable::getRenderOperation( RenderOperation& op )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		DynamicRenderable::getRenderOperation(op);

		const int nLOD = getEffectiveRenderLevel();
		auto fragment = _pMWF->acquire< MetaFragment::Interfaces::Upgradable >();
		Touch3DFlags t3dTransition = fragment.getNeighborFlags(nLOD);

		if (_pIdxData0 != NULL)
			op.indexData = _pIdxData0;

		if (nLOD != _lod0 || t3dTransition != _t3df0)
		{
			MetaFragment::Interfaces::Upgraded upgraded = fragment.upgrade();

			if (upgraded.generateConfiguration(nLOD, t3dTransition))
			{
				_lod0 = nLOD;
				_t3df0 = t3dTransition;
				_pIdxData0 = 
					op.indexData = getMeshData(nLOD, t3dTransition).second->getIndexData();

				MeshData::VertexData * pVtxData = getVertexData(nLOD);
				op.vertexData->vertexBufferBinding->setBinding(0, pVtxData->getVertexBuffer());
				op.vertexData->vertexCount = pVtxData->getCount();
			}
		}

		OgreAssert(op.indexData != NULL, "Index data was null");
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
		_pIdxData0 = NULL;
		_pShadow->reset(); // TODO: Can potentially block
	}

	bool IsoSurfaceRenderable::prepareVertexBuffer( const unsigned nLOD, const size_t vertexCount, bool bClearIndicesToo )
	{
		_t3df0 = T3DS_None;
		_lod0 = -1;
		_pIdxData0 = NULL;
		return DynamicRenderable::prepareVertexBuffer(nLOD, vertexCount, bClearIndicesToo);
	}

	void IsoSurfaceRenderable::detachFromParent( void )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		DynamicRenderable::detachFromParent();
		// TODO: MetaWorldFragment::unlinkNeighbors();
	}

}/// namespace Ogre
