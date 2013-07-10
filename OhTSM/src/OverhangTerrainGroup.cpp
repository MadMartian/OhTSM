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
		_factory(sm->getOptions(), pManRsrcLoader),
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
			setMaterial(i->channel, i->value->material);
	}

	OverhangTerrainGroup::~OverhangTerrainGroup()
	{
		OHTDD_Destroy();

		WorkQueue* wq = Root::getSingleton().getWorkQueue();
		wq->abortRequestsByChannel(_nWorkQChannel);

		wq->removeRequestHandler(_nWorkQChannel, this);
		wq->removeResponseHandler(_nWorkQChannel, this);
	}

	PageSection * OverhangTerrainGroup::createPage()
	{
		return OGRE_NEW PageSection(this, &_factory, _descchan);
	}

	OverhangTerrainGroup::TerrainSlot* OverhangTerrainGroup::getTerrainSlot(const int16 x, const int16 y, bool createIfMissing)
	{
		PageID key = calculatePageID(x, y);
		TerrainSlotMap::iterator i = _slots.find(key);
		if (i != _slots.end())
			return i->second;
		else if (createIfMissing)
		{
			TerrainSlot* slot = new TerrainSlot(x, y);
			_slots[key] = slot;
			return slot;
		}
		return 0;
	}
	//---------------------------------------------------------------------
	OverhangTerrainGroup::TerrainSlot* OverhangTerrainGroup::getTerrainSlot(const int16 x, const int16 y) const
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
		TerrainSlot * slot = getTerrainSlot(x, y, true);

		if (slot->state() != TerrainSlot::TSS_Empty || !bLoad)
			return true;

		if (tryLockNeighborhood(slot))
		{
			OHT_DBGTRACE("LOAD " << slot->x << 'x' << slot->y);

			slot->instance = createPage();
			slot->data = new TerrainSlot::LoadData(options, x, y);
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
	bool OverhangTerrainGroup::defineTerrain_worker( TerrainSlot * slot )
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

	void OverhangTerrainGroup::defineTerrain_response( TerrainSlot* slot )
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

		TerrainSlot * pSlot = getTerrainSlot(x, y);
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

	void OverhangTerrainGroup::unloadTerrainImpl( TerrainSlot * pSlot, const bool synchronous /*= false */ )
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

	void OverhangTerrainGroup::unloadTerrain_worker( TerrainSlot * slot, UnloadPageResponseData & respdata )
	{
		OHT_DBGTRACE("Unload terrain " << slot->x << 'x' << slot->y);

		if (_pPageProvider != NULL)
			_pPageProvider->unloadPage(slot->x, slot->y);

		for (Channel::Descriptor::iterator j = _descchan.begin(); j != _descchan.end(); ++j)
			for (MetaFragmentIterator i = slot->instance->iterateMetaFrags(*j); i; ++i)
				respdata.dispose->push_back(i->acquireBasicInterface().surface);
	}

	void OverhangTerrainGroup::unloadTerrain_response( TerrainSlot * pSlot, UnloadPageResponseData &data )
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
		case DestroyAll:
			clear_response();
			break;
		}
	}

	// THREAD: *Main
	bool OverhangTerrainGroup::saveTerrain( TerrainSlot * pSlot, const bool synchronous /*= false*/ )
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

	void OverhangTerrainGroup::saveTerrain_worker( TerrainSlot * pSlot )
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

	void OverhangTerrainGroup::saveTerrain_response( TerrainSlot * slot )
	{
		slot->doneSaving();

		if (slot->state() == TerrainSlot::TSS_Unloading)
			unloadTerrainImpl(slot);
	}

	// THREAD: *Main
	void OverhangTerrainGroup::preparePage(PageSection * const page, const TerrainSlot::LoadData * data)
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
			if (!i->value->material.isNull())
				page->setMaterial(i->channel, i->value->material);

		page->initialise(pSceneNode);
	}

	void OverhangTerrainGroup::linkPageNeighbors( const int16 x, const int16 y, PageSection * const pPage )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		TerrainSlot * pSlot;

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

			if (i->second->state() != TerrainSlot::TSS_Destroy)
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
				TerrainSlot* slot = i->second;
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

				TerrainSlot * pSlot = getTerrainSlot(xSlot, ySlot);

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
			TerrainSlot * pSlot = *i;
			OHT_DBGTRACE("\tPage, " << pSlot->position);
			pSlot->instance->addMetaBall(position - pSlot->position, radius, excavating);
		}
	}

	void OverhangTerrainGroup::addMetaBall_response( MetaBallWorkRequest::SlotList::iterator iBegin, MetaBallWorkRequest::SlotList::iterator iEnd )
	{
		oht_assert_threadmodel(ThrMdl_Main);
		while (iBegin != iEnd)
		{
			TerrainSlot * pSlot = *iBegin++;
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
		int16 nCurrPageY, nCurrPageX;

		{ // Clamp the ray to a coarser precision and so that all components are non-zero
			Vector3 vDir = ray.getDirection();
			const Real fTolerance = std::numeric_limits< Real >::epsilon() * 10000.0f;

			if (vDir.x > -fTolerance && vDir.x < +fTolerance)
				vDir.x = vDir.x < 0 ? -fTolerance : +fTolerance;

			if (vDir.y > -fTolerance && vDir.y < +fTolerance)
				vDir.y = vDir.y < 0 ? -fTolerance : +fTolerance;

			if (vDir.z > -fTolerance && vDir.z < +fTolerance)
				vDir.z = vDir.z < 0 ? -fTolerance : +fTolerance;

			ray.setDirection(vDir);
		}

		toSlotPosition(ray.getOrigin(), nCurrPageX, nCurrPageY);
		TerrainSlot* pSlot = getTerrainSlot(nCurrPageX, nCurrPageY);	// May be NULL
		RayResult result(false, Vector3::ZERO, NULL);

		OHT_DBGTRACE("TEST Ray: " << ray.getOrigin() << ":" << ray.getDirection() << ", starting at slot [" << nCurrPageX << "x" << nCurrPageY << "]");
		const Vector3 
			vPageWalkDir = toSpace(OCS_World, OCS_PagingStrategy, ray.getDirection());

		Vector2 vWalkPage;
		{
			Vector3				// Normalise the offset  based on the world size of a square, and rebase to the bottom left
				vTmp = toSpace(OCS_World, OCS_PagingStrategy, (ray.getOrigin() - computeTerrainSlotPosition(nCurrPageX, nCurrPageY)) / options.getPageWorldSize()) + 0.5f; // Half-page offset starts at corner
		
			vWalkPage.x = (vTmp.x < 0 ? 1.0f + vTmp.x : vTmp.x);
			vWalkPage.y = (vTmp.y < 0 ? 1.0f + vTmp.y : vTmp.y);
		}

		// this is our counter moving away from the 'current' square
		Vector2 vInc (
			Math::Abs(vPageWalkDir.x), 
			Math::Abs(vPageWalkDir.y)
		);
		const int16 
			nDirX = vPageWalkDir.x > 0.0f ? 1 : -1,
			nDirY = vPageWalkDir.y > 0.0f ? 1 : -1;

		// We're always counting from 0 to 1 regardless of what direction we're heading
		if (nDirX < 0)
			vWalkPage.x = 1.0f - vWalkPage.x;
		if (nDirY < 0)
			vWalkPage.y = 1.0f - vWalkPage.y;

		// find next slot
		bool bSearching = true;
		int nGapCount = 0;

		if (Math::RealEqual(vInc.x, 0.0f) && Math::RealEqual(vInc.y, 0.0f))
			bSearching = false;

		while(bSearching)
		{
			while ( (pSlot == NULL || pSlot->instance == NULL || !pSlot->canRead()) && bSearching)
			{
				++nGapCount;
				/// if we don't find any filled slot in 6 traversals, give up
				if (nGapCount > 6)
				{
					bSearching = false;
					break;
				}

				// find next slot
				const Vector2 vWalkSlot0 = vWalkPage;

				while (vWalkPage.x < 1.0f && vWalkPage.y < 1.0f)
					vWalkPage += vInc;

				if (vWalkPage.x >= 1.0f && vWalkPage.y >= 1.0f)
				{
					// We crossed a corner, need to figure out which we passed first
					const Real 
						fDistY = (1.0f - vWalkSlot0.y) / vInc.y,
						fDistX = (1.0f - vWalkSlot0.x) / vInc.x;

					if (fDistX < fDistY)
					{
						nCurrPageX += nDirX;
						vWalkPage.x -= 1.0f;
					}
					else
					{
						nCurrPageY += nDirY;
						vWalkPage.y -= 1.0f;
					}

				}
				else if (vWalkPage.x >= 1.0f)
				{
					nCurrPageX += nDirX;
					vWalkPage.x -= 1.0f;
				}
				else if (vWalkPage.y >= 1.0f)
				{
					nCurrPageY += nDirY;
					vWalkPage.y -= 1.0f;
				}
				if (params.limit)
				{
					if (ray.getOrigin().distance(computeTerrainSlotPosition(nCurrPageX, nCurrPageY)) > params.limit)
					{
						bSearching = false;
						break;
					}
				}
				pSlot = getTerrainSlot(nCurrPageX, nCurrPageY);
			}

			// Check sought slot
			// TODO: pageSceneNode != NULL is ugly, use a "isReady/isLoaded" getter checker instead
			if (pSlot != NULL && pSlot->instance != NULL && pSlot->canRead())
			{
				nGapCount = 0;
				OHT_DBGTRACE(
					"\t[" << pSlot->x << "x" << pSlot->y << "], " <<
					"bbox=" << pSlot->instance->getBoundingBox()
				);
				if (pSlot->instance->rayIntersects(result, ray, params))
				{
					bSearching = false;
					break;
				}
				else
				{
					// not this one, trigger search for another slot
					pSlot = NULL;
				}
			}

		}

		if (!result.hit)
			OHT_DBGTRACE("No ray results");
		else
			OHT_DBGTRACE(
				"RAY HIT: " << result.position << ", " <<
				"bbox=" << pSlot->instance->getBoundingBox()
			);

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
		y = static_cast< int16 > (ptSlot.y);	// TODO: Umm, I thought y was supposed to be flipped?

#ifdef _DEBUG
		TerrainSlot * pSlot = getTerrainSlot(x, y);
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

	void OverhangTerrainGroup::fillNeighbors( TerrainSlot ** vpSlots, const TerrainSlot * pSlot )
	{
		vpSlots[VonN_NORTH] = getTerrainSlot(pSlot->x,			pSlot->y - _pyi);
		vpSlots[VonN_WEST] = getTerrainSlot	(pSlot->x - _pxi,	pSlot->y);
		vpSlots[VonN_SOUTH] = getTerrainSlot(pSlot->x,			pSlot->y + _pyi);
		vpSlots[VonN_EAST] = getTerrainSlot	(pSlot->x + _pxi,	pSlot->y);
	}

	bool OverhangTerrainGroup::tryLockNeighborhood( const TerrainSlot * slot )
	{
		TerrainSlot * vpSlotNeighbor[CountVonNeumannNeighbors];
		fillNeighbors(vpSlotNeighbor, slot);

		bool bCanLockVonNeumannNeighborhood = true;
		for (unsigned c = 0; c < CountVonNeumannNeighbors; ++c)
		{
			TerrainSlot * pSlotNeighbor = vpSlotNeighbor[c];
			if (pSlotNeighbor != NULL)
				bCanLockVonNeumannNeighborhood = bCanLockVonNeumannNeighborhood && pSlotNeighbor->canNeighborQuery(Neighborhood::opposite((VonNeumannNeighbor)c));
			}
		if (bCanLockVonNeumannNeighborhood)
		{
			for (unsigned c = 0; c < CountVonNeumannNeighbors; ++c)
			{
				TerrainSlot * pSlotNeighbor = vpSlotNeighbor[c];
				if (pSlotNeighbor != NULL)
					pSlotNeighbor->setNeighborQuery(Neighborhood::opposite((VonNeumannNeighbor)c));
			}
		}
		return bCanLockVonNeumannNeighborhood;
	}

	void OverhangTerrainGroup::unlockNeighborhood( const TerrainSlot* slot )
	{
		TerrainSlot * vpSlotNeighbor[CountVonNeumannNeighbors];
		fillNeighbors(vpSlotNeighbor, slot);

		for (unsigned c = 0; c < CountVonNeumannNeighbors; ++c)
		{
			TerrainSlot * pSlotNeighbor = vpSlotNeighbor[c];
			VonNeumannNeighbor evnQuery = Neighborhood::opposite((VonNeumannNeighbor)c);
			if (pSlotNeighbor != NULL && pSlotNeighbor->isNeighborQueried(evnQuery))
				pSlotNeighbor->clearNeighborQuery(evnQuery);
		}
	}

	void OverhangTerrainGroup::saveTerrainImpl( TerrainSlot * pSlot, const bool synchronous /*= false*/ )
	{
		WorkRequest req;
		req.slot = pSlot;
		req.origin = this;
		Root::getSingleton().getWorkQueue()->addRequest(
			_nWorkQChannel, static_cast< uint16 > (SavePage),
			Any(req), 0, synchronous
		);
	}

	OverhangTerrainGroup::TerrainSlot::TerrainSlot( const int16 x, const int16 y ) 
	: x(x), y(y), instance (NULL), data(NULL), _enState0(TSS_Empty), _enState(TSS_Empty), queryNeighbors(0)
	{

	}

	void OverhangTerrainGroup::TerrainSlot::freeLoadData()
	{
		delete data;
		data = NULL;
	}

	OverhangTerrainGroup::TerrainSlot::~TerrainSlot()
	{
		delete data; 
		delete instance;
	}

	void OverhangTerrainGroup::TerrainSlot::setNeighborQuery( const VonNeumannNeighbor evnNeighbor )
	{
		if (_enState != TSS_Neutral && _enState != TSS_Empty && (_enState != TSS_NeighborQuery || (queryNeighbors & Neighborhood::flag(evnNeighbor))))
			throw StateEx("Cannot set query neighbor state, slot is busy");

		if (!queryNeighbors)
			_enState0 = _enState;
		_enState = TSS_NeighborQuery;
		queryNeighbors |= Neighborhood::flag(evnNeighbor);
	}

	void OverhangTerrainGroup::TerrainSlot::clearNeighborQuery(const VonNeumannNeighbor evnNeighbor)
	{
		if (_enState != TSS_NeighborQuery || !(queryNeighbors & Neighborhood::flag(evnNeighbor)))
			throw StateEx("Cannot clear neighbor query state, wasn't previously set");

		queryNeighbors ^= Neighborhood::flag(evnNeighbor);

		if (!queryNeighbors)
		{
			_enState = _enState0;
			if (_enState == TSS_Neutral || _enState == TSS_Empty)
				processPendingTasks();
		}
	}

	bool OverhangTerrainGroup::TerrainSlot::canNeighborQuery( const VonNeumannNeighbor evnNeighbor ) const
	{
		switch (_enState)
		{
		case TSS_Neutral:
		case TSS_Empty:
			return true;
		case TSS_NeighborQuery:
			return !(queryNeighbors & Neighborhood::flag(evnNeighbor));
		default:
			return false;
		}
	}

	bool OverhangTerrainGroup::TerrainSlot::isNeighborQueried( const VonNeumannNeighbor evnNeighbor ) const
	{
		return _enState == TSS_NeighborQuery && (queryNeighbors & Neighborhood::flag(evnNeighbor));
	}


	void OverhangTerrainGroup::TerrainSlot::saving()
	{
		if (_enState != TSS_Neutral)
			throw StateEx("State was not neutral or save-unload");

		_enState0 = _enState;
		_enState = TSS_Saving;
	}

	void OverhangTerrainGroup::TerrainSlot::doneSaving()
	{
		if (_enState != TSS_Saving)
			throw StateEx("State was not saving");

		_enState = _enState0;
		if (_enState == TSS_Neutral || _enState == TSS_Empty)
			processPendingTasks();
	}

	void OverhangTerrainGroup::TerrainSlot::saveUnload()
	{
		if (_enState != TSS_Neutral)
			throw StateEx("State was not neutral");

		_enState0 = TSS_Unloading;
		_enState = TSS_Saving;
	}

	void OverhangTerrainGroup::TerrainSlot::mutating()
	{
		if (_enState != TSS_Neutral)
			throw StateEx("State was not neutral");

		_enState = TSS_Mutate;
	}

	void OverhangTerrainGroup::TerrainSlot::doneMutating()
	{
		if (_enState != TSS_Mutate)
			throw StateEx("State was not mutating");

		_enState = TSS_Neutral;
		processPendingTasks();
	}

	void OverhangTerrainGroup::TerrainSlot::loading()
	{
		if (_enState != TSS_Empty)
			throw StateEx("State was not empty");

		_enState = TSS_Loading;
	}

	void OverhangTerrainGroup::TerrainSlot::doneLoading()
	{
		if (_enState != TSS_Loading)
			throw StateEx("State was not loading");

		_enState = TSS_Neutral;
		processPendingTasks();
	}

	void OverhangTerrainGroup::TerrainSlot::unloading()
	{
		if (_enState != TSS_Neutral)
			throw StateEx("State was not neutral");

		_enState = TSS_Unloading;
	}

	void OverhangTerrainGroup::TerrainSlot::doneUnloading()
	{
		if (_enState != TSS_Unloading)
			throw StateEx("State was not unloading");

		_enState = TSS_Empty;
		processPendingTasks();
	}

	void OverhangTerrainGroup::TerrainSlot::destroy()
	{
		if (_enState != TSS_Neutral)
			throw StateEx("State was not neutral");

		_enState = TSS_Destroy;
	}

	void OverhangTerrainGroup::TerrainSlot::processPendingTasks()
	{
		while(!_qJoinTasks.empty())
		{
			JoinTask task = _qJoinTasks.front();
			_qJoinTasks.pop();
			switch (task.type)
			{
			case JoinTask::JTT_Destroy: 
				delete instance;
				instance = NULL;
				destroy();
				break;
			case JoinTask::JTT_SetMaterial:
				instance->setMaterial(task.channel, task.material);
				break;
			}
		}
	}

	void OverhangTerrainGroup::TerrainSlot::setMaterial( const Channel::Ident channel, MaterialPtr pMaterial )
	{
		if (_enState == TSS_Neutral)
			instance->setMaterial(channel, pMaterial);
		else
		{
			JoinTask task;
			task.type = JoinTask::JTT_SetMaterial;
			task.material = pMaterial;
			task.channel = channel;
			_qJoinTasks.push(task);
		}
	}

	void OverhangTerrainGroup::TerrainSlot::destroySlot()
	{
		if (_enState == TSS_Neutral || _enState == TSS_Empty)
		{
			delete instance;
			instance = NULL;
			destroy();
		}
		else
		{
			JoinTask task;
			task.type = JoinTask::JTT_Destroy;
			_qJoinTasks.push(task);
		}
	}

	OverhangTerrainGroup::TerrainSlot::StateEx::StateEx( const char * szMsg ) 
		: std::exception(szMsg) 
	{
		LogManager::getSingleton().stream(LML_CRITICAL) << szMsg;
	}

}
