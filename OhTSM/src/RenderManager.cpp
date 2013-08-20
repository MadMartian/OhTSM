#include "pch.h"

#include "RenderManager.h"

#include "LODRenderable.h"

#include "DebugTools.h"

namespace Ogre
{
	bool RenderManager::renderableQueued( Renderable * pRenderable, uint8 nQID, ushort nPriority, Technique ** ppTech, RenderQueue * pQueue )
	{
		OHT_DBGTRACE("Renderable " << pRenderable << " queued, QID=" << (int)nQID << ", priority=" << nPriority);
		return true;
	}

	void RenderManager::DetermineRenderStateVisitor::visit( Renderable* r )
	{
		LODRenderable * pRend = dynamic_cast< LODRenderable * > (r);

		if (pRend != NULL)
		{
			flag &= pRend->determineRenderState();
			OHT_DBGTRACE("\tDetermine State for " << pRend);
		}
	}

	RenderManager::DetermineRenderStateVisitor::DetermineRenderStateVisitor() 
		: flag(true)
	{}

	void RenderManager::renderQueueStarted( uint8 nQID, const String & invocation, bool & bSkip )
	{
		OHT_DBGTRACE("START(RenderQueue[" << (int)nQID << "]) -> " << invocation);

		RenderQueueGroup * pGrp = _pScMgr->getRenderQueue() ->getQueueGroup(nQID);

		bool bFlag = true;

		RenderQueueGroup::PriorityMapIterator i = pGrp ->getIterator();
		DetermineRenderStateVisitor visitor;

		while (i.hasMoreElements())
		{
			RenderPriorityGroup * pPrGrp = i.getNext();
			pPrGrp->getSolidsBasic().acceptVisitor(&visitor, QueuedRenderableCollection::OM_SORT_ASCENDING);
		}

		_queues[nQID]._bRenderFlag = visitor.flag;
	}

	void RenderManager::renderQueueEnded( uint8 nQID, const String& invocation, bool & bRepeat )
	{
		OHT_DBGTRACE("END(RenderQueue[" << (int)nQID << "]) -> " << invocation);
	}

	RenderManager::RenderManager( SceneManager * pScMgr )
		: _pScMgr(pScMgr)
	{
		_pScMgr = pScMgr;
		_pScMgr->addRenderQueueListener(this);
		_pScMgr->getRenderQueue() ->setRenderableListener(this);
	}

	bool RenderManager::checkBuffers( const IndexData * pIdxData, const HardwareVertexBufferSharedPtr pHWVtxB, VertexDeclaration * pVtxDecl, const float fScale )
	{
		const VertexElement * pElemPos = pVtxDecl->findElementBySemantic(VES_POSITION);
		const size_t nVtxSize = pVtxDecl->getVertexSize(0);
		float * pf;
		const float fMaxDist = sqrtf(3*fScale*fScale)*4;

		unsigned char * pVtxB = (unsigned char *)pHWVtxB->lock(HardwareBuffer::HBL_READ_ONLY);
		uint16 * pIdxB = (uint16*)pIdxData->indexBuffer->lock(HardwareBuffer::HBL_READ_ONLY);
		bool bResult = true;

		for (size_t i = pIdxData->indexStart; i < pIdxData->indexCount; i += 3)
		{
			const uint16 
				a = pIdxB[i+0],
				b = pIdxB[i+1],
				c = pIdxB[i+2];

			Vector3 va;
			pElemPos->baseVertexPointerToElement(pVtxB + a * nVtxSize, &pf);
			va.x = *pf++;
			va.y = *pf++;
			va.z = *pf++;

			Vector3 vb;
			pElemPos->baseVertexPointerToElement(pVtxB + b * nVtxSize, &pf);
			vb.x = *pf++;
			vb.y = *pf++;
			vb.z = *pf++;

			Vector3 vc;
			pElemPos->baseVertexPointerToElement(pVtxB + c * nVtxSize, &pf);
			vc.x = *pf++;
			vc.y = *pf++;
			vc.z = *pf++;

			bResult &= (va.distance(vb) < fMaxDist && vb.distance(vc) < fMaxDist && vc.distance(va) < fMaxDist);
		}

		pIdxData->indexBuffer->unlock();
		pHWVtxB->unlock();

		return bResult;
	}

	RenderManager::QueueManager::QueueManager() 
		: _bRenderFlag(false)
	{

	}
}