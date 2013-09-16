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

#include <OgreDataStream.h>

#include "OverhangTerrainGroup.h"
#include "MetaWorldFragment.h"
#include "MetaBall.h"
#include "DebugTools.h"
#include "IsoVertexElements.h"
#include "IsoSurfaceRenderable.h"
#include "PageSection.h"
#include "IsoSurfaceBuilder.h"

#define NCELLS 64
#define SCALE 2.9296875

namespace Ogre
{
	const uint32 OverhangTerrainGroup::CHUNK_ID = StreamSerialiser::makeIdentifier("OHTG");
	const uint16 OverhangTerrainGroup::CHUNK_VERSION = 1;
	const uint32 OverhangTerrainGroup::CHUNK_PAGE_ID = StreamSerialiser::makeIdentifier("TGPG");
	const uint16 OverhangTerrainGroup::CHUNK_PAGE_VERSION = 1;

	OverhangTerrainGroup::OverhangTerrainGroup( 
		OverhangTerrainSceneManager * sm, 
		ManualResourceLoader * pManRsrcLoader /*= NULL*/,
		const String & sResourceGroup /*= ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME*/
	) 
	:	OverhangTerrainManager(sm->getOptions(), sm),
		_ptOrigin(Vector3::ZERO), 
		_descchan(sm->getOptions().channels.descriptor),
		_sResourceGroup(sResourceGroup),
		_factory(sm->getRenderManager(), sm->getOptions(), pManRsrcLoader),
		_chanprops(sm->getOptions().channels.descriptor)
	{
		WorkQueue * wq = Root::getSingleton().getWorkQueue();
		_nWorkQChannel = wq->getChannel("Ogre/OverhangTerrainGroup");
		wq->addRequestHandler(_nWorkQChannel, this);
		wq->addResponseHandler(_nWorkQChannel, this);

		// Used for drawing debugging information to the display
		OHTDD_Init(sm);

		Vector3 vi = toSpace(OCS_Terrain, OCS_PagingStrategy, Vector3::UNIT_SCALE * 1.1);
		_pxi = static_cast< int16 > (vi.x);
		_pyi = static_cast< int16 > (vi.y);

		const Channel::Index< OverhangTerrainOptions::ChannelOptions > & channels = sm->getOptions().channels;
		for (Channel::Index< OverhangTerrainOptions::ChannelOptions >::const_iterator i = channels.begin(); i != channels.end(); ++i)
		{
			setMaterial(i->channel, i->value->material);
			setRenderQueueGroup(i->channel, i->value->qid);
		}
	}

	OverhangTerrainGroup::~OverhangTerrainGroup()
	{
		OHTDD_Destroy();

		WorkQueue* wq = Root::getSingleton().getWorkQueue();
		wq->abortRequestsByChannel(_nWorkQChannel);

		wq->removeRequestHandler(_nWorkQChannel, this);
		wq->removeResponseHandler(_nWorkQChannel, this);
	}

	PageSection * OverhangTerrainGroup::createPage( OverhangTerrainSlot * pSlot )
	{
		return OGRE_NEW PageSection(this, pSlot, &_factory, _descchan);
	}

	OverhangTerrainSlot* OverhangTerrainGroup::getTerrainSlot(const int16 x, const int16 y, bool createIfMissing)
	{
		PageID key = calculatePageID(x, y);
		TerrainSlotMap::iterator i = _slots.find(key);
		if (i != _slots.end())
			return i->second;
		else if (createIfMissing)
		{
			OverhangTerrainSlot* slot = new OverhangTerrainSlot(this, x, y);
			_slots[key] = slot;
			return slot;
		}
		return 0;
	}
	//---------------------------------------------------------------------
	OverhangTerrainSlot* OverhangTerrainGroup::getTerrainSlot(const int16 x, const int16 y) const
	{
		PageID key = calculatePageID(x, y);
		TerrainSlotMap::const_iterator i = _slots.find(key);
		if (i != _slots.end())
			return i->second;
		else
			return NULL;
	}
	bool OverhangTerrainGroup::defineTerrain( const int16 x, const int16 y, const bool bLoad /*= true*/, bool synchronous /*= false*/ )
	{
		oht_assert_threadmodel(ThrMdl_Main);
		OverhangTerrainSlot * slot = getTerrainSlot(x, y, true);

		if (slot->state() != OverhangTerrainSlot::TSS_Empty || !bLoad)
			return true;

		if (tryLockNeighborhood(slot))
		{
			OHT_DBGTRACE("LOAD " << slot->x << 'x' << slot->y);

			slot->instance = createPage(slot);
			slot->data = new OverhangTerrainSlot::LoadData(options, x, y);
			slot->loading();

			WorkRequest req;
			req.slot = slot;
			req.origin = this;

			Root::getSingleton().getWorkQueue()->addRequest(
				_nWorkQChannel, static_cast <uint16> (LoadPage), 
				Any(req), 0, synchronous);

			return true;
		} else
			return false;
	}

	// THREAD: Worker
	bool OverhangTerrainGroup::defineTerrain_worker( OverhangTerrainSlot * slot )
	{
		bool bStatus = false;

		OgreAssert(slot->data != NULL, "This must be empty");

		// If the page provider does not agree to handle all page loading concerns then default behavior below ensues and anything already 
		// loaded by the page provider is ignored and discarded.
		if (_pPageProvider == NULL || !_pPageProvider->loadPage(slot->x, slot->y, &slot->data->params, slot->instance))
		{
			DataStreamPtr pin = acquirePageStream(slot->x, slot->y);

			if (pin->isReadable())
			{
				StreamSerialiser in (pin);

				if (!in.readChunkBegin(CHUNK_PAGE_ID, CHUNK_PAGE_VERSION))
					OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND, "Stream does not contain OverhangTerrainGroup page data", "LoadPage");

				slot->data->params << in;					
				*slot->instance << slot->data->params;

				bool bHasPage;
				in.read(&bHasPage);

				if (bHasPage)
					*slot->instance << in;

				in.readChunkEnd(CHUNK_PAGE_ID);

				bStatus = true;
			}
			pin->close();

			slot->instance->conjoin();
		}

		return bStatus;
	}

	void OverhangTerrainGroup::defineTerrain_response( OverhangTerrainSlot* slot )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		OHT_DBGTRACE("LOADED " << slot->x << 'x' << slot->y);
		if (slot->instance != NULL)
		{
			assert(slot->data != NULL);
			if (_pPageProvider != NULL)
				_pPageProvider->preparePage(slot->x, slot->y, slot->instance);
			linkPageNeighbors(slot->x, slot->y, slot->instance);
			preparePage(slot->instance, slot->data);
		}

		slot->freeLoadData();

		unlockNeighborhood(slot);
		slot->doneLoading();

		if (slot->instance != NULL)
		{
			getSceneManager()->attachPage(slot->instance);
			slot->position = computeTerrainSlotPosition(slot->x, slot->y);
			slot->instance->setPosition(slot->position);
		}
	}

	void OverhangTerrainGroup::saveGroupDefinition( StreamSerialiser& stream )
	{
		getSceneManager()->getOptions() >> stream;

		stream.writeChunkBegin(CHUNK_ID, CHUNK_VERSION);
		stream.write(&_ptOrigin);
		stream.writeChunkEnd(CHUNK_ID);
	}

	void OverhangTerrainGroup::loadGroupDefinition( StreamSerialiser& stream )
	{
		OverhangTerrainOptions opts;

		opts << stream;
		getSceneManager()->setOptions(opts);

		if (!stream.readChunkBegin(CHUNK_ID, CHUNK_VERSION))
			OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND, "Stream does not contain OverhangTerrainGroup data", "loadGroupDefinition");

		stream.read(&_ptOrigin);

		stream.readChunkEnd(CHUNK_ID);

		// TODO: Re-initialize?
	}

	// THREAD: *Main
	bool OverhangTerrainGroup::unloadTerrain( const int16 x, const int16 y, const bool synchronous /*= false*/ )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		OverhangTerrainSlot * pSlot = getTerrainSlot(x, y);
		if (pSlot == NULL)
			return true;

		if (pSlot->canUnload())
		{
			// State was Neutral but page instance was NULL
			if (pSlot->instance->isDirty() && options.autoSave && pSlot->canSave()) // TODO: Synchronize PageSection::isDirty()
			{
				pSlot->saveUnload();
				saveTerrainImpl(pSlot, synchronous);
			}
			else if (tryLockNeighborhood(pSlot))
			{
				pSlot->unloading();
				unloadTerrainImpl(pSlot, synchronous);
			}
			return true;
		} else
			return false;
	}

	void OverhangTerrainGroup::unloadTerrainImpl( OverhangTerrainSlot * pSlot, const bool synchronous /*= false */ )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		OHT_DBGTRACE("Unload terrain " << pSlot->x << 'x' << pSlot->y);

		pSlot->instance->detachFromScene();

		if (_pPageProvider != NULL)
			_pPageProvider->detachPage(pSlot->x, pSlot->y, pSlot->instance);

		pSlot->instance->unlinkPageNeighbors();

		WorkRequest req;
		req.slot = pSlot;
		req.origin = this;

		Root::getSingleton().getWorkQueue()->addRequest(
			_nWorkQChannel, static_cast <uint16> (UnloadPage), 
			Any(req), 0, synchronous);
	}

	void OverhangTerrainGroup::unloadTerrain_worker( OverhangTerrainSlot * slot, UnloadPageResponseData & respdata )
	{
		OHT_DBGTRACE("Unload terrain " << slot->x << 'x' << slot->y);

		if (_pPageProvider != NULL)
			_pPageProvider->unloadPage(slot->x, slot->y);

		for (Channel::Descriptor::iterator j = _descchan.begin(); j != _descchan.end(); ++j)
			for (MetaFragmentIterator i = slot->instance->iterateMetaFrags(*j); i; ++i)
				respdata.dispose->push_back(i->acquireBasicInterface().surface);
	}

	void OverhangTerrainGroup::unloadTerrain_response( OverhangTerrainSlot * pSlot, UnloadPageResponseData &data )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		unlockNeighborhood(pSlot);
		
		for (std::vector< Renderable * >::iterator i = data.dispose->begin(); i != data.dispose->end(); ++i)
			delete *i;

		pSlot->doneUnloading();

		TerrainSlotMap::iterator i = _slots.find(calculatePageID(pSlot->x, pSlot->y));

		OgreAssert(i->second == pSlot, "Sought slot did not match argument");
		_slots.erase(i);
		OGRE_DELETE pSlot;
	}

	bool OverhangTerrainGroup::canHandleRequest( const WorkQueue::Request* req, const WorkQueue* srcQ )
	{
		OverhangTerrainGroup * pOrigin;

		switch (req->getType())
		{
		case AddMetaObject:
			pOrigin = any_cast< MetaBallWorkRequest > (req->getData()) .origin;
			break;
		case BuildSurface:
			pOrigin = any_cast< SurfaceGenRequest > (req->getData()).origin;
			break;
		default:
			pOrigin = any_cast< WorkRequest > (req->getData()) .origin;
			break;
		}
		
		// only deal with own requests
		if (pOrigin != this)
			return false;
		else
			return RequestHandler::canHandleRequest(req, srcQ);
	}

	WorkQueue::Response* OverhangTerrainGroup::handleRequest( const WorkQueue::Request* req, const WorkQueue* srcQ )
	{
		WorkQueue::Response* response = NULL;

		try
		{
			switch (req->getType())
			{
			case UnloadPage:
				{
					WorkRequest lreq = any_cast<WorkRequest>(req->getData());
					UnloadPageResponseData respdata;
					respdata.dispose = new std::vector< Renderable * > ();
					unloadTerrain_worker(lreq.slot, respdata);
					response = new WorkQueue::Response(req, true, Any(respdata));
				}
				break;

			case LoadPage:
				{
					WorkRequest lreq = any_cast<WorkRequest>(req->getData());
					defineTerrain_worker(lreq.slot);
					response = new WorkQueue::Response(req, true, Any());
				}
				break;

			case SavePage:
				{
					WorkRequest lreq = any_cast<WorkRequest>(req->getData());
					saveTerrain_worker(lreq.slot);
					response = new WorkQueue::Response(req, true, Any());
				}
				break;

			case AddMetaObject:
				{
					MetaBallWorkRequest lreq = any_cast<MetaBallWorkRequest>(req->getData());

					addMetaBall_worker(
						lreq.slots.begin(),
						lreq.slots.end(),
						Vector3(
							lreq.position.x, 
							lreq.position.y, 
							lreq.position.z
						), 
						lreq.radius, 
						lreq.excavating
					);
					response = new WorkQueue::Response(req, true, Any());
				}
				break;
			case BuildSurface:
				{
					using namespace HardwareShadow;

					SurfaceGenRequest reqdata = any_cast<SurfaceGenRequest>(req->getData());
					HardwareIsoVertexShadow::ProducerQueueAccess queue = reqdata.shadow->requestProducerQueue(reqdata.lod, reqdata.stitches);
					IsoSurfaceBuilder * pISB = _factory.getIsoSurfaceBuilder();
					auto fragment = reqdata.mf->acquire< MetaFragment::Interfaces::const_Basic >();

					pISB->queueBuild(reqdata.mf, reqdata.shadow, reqdata.channel, reqdata.lod, reqdata.surfaceFlags, reqdata.stitches, reqdata.vertexBufferCapacity);
					response  = new WorkQueue::Response(req, true, Any());				
				}
				break;
			}
		} catch (Exception& e)
		{
			// oops
			response = new WorkQueue::Response(req, false, Any(), 
				e.getFullDescription());
		}

		return response;
	}

	bool OverhangTerrainGroup::canHandleResponse( const WorkQueue::Response* res, const WorkQueue* srcQ )
	{
		OverhangTerrainGroup * pOrigin;

		switch (res->getRequest()->getType())
		{
		case AddMetaObject:
			pOrigin = any_cast< MetaBallWorkRequest > (res->getRequest()->getData()) .origin;
			break;
		default:
			pOrigin = any_cast< WorkRequest > (res->getRequest()->getData()) .origin;
			break;
		}

		// only deal with own requests
		if (pOrigin != this)
			return false;
		else
			return ResponseHandler::canHandleResponse(res, srcQ);
	}

	void OverhangTerrainGroup::handleResponse( const WorkQueue::Response* res, const WorkQueue* srcQ )
	{
		switch (res->getRequest()->getType())
		{
		case LoadPage:
			{
				WorkRequest lreq = any_cast<WorkRequest>(res->getRequest()->getData());
				if (res->succeeded())
					defineTerrain_response(lreq.slot);
				else
					LogManager::getSingleton().stream(LML_CRITICAL) <<
					"We failed to prepare the terrain at (" << lreq.slot->x << ", " <<
					lreq.slot->y <<") with the error '" << res->getMessages() << "'";
				lreq.slot->freeLoadData();
			}
			break;
		case UnloadPage:
			{
				WorkRequest lreq = any_cast<WorkRequest>(res->getRequest()->getData());
				UnloadPageResponseData data = any_cast< UnloadPageResponseData > (res->getData());
				if (res->succeeded())
					unloadTerrain_response(lreq.slot, data);
				else
					LogManager::getSingleton().stream(LML_CRITICAL) <<
						"We failed to unload terrain at (" << lreq.slot->x << ", " <<
						lreq.slot->y <<") with the error '" << res->getMessages() << "'";

				delete data.dispose;
				data.dispose = NULL;
			}
			break;
		case SavePage:
			{
				WorkRequest lreq = any_cast<WorkRequest>(res->getRequest()->getData());

				if (res->succeeded())
					saveTerrain_response(lreq.slot);
				else
					LogManager::getSingleton().stream(LML_CRITICAL) <<
					"We failed to save terrain at (" << lreq.slot->x << ", " <<
					lreq.slot->y <<") with the error '" << res->getMessages() << "'";

			}
			break;
		case AddMetaObject:
			{
				MetaBallWorkRequest lreq = any_cast<MetaBallWorkRequest>(res->getRequest()->getData());
				addMetaBall_response(lreq.slots.begin(), lreq.slots.end());
			}
			break;
		case BuildSurface:
			{
				SurfaceGenRequest lreq = any_cast<SurfaceGenRequest>(res->getRequest()->getData());
				lreq.slot->doneQuery();
			}
			break;
		case DestroyAll:
			clear_response();
			break;
		}
	}

	// THREAD: *Main
	bool OverhangTerrainGroup::saveTerrain( OverhangTerrainSlot * pSlot, const bool synchronous /*= false*/ )
	{
		// Enforce a fully-loaded page in the slot
		// TODO: Has failed:
		// - When page is non-null but not loaded, i.e. it's scene node is NULL
		// ADDENDUM: May be fixed now with the introduction of the terrain slot state machine
		OgreAssert (pSlot != NULL && pSlot->instance != NULL && pSlot->canSave(), "Can only save loaded dirty pages");

		if (pSlot->canSave())
		{
			pSlot->saving();
			saveTerrainImpl(pSlot, synchronous);

			return true;
		} else
			return false;
	}

	void OverhangTerrainGroup::saveTerrain_worker( OverhangTerrainSlot * pSlot )
	{
		PageSection * pPage = pSlot->instance;
		const ulong nTotalPageSize = options.getTotalPageSize();
		const unsigned short nTPP = options.getTilesPerPage();
		const ushort nTotalTiles = nTPP * nTPP;
		bool bSavedPage = false;

		if (_pPageProvider != NULL)
			bSavedPage = _pPageProvider->savePage(pPage, pSlot->x, pSlot->y, options.pageSize, nTotalPageSize);

		// If the page provider doesn't take-over full control of saving pages then we will perform the default save operation regardless of the
		// actual state of the page provider
		if (!bSavedPage)
		{
			DataStreamPtr pout = acquirePageStream(pSlot->x, pSlot->y, false);
			{
				StreamSerialiser out (pout);

				out.writeChunkBegin(CHUNK_PAGE_ID, CHUNK_PAGE_VERSION);
				const bool bHasPage = true;
				out.write (&bHasPage);
				*pPage >> out;

				out.writeChunkEnd(CHUNK_PAGE_ID);
			}
			pout->close();
		}
	}

	void OverhangTerrainGroup::saveTerrain_response( OverhangTerrainSlot * slot )
	{
		slot->doneSaving();

		if (slot->state() == OverhangTerrainSlot::TSS_Unloading)
			unloadTerrainImpl(slot);
	}

	// THREAD: *Main
	void OverhangTerrainGroup::preparePage(PageSection * const page, const OverhangTerrainSlot::LoadData * data)
	{
		String name;

		assert(data != NULL);

		// Create a node for all tiles to be attached to
		// Note we sequentially name since page can be attached at different points
		// so page x/z is not appropriate
		StringUtil::StrStreamType ss;
		ss << "Page[" << data->params.pageX << ',' << data->params.pageY << ']';
		name = ss.str();

		SceneNode * pSceneNode;
		if (getSceneManager()->hasSceneNode(name))
		{
			pSceneNode = getSceneManager()->getSceneNode(name);
		}
		else
		{
			pSceneNode = getSceneManager()->createSceneNode(name);
		}
		for (Channel::Index< ChannelProperties >::const_iterator i = _chanprops.begin(); i != _chanprops.end(); ++i)
		{
			if (!i->value->material.isNull())
				page->setMaterial(i->channel, i->value->material);
			page->setRenderQueue(i->channel, i->value->qid);
		}

		page->initialise(pSceneNode);
	}

	void OverhangTerrainGroup::linkPageNeighbors( const int16 x, const int16 y, PageSection * const pPage )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		OverhangTerrainSlot * pSlot;

		// WEST linkage
		pSlot = getTerrainSlot(x - _pxi, y);
		if (pSlot != NULL && pSlot->instance != NULL)
			pPage->linkPageNeighbor(pSlot->instance, VonN_WEST);

		// EAST linkage
		pSlot = getTerrainSlot(x + _pxi, y);
		if (pSlot != NULL && pSlot->instance != NULL)
			pPage->linkPageNeighbor(pSlot->instance, VonN_EAST);

		// NORTH linkage
		pSlot = getTerrainSlot(x, y - _pyi);
		if (pSlot != NULL && pSlot->instance != NULL)
			pPage->linkPageNeighbor(pSlot->instance, VonN_NORTH);

		// SOUTH linkage
		pSlot = getTerrainSlot(x, y + _pyi);
		if (pSlot != NULL && pSlot->instance != NULL)
			pPage->linkPageNeighbor(pSlot->instance, VonN_SOUTH);
	}

	Ogre::DataStreamPtr OverhangTerrainGroup::acquirePageStream( const int16 x, const int16 z, bool bReadOnly /*= true */ )
	{
		StringUtil::StrStreamType ssName;

		ssName << "ohtst-" << std::setw(8) << std::setfill('0') << std::hex << calculatePageID(x, z) << ".dat" << std::ends;
		const String sFilename = ssName.str();

		if (bReadOnly)
			return Root::getSingleton().openFileStream(sFilename, _sResourceGroup);
		else
			return Root::getSingleton().createFileStream(sFilename, _sResourceGroup, true);
	}

	void OverhangTerrainGroup::clear()
	{
		WorkRequestBase req;
		req.origin = this;
		Root::getSingleton().getWorkQueue()->addRequest(
			_nWorkQChannel, static_cast< uint16 > (DestroyAll),
			Any(req), 0, false
		);
	}

	void OverhangTerrainGroup::clear_response()
	{
		for (TerrainSlotMap::iterator i = _slots.begin(); i != _slots.end(); ++i)
		{
			if (i->second->canDestroy())
				i->second->destroy();

			if (i->second->state() != OverhangTerrainSlot::TSS_Destroy)
			{
				clear(); // Repeat the process all over again
				return;
			}
		}

		for (TerrainSlotMap::iterator i = _slots.begin(); i != _slots.end(); ++i)
			delete i->second;
		_slots.clear();
	}

	void OverhangTerrainGroup::setOrigin(const Vector3& pos)
	{
		if (pos != _ptOrigin)
		{
			_ptOrigin = pos;
			for (TerrainSlotMap::iterator i = _slots.begin(); i != _slots.end(); ++i)
			{
				OverhangTerrainSlot* slot = i->second;
				if (slot->instance)
				{
					slot->instance->setPosition(computeTerrainSlotPosition(slot->x, slot->y));
				}
			}
		}

	}
	void OverhangTerrainGroup::setMaterial(const Channel::Ident channel, const MaterialPtr & m)
	{
		if (m != _chanprops[channel].material)
		{
			_chanprops[channel].material = m;
			for (TerrainSlotMap::iterator i = _slots.begin(); i != _slots.end(); ++i)
			{
				i->second->setMaterial(channel, m);
			}
		}
	}

	void OverhangTerrainGroup::setRenderQueueGroup( const Channel::Ident channel, const int nQID )
	{
		if (nQID != _chanprops[channel].qid)
		{
			_chanprops[channel].qid = nQID;
			for (TerrainSlotMap::iterator i = _slots.begin(); i != _slots.end(); ++i)
			{
				i->second->setRenderQueueGroup(channel, nQID);
			}
		}
	}

	//-------------------------------------------------------------------------
	// THREAD: *Main
	void OverhangTerrainGroup::addMetaBall( const Vector3 & position, const Real radius, const bool excavating /*= true*/, const bool synchronous /*= false*/ )
	{
		MetaBallWorkRequest req;
		req.origin = this;
		req.excavating = excavating;
		req.position.x = position.x;
		req.position.y = position.y;
		req.position.z = position.z;
		req.radius = radius;

		MetaBall moBallTest (position, radius, excavating);
		const AxisAlignedBox bboxBallPgStr = toSpace(OCS_World, OCS_Terrain, moBallTest.getAABB());
		const Vector3 ptOriginTerrainSpace = toSpace(OCS_World, OCS_Terrain, _ptOrigin);
		const Vector3 
			bb0 = bboxBallPgStr.getMinimum() - ptOriginTerrainSpace,
			bbN = bboxBallPgStr.getMaximum() - ptOriginTerrainSpace;

		Vector2
			p0 = Vector2(bb0.x, bb0.y),
			pN = Vector2(bbN.x, bbN.y);

		const Real
			nPWS = options.getPageWorldSize();

		p0 /= nPWS;
		pN /= nPWS;

		p0.x = floor (p0.x);
		p0.y = floor (p0.y);

		pN.x = ceil(pN.x);
		pN.y = ceil(pN.y);

		/*p0 -= options.cellScale;
		pN += options.cellScale;*/

		int16 xSlot, ySlot;
		Vector3 vecPageWalk;

		OHT_DBGTRACE("Add Meta Ball, " << position << ", bbox=" << moBallTest.getAABB());
		// TODO: This walk algorithm walks too far, it doesn't break anything but it is wrong and slightly more expensive as a result.  
		// It occurred at least once when the ball is near the center of a page
		// NOTE: OhTGrp::toSlotPosition(vec, ...) doesn't work because 'vec' could overlap up-to 4 pages logically yielding 4 slots
		for (vecPageWalk.y = p0.y; vecPageWalk.y <= pN.y; vecPageWalk.y += 1.0f)
		{
			for (vecPageWalk.x = p0.x; vecPageWalk.x <= pN.x; vecPageWalk.x += 1.0f)
			{
				// Simplification of toSlotPosition(vecPageWalk, xSlot, ySlot, OCS_Terrain);
				Vector3 ptSlot = vecPageWalk;

				// Round coordinates
				ptSlot.x = floor(ptSlot.x + 0.5f);
				ptSlot.y = floor(ptSlot.y + 0.5f);

				// Transform into paging-strategy space
				transformSpace(OCS_Terrain, OCS_PagingStrategy, ptSlot);

				xSlot = static_cast< int16 > (ptSlot.x);
				ySlot = static_cast< int16 > (ptSlot.y);	// TODO: Umm, I thought y was supposed to be flipped?

				OverhangTerrainSlot * pSlot = getTerrainSlot(xSlot, ySlot);

				if (
					pSlot == NULL || pSlot->instance == NULL 
					|| 
					( // Only inactive state allow further mutations
						!pSlot->canMutate()
					)
				)
					return; // If any terrain slots are busy, we abort the meta-ball operation completely
				else
				{
					req.slots.push_back(pSlot);
				}
			}
		}

		for (MetaBallWorkRequest::SlotList::iterator i = req.slots.begin(); i != req.slots.end(); ++i)
			(*i)->mutating();
			
		Root::getSingleton().getWorkQueue()->addRequest(
			_nWorkQChannel, static_cast <uint16> (AddMetaObject), 
			Any(req), 0, synchronous);
	}

	void OverhangTerrainGroup::addMetaBall_worker( MetaBallWorkRequest::SlotList::iterator iBegin, MetaBallWorkRequest::SlotList::iterator iEnd, const Vector3 & position, const Real radius, const bool excavating /*= true*/ )
	{
		for (MetaBallWorkRequest::SlotList::iterator i = iBegin; i != iEnd; ++i)
		{
			OverhangTerrainSlot * pSlot = *i;
			OHT_DBGTRACE("\tPage, " << pSlot->position);
			pSlot->instance->addMetaBall(position - pSlot->position, radius, excavating);
		}
	}

	void OverhangTerrainGroup::addMetaBall_response( MetaBallWorkRequest::SlotList::iterator iBegin, MetaBallWorkRequest::SlotList::iterator iEnd )
	{
		oht_assert_threadmodel(ThrMdl_Main);
		while (iBegin != iEnd)
		{
			OverhangTerrainSlot * pSlot = *iBegin++;
			pSlot->instance->commitOperation();
			pSlot->doneMutating();
		}
	}

	Ogre::Vector3 OverhangTerrainGroup::computeTerrainSlotPosition( const int16 x, const int16 y ) const
	{
		const Real nPWsz = static_cast< Real > (options.getPageWorldSize());
		Vector3 v (
			static_cast< Real > (x), 
			static_cast< Real > (y),
			0
		);
		v *= nPWsz;

		transformSpace(OCS_PagingStrategy, OCS_World, v);
		return v += _ptOrigin;
	}

	OverhangTerrainManager::RayResult OverhangTerrainGroup::rayIntersects( Ray ray, const OverhangTerrainManager::RayQueryParams & params ) const
	{
		RayResult result(false, Vector3::ZERO, NULL);

		OHTDD_Clear();

		const float fTolerance = std::numeric_limits< Real >::epsilon() * 10000.0f;

		clamp(ray, fTolerance);
		OHT_DBGTRACE("TEST Ray: " << ray.getOrigin() << ":" << ray.getDirection());
		const Real
			fPageOffset = options.getPageWorldSize() / 2.0f;

		int16 py, px;
		OverhangTerrainSlot * pSlot0 = NULL, * pSlot;

		toSlotPosition(ray.getOrigin(), px, py);
		pSlot0 = NULL;

		for (DiscreteRayIterator i = DiscreteRayIterator::from(ray, options.getPageWorldSize(), -Vector3(1, 0, 1) * (options.getPageWorldSize() / 2.0f));
			i < params.limit;
			++i
		)
		{
			switch (i.neighbor)
			{
			case VonN_NORTH:	++py; break;	// Page-space has y-coordinate flipped
			case VonN_EAST:		++px; break;
			case VonN_SOUTH:	--py; break;	// Page-space has y-coordinate flipped
			case VonN_WEST:		--px; break;
			default:
				break;
			}
			pSlot = getTerrainSlot(px, py);

			if (pSlot == NULL || pSlot == pSlot0 || pSlot->instance == NULL || !pSlot->canRead())
				continue;

			pSlot->query();

			OHT_DBGTRACE(
				"\t[" << pSlot->x << "x" << pSlot->y << "], " <<
				"bbox=" << pSlot->instance->getBoundingBox()
			);
			Vector3
				origin = i.intersection(fTolerance) - pSlot->instance->getPosition(),
				direction = ray.getDirection();

#ifdef _DEBUG
			const Vector3
				p = origin + pSlot->instance->getPosition(),
				b0 = pSlot->instance->getBoundingBox().getMinimum() - options.cellScale,
				bN = pSlot->instance->getBoundingBox().getMaximum() + options.cellScale;
			OgreAssert(b0.x <= p.x && b0.z <= p.z && bN.x >= p.x && bN.z >= p.z, "Ray origin must be contained within the selected page");
#endif // _DEBUG

			bool bResult = pSlot->instance->rayIntersects(result, Ray(origin, direction), params, i.distance);
			pSlot->doneQuery();
			if (bResult)
				break;

			pSlot0 = pSlot;
		}

		if (!result.hit)
			OHT_DBGTRACE("No ray results");
		else
			OHT_DBGTRACE("RAY HIT: " << result.position);

		return result;		
	}

	void OverhangTerrainGroup::toSlotPosition( const Vector3 & pt, int16 & x, int16 & y, const OverhangCoordinateSpace encsPt /*= OCS_World*/ ) const
	{
		const Real nPWS = options.getPageWorldSize();
		// TODO: I think (nPWS * 0.5) is erroneous and the point shouldn't be centered like this
		Vector3 ptSlot = pt;
		
		// Rebase at terrain group origin
		ptSlot -= toSpace(OCS_World, encsPt, _ptOrigin);

		// Scale down to unit per page
		ptSlot /= nPWS;

		transformSpace(encsPt, OCS_Terrain, ptSlot);

		// Round coordinates
		ptSlot.x = floor(ptSlot.x + 0.5f);
		ptSlot.y = floor(ptSlot.y + 0.5f);

		// Transform into paging-strategy space
		transformSpace(OCS_Terrain, OCS_PagingStrategy, ptSlot);

		x = static_cast< int16 > (ptSlot.x);
		y = static_cast< int16 > (ptSlot.y);

#ifdef _DEBUG
		OverhangTerrainSlot * pSlot = getTerrainSlot(x, y);
		if (pSlot != NULL && pSlot->instance != NULL && pSlot->canRead())
		{
			const Vector3
				va = toSpace(OCS_World, OCS_Terrain, pSlot->instance->getBoundingBox().getMinimum()),
				vb = va + Vector3(nPWS, nPWS, 0),
				vp = toSpace(encsPt, OCS_Terrain, pt);

			assert(
				va.x <= vp.x && vb.x >= vp.x && 
				va.y <= vp.y && vb.y >= vp.y
			);
		}
#endif
	}

	Ogre::PageID OverhangTerrainGroup::calculatePageID( const int16 x, const int16 y ) const
	{
		return _pPagedWorld->calculatePageID(x, y);
	}

	void OverhangTerrainGroup::fillNeighbors( OverhangTerrainSlot ** vpSlots, const OverhangTerrainSlot * pSlot )
	{
		vpSlots[VonN_NORTH] = getTerrainSlot(pSlot->x,			pSlot->y - _pyi);
		vpSlots[VonN_WEST] = getTerrainSlot	(pSlot->x - _pxi,	pSlot->y);
		vpSlots[VonN_SOUTH] = getTerrainSlot(pSlot->x,			pSlot->y + _pyi);
		vpSlots[VonN_EAST] = getTerrainSlot	(pSlot->x + _pxi,	pSlot->y);
	}

	bool OverhangTerrainGroup::tryLockNeighborhood( const OverhangTerrainSlot * slot )
	{
		OverhangTerrainSlot * vpSlotNeighbor[CountVonNeumannNeighbors];
		fillNeighbors(vpSlotNeighbor, slot);

		bool bCanLockVonNeumannNeighborhood = true;
		for (unsigned c = 0; c < CountVonNeumannNeighbors; ++c)
		{
			OverhangTerrainSlot * pSlotNeighbor = vpSlotNeighbor[c];
			if (pSlotNeighbor != NULL)
				bCanLockVonNeumannNeighborhood = bCanLockVonNeumannNeighborhood && pSlotNeighbor->canNeighborQuery(Neighborhood::opposite((VonNeumannNeighbor)c));
			}
		if (bCanLockVonNeumannNeighborhood)
		{
			for (unsigned c = 0; c < CountVonNeumannNeighbors; ++c)
			{
				OverhangTerrainSlot * pSlotNeighbor = vpSlotNeighbor[c];
				if (pSlotNeighbor != NULL)
					pSlotNeighbor->setNeighborQuery(Neighborhood::opposite((VonNeumannNeighbor)c));
			}
		}
		return bCanLockVonNeumannNeighborhood;
	}

	void OverhangTerrainGroup::unlockNeighborhood( const OverhangTerrainSlot* slot )
	{
		OverhangTerrainSlot * vpSlotNeighbor[CountVonNeumannNeighbors];
		fillNeighbors(vpSlotNeighbor, slot);

		for (unsigned c = 0; c < CountVonNeumannNeighbors; ++c)
		{
			OverhangTerrainSlot * pSlotNeighbor = vpSlotNeighbor[c];
			VonNeumannNeighbor evnQuery = Neighborhood::opposite((VonNeumannNeighbor)c);
			if (pSlotNeighbor != NULL && pSlotNeighbor->isNeighborQueried(evnQuery))
				pSlotNeighbor->clearNeighborQuery(evnQuery);
		}
	}

	void OverhangTerrainGroup::saveTerrainImpl( OverhangTerrainSlot * pSlot, const bool synchronous /*= false*/ )
	{
		WorkRequest req;
		req.slot = pSlot;
		req.origin = this;
		Root::getSingleton().getWorkQueue()->addRequest(
			_nWorkQChannel, static_cast< uint16 > (SavePage),
			Any(req), 0, synchronous
		);
	}

	WorkQueue::RequestID OverhangTerrainGroup::generateSurfaceConfiguration( MetaFragment::Container * pMF, const IsoSurfaceRenderable * pISR, const unsigned nLOD, const Touch3DFlags enStitches )
	{
		oht_assert_threadmodel(ThrMdl_Main);
		SurfaceGenRequest reqdata;

		reqdata.mf = pMF;
		const Voxel::MetaVoxelFactory * pFactory = pMF->factory;
		reqdata.shadow = pISR->getShadow();
		reqdata.channel = pFactory->channel;
		reqdata.lod = nLOD;
		reqdata.surfaceFlags = pFactory->surfaceFlags;
		reqdata.stitches = enStitches;
		reqdata.vertexBufferCapacity = pISR->getVertexBufferCapacity(nLOD);

		return 
			Root::getSingleton().getWorkQueue()->addRequest(
			_nWorkQChannel, static_cast <uint16> (BuildSurface), 
			Any(reqdata), 0, false);
	}

	void OverhangTerrainGroup::cancelRequest( const WorkQueue::RequestID rid )
	{
		Root::getSingleton().getWorkQueue()->abortRequest(rid);
	}

}
