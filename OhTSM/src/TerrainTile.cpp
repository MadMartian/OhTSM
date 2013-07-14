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

#include <OgreSceneNode.h>

#include "TerrainTile.h"
#include "PageSection.h"
#include "IsoSurfaceRenderable.h"
#include "MetaBall.h"
#include "MetaHeightmap.h"
#include "MetaWorldFragment.h"
#include "DebugTools.h"

#include "OverhangTerrainManager.h"

namespace Ogre
{
	TerrainTile::TerrainTile(const size_t p, const size_t q, const Channel::Descriptor & descchann, PagePrivateNonthreaded * const page, const OverhangTerrainOptions & opts)
	: p(p), q(q), 
	borders(
		getTouch2DSide(
			getTouchStatus(p, 0, opts.getTilesPerPage() - 1), 
			getTouchStatus(q, 0, opts.getTilesPerPage() - 1)
		)
	),
	_index2mapMF(descchann), _dirtyMF(descchann), _properties(descchann),
	_options(opts), _pPage(page), _bInit(false), _bParameterized(false)
	{
		for ( int i = 0; i < CountVonNeumannNeighbors; i++ )
			_vpInternalNeighbors[ i ] = NULL;
	}

	TerrainTile::~TerrainTile()
	{
		oht_assert_threadmodel(ThrMdl_Single);

		for (Channel::Index< MetaFragMap >::iterator j = _index2mapMF.begin(); j != _index2mapMF.end(); ++j)
		{
			for(MetaFragMap::iterator it = j->value->begin(); it != j->value->end(); ++it)
			{
				{
					MetaFragment::Interfaces::Unique fragment = it->second->acquireInterface();
					_pPage->fireOnDestroyMetaRegion(j->channel, &fragment);
				}
				delete it->second;
			}
		}

	}

	void TerrainTile::initNeighbor(const VonNeumannNeighbor n, TerrainTile * t )
	{
		oht_assert_threadmodel(ThrMdl_Single);
		OgreAssert(_vpInternalNeighbors[n] == NULL, "Neighbor already initialized");
		_vpInternalNeighbors[n] = t;
	}

	void TerrainTile::setRenderQueueGroup( const Channel::Ident channel, uint8 qid )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		_properties[channel].renderq = qid;
		Channel::Index< MetaFragMap >::iterator j = _index2mapMF.find(channel);

		if (j != _index2mapMF.end())
		{
			for(MetaFragMap::iterator i = j->value->begin(); i != j->value->end(); ++i)
			{
				i->second->acquireBasicInterface().surface->setRenderQueueGroup(qid);
			}
		}
	}

	void TerrainTile::operator << ( const PageInitParams::TileParams & params )
	{
		oht_assert_threadmodel(ThrMdl_Background);

		const int 
			endx = params.vx0 + _options.tileSize,
			endz = params.vy0 + _options.tileSize;

		_x0 = params.vx0;
		_y0 = params.vy0;

		const Vector3 vecOffsetHalf = Vector3(1, 0, 1) * (_options.getPageWorldSize() / 2);
		_bbox.minimum.x = ( Real ) params.vx0 * _options.cellScale - vecOffsetHalf.x;
		_bbox.minimum.y = ( Real ) params.vy0 * _options.cellScale - vecOffsetHalf.z;

		_bbox.maximum.x = ( Real ) ( endx - 1 ) * _options.cellScale - vecOffsetHalf.x;
		_bbox.maximum.y = ( Real ) ( endz - 1 ) * _options.cellScale - vecOffsetHalf.z;

		_pos.x = ( params.vx0 * _options.cellScale + (endx - 1) * _options.cellScale ) / 2 - vecOffsetHalf.x;
		_pos.y = ( params.vy0 * _options.cellScale + (endz - 1) * _options.cellScale ) / 2 - vecOffsetHalf.z;

		_bParameterized = true;
	}

	void TerrainTile::initialise(SceneNode * pParentSceneNode)
	{
		oht_assert_threadmodel(ThrMdl_Main);

		for (Channel::Index< MetaFragMap >::iterator j = _index2mapMF.begin(); j != _index2mapMF.end(); ++j)
		{
			for (MetaFragMap::iterator i = j->value->begin(); i != j->value->end(); ++i)
			{
				MetaFragment::Container * pMWF = i->second;
				const MetaFragment::Container * pConstMWF = i->second;
				initialiseMWF(j->channel, pParentSceneNode, pConstMWF->acquireBasicInterface(), pMWF->acquireBuilderInterface());
			}
		}

		_bInit = true;
	}

	void TerrainTile::voxeliseTerrain( MetaHeightMap * const pMetaHeightMap )
	{
		oht_assert_threadmodel(ThrMdl_Background);

		Real min, max;
		pMetaHeightMap->span(_x0, _y0, _x0 + _options.tileSize, _y0 + _options.tileSize, min, max);

		const YLevel
			ylmb0 = computeYLevel(min-1),
			ylmbN = computeYLevel(max+1);

		for (YLevel yli = ylmb0; yli <= ylmbN; ++yli)
		{
			MetaFragment::Container * pFrag = acquireMetaWorldFragment(TERRAIN_ENTITY_CHANNEL, yli);
			pFrag->acquire< MetaFragment::Interfaces::Unique > ()
				.addMetaObject(pMetaHeightMap);
		}
	}

	void TerrainTile::unlinkeHeightMap( const MetaHeightMap * const pMetaHeightMap )
	{
		MetaFragMap & map = _index2mapMF[TERRAIN_ENTITY_CHANNEL];
		for (MetaFragMap::iterator i = map.begin(); i != _index2mapMF[TERRAIN_ENTITY_CHANNEL].end(); ++i)
			i->second->acquire< MetaFragment::Interfaces::Unique > ()
			.removeMetaObject(pMetaHeightMap);
	}

	void TerrainTile::initialiseMWF( const Channel::Ident channel, SceneNode * pParentSceneNode, MetaFragment::Interfaces::const_Basic & basic, MetaFragment::Interfaces::Builder & builder )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		StringUtil::StrStreamType 
			ssSceneNodeName, 
			ssISRName;

		ssSceneNodeName << "MWF[" << p << ',' << q << ';' << basic.ylevel << "] (" << _pPage->getPageX() << 'x' << _pPage->getPageY() << ')';
		AxisAlignedBox bboxChild = getYLevelBounds(basic.ylevel, OCS_Terrain);
		_pPage->getManager().transformSpace(OCS_Terrain, OCS_World, bboxChild);

		OHT_DBGTRACE("Create Scene Node at " << bboxChild.getCenter() << " relative to " << pParentSceneNode->getPosition());
		SceneNode * pScNode = pParentSceneNode->createChildSceneNode(ssSceneNodeName.str(), bboxChild.getCenter());

		ssISRName << "ISR[" << p << ',' << q << ';' << basic.ylevel << "] (" << _pPage->getPageX() << 'x' << _pPage->getPageY() << ')';

		builder.initialise(_options.primaryCamera, pScNode, ssISRName.str());

		Channel::Index< ChannelProperties >::const_iterator p = _properties.find(channel);
		if (p != _properties.end())
		{
			builder.setMaterial(p->value->material);
			builder.surface->setRenderQueueGroup(p->value->renderq);
		}

		_pPage->fireOnInitMetaRegion(channel, &basic, &builder);
	}

	void TerrainTile::setMaterial( const Channel::Ident channel, const MaterialPtr& m )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		_properties[channel].material = m;

		Channel::Index< MetaFragMap >::iterator j = _index2mapMF.find(channel);

		if (j != _index2mapMF.end())
		{
			for(MetaFragMap::iterator i = j->value->begin(); i != j->value->end(); ++i)
			{
				MetaFragment::Interfaces::Builder fragment = i->second->acquireBuilderInterface();
				fragment.setMaterial(m);
			}
		}
	}

	const TerrainTile* TerrainTile::raySelectNeighbour(const Ray& ray, Real distanceLimit /* = 0 */) const
	{
		oht_assert_threadmodel(ThrMdl_Main);
		const Real nWorldSize = _options.getTileWorldSize();
		const size_t
			nSize = _options.tileSize;

		Ray modifiedRay(ray.getOrigin(), ray.getDirection());
		// Move back half a square - if we're on the edge of the AABB we might
		// miss the intersection otherwise; it's ok for everywhere else since
		// we want the far intersection anyway
		modifiedRay.setOrigin(modifiedRay.getPoint(-nWorldSize/nSize * 0.5f));

		// transform into terrain space
		Vector3 tPos = modifiedRay.getOrigin();
		const Vector3 & tDir = modifiedRay.getDirection();

		// Reposition at tile origin, height doesn't matter
		// TODO: Check
		tPos.x -= _pos.x;
		tPos.y -= _pos.y;

		// Translate and convert to terrain space
		//tPos.x -= nWorldSize * 0.5;
		//tPos.y -= nWorldSize * 0.5;
		tPos /= nWorldSize;

		// Discard rays with no lateral component
		if (Math::RealEqual(modifiedRay.getDirection().x, 0.0f, 1e-4) && Math::RealEqual(modifiedRay.getDirection().y, 0.0f, 1e-4))
			return 0;

		Ray terrainRay(tPos, modifiedRay.getDirection());
		// Intersect with boundary planes 
		// Only collide with the positive (exit) side of the plane, because we may be
		// querying from a point outside ourselves if we've cascaded more than once
		Real dist = std::numeric_limits<Real>::max();
		std::pair<bool, Real> intersectResult;
		if (tDir.x < 0.0f)
		{
			intersectResult = Math::intersects(terrainRay, Plane(Vector3::UNIT_X, Vector3::ZERO));
			if (intersectResult.first && intersectResult.second < dist)
				dist = intersectResult.second;
		}
		else if (tDir.x > 0.0f)
		{
			intersectResult = Math::intersects(terrainRay, Plane(Vector3::NEGATIVE_UNIT_X, Vector3(1,0,0)));
			if (intersectResult.first && intersectResult.second < dist)
				dist = intersectResult.second;
		}
		if (tDir.y < 0.0f)
		{
			intersectResult = Math::intersects(terrainRay, Plane(Vector3::UNIT_Y, Vector3::ZERO));
			if (intersectResult.first && intersectResult.second < dist)
				dist = intersectResult.second;
		}
		else if (tDir.y > 0.0f)
		{
			intersectResult = Math::intersects(terrainRay, Plane(Vector3::NEGATIVE_UNIT_Y, Vector3(0,1,0)));
			if (intersectResult.first && intersectResult.second < dist)
				dist = intersectResult.second;
		}


		// discard out of range
		if (distanceLimit && dist * nWorldSize > distanceLimit)
			return 0;

		Vector3 terrainIntersectPos = terrainRay.getPoint(dist);
		Real x = terrainIntersectPos.x;
		Real y = terrainIntersectPos.y;
		Real dx = tDir.x;
		Real dy = tDir.y;

		// Never return diagonal directions, we will navigate those recursively anyway
		if (Math::RealEqual(x, 1.0f, 1e-4f) && dx > 0)
			return _vpInternalNeighbors[VonN_EAST];
		else if (Math::RealEqual(x, 0.0f, 1e-4f) && dx < 0)
			return _vpInternalNeighbors[VonN_WEST];
		else if (Math::RealEqual(y, 1.0f, 1e-4f) && dy > 0)		// TODO: Used to be y = 1.0 for PlN_NORTH and y = 0.0 for PlN_SOUTH, verify change is acceptable
			return _vpInternalNeighbors[VonN_SOUTH];
		else if (Math::RealEqual(y, 0.0f, 1e-4f) && dy < 0)
			return _vpInternalNeighbors[VonN_NORTH];

		return 0;
	}

	bool TerrainTile::rayIntersects( OverhangTerrainManager::RayResult & result, const Ray& rayPageRelTerrainSpace, const OverhangTerrainManager::RayQueryParams & params, bool bCascade /*= false*/ ) const
	{
		oht_assert_threadmodel(ThrMdl_Main);
		std::pair<bool, Real> intersection;

		result.hit = false;
		
		intersection = rayPageRelTerrainSpace % _bbox;

		if (!intersection.first)
		{
			if (bCascade)
			{
				const TerrainTile* pNeighbor = raySelectNeighbour(rayPageRelTerrainSpace, params.limit);
				if (pNeighbor != NULL)
					return pNeighbor->rayIntersects(result, rayPageRelTerrainSpace, params, bCascade);
			}
			return false;
		}


		OHT_DBGTRACE(
			"\t\t\t@ " 
			<< 
				_pPage->getManager().toSpace(OCS_Terrain, OCS_World, rayPageRelTerrainSpace.getPoint(intersection.second))
				+ _pPage->getPublic()->getPosition()
		);

		{
			OHTDD_Coords(OCS_World, OCS_Terrain);
			OHTDD_Color(DebugDisplay::MC_Blue);
			OHTDD_Point(rayPageRelTerrainSpace.getPoint(intersection.second));
			intersection = rayIntersectsMetaWorld(result, rayPageRelTerrainSpace, intersection.second, params);
		}
			
		// Only traverse this test when there have been meta-ball intersections and at least a terrain quad intersection unless we are underground.
		// The case is skipped is whenever the ray passes through this tile entirely above ground (original terrain-renderable height, not meta)

		if (!intersection.first && bCascade)
		{
			const TerrainTile * pNeighbor = raySelectNeighbour(rayPageRelTerrainSpace, params.limit);
			if (pNeighbor != NULL)
				return pNeighbor->rayIntersects(result, rayPageRelTerrainSpace, params, bCascade);
		}

		result.hit = intersection.first;
		result.position = rayPageRelTerrainSpace.getPoint(intersection.second);

#ifdef _DEBUG
		if (result.hit)
		{
			OHT_DBGTRACE("\t\tResult: " << (_pPage->getManager().toSpace(OCS_Terrain, OCS_World, result.position) + _pPage->getPublic()->getPosition()));
		}
#endif // _DEBUG

		return result.hit;
	}

	std::pair< bool, Real > TerrainTile::rayIntersectsMetaWorld( OverhangTerrainManager::RayResult & result, const Ray & rayPageRelTerrainSpace, const Real tile, const OverhangTerrainManager::RayQueryParams & params ) const
	{
		oht_assert_threadmodel(ThrMdl_Single);

		Vector3
			// Terrain-tile BBox intersection point relative to page
			origin = rayPageRelTerrainSpace.getPoint(tile),
			direction = rayPageRelTerrainSpace.getDirection(),

			p = origin;

		OverhangTerrainManager::transformSpace(OCS_Terrain, _options.alignment, OCS_DataGrid, origin, _options.cellScale);
		OverhangTerrainManager::transformSpace(OCS_Terrain, _options.alignment, OCS_DataGrid, direction, _options.cellScale);
		direction.normalise();

		// Translate to meta-cube region
		OverhangTerrainManager::transformSpace(OCS_Terrain, _options.alignment, OCS_Terrain, p);

		// Store the origin y-level for translation
		const YLevel yl0 = computeYLevel(p.z);

		// Data-grid space coordinates of the first would-be meta-cube that the ray would intersect
		const AxisAlignedBox bboxOrigin = getYLevelBounds(yl0, OCS_DataGrid);

		// Translate the intersection point so it's relative to the first would-be meta-cube
		origin -= bboxOrigin.getMinimum();

		OHTDD_Coords(OCS_Terrain, OCS_DataGrid);
		OHTDD_Cube(bboxOrigin);
		OHTDD_Translate(-bboxOrigin.getMinimum());
		//Vector3 ptTerr = Vector3(_bbox.minimum.x, _bbox.minimum.y, 0);
		//OverhangTerrainManager::transformSpace(OCS_Terrain, _options.alignment, OCS_DataGrid, ptTerr);
		//OHTDD_Translate(ptTerr);
		OHTDD_Color(DebugDisplay::MC_Yellow);

		// Clamp intersection point start to boundaries
		const Vector2 size = _bbox.maximum - _bbox.minimum;
		origin.makeFloor(Vector3(size.x, std::numeric_limits< Real > ::max(), size.y) - 1.0f / (Real)Voxel::FS_Span);
		origin.makeCeil(Vector3::ZERO);

		const int nCellsPerCube = (int)_options.tileSize-1;

		// Create a ray in data-grid space with origin relative to the first would-be meta-cube intersected
		const Ray rayTileCubeIntersectRelDataGridSpace(origin, direction);
		RayCellWalk walker(rayTileCubeIntersectRelDataGridSpace.getOrigin(), rayTileCubeIntersectRelDataGridSpace.getDirection(), params.limit / _options.cellScale);

		walker.lod = 0;
		while (
			!walker->first &&
				walker && 
				walker.cell->i >=0 && walker.cell->i < nCellsPerCube && 
				walker.cell->k >=0 && walker.cell->k < nCellsPerCube
		)
		{
			const YLevel yl = YLevel::fromCell(walker.cell->j, nCellsPerCube) + yl0;	// DEPS: Y-Level computation

			if (params.channels.size == 0)
			{
				for (Channel::Index< MetaFragMap >::const_iterator j = _index2mapMF.begin(); j != _index2mapMF.end(); ++j)
					rayQueryWalkCell(result, walker, *j->value, yl, yl0, bboxOrigin, rayTileCubeIntersectRelDataGridSpace, nCellsPerCube);
			} else
			{
				for (unsigned c = 0; c < params.channels.size; ++c)
				{
					Channel::Index< MetaFragMap >::const_iterator j = _index2mapMF.find(params.channels.array[c]);

					if (j != _index2mapMF.end())
						rayQueryWalkCell(result, walker, *j->value, yl, yl0, bboxOrigin, rayTileCubeIntersectRelDataGridSpace, nCellsPerCube);
				}
			}
		}
		result.hit = walker->first;

		return std::pair< bool, Real > (walker->first, walker->second * _options.cellScale + tile);
	}

	void TerrainTile::rayQueryWalkCell(
		OverhangTerrainManager::RayResult &result,
		RayCellWalk &walker,
		const MetaFragMap & mapMF,
		const YLevel yl,
		const YLevel yl0,
		const AxisAlignedBox & bboxOrigin,
		const Ray &rayTileCubeIntersectRelDataGridSpace,
		const int nCellsPerCube
	) const
	{
		MetaFragMap::const_iterator iMWF = mapMF.find(yl);	// TODO: Cache this result because yLevel changes very infrequently

		OHT_DBGTRACE(yl);
		if (iMWF != mapMF.end())
		{
			MetaFragment::Interfaces::Unique fragment = iMWF->second->acquireInterface();

			const AxisAlignedBox bboxYL = getYLevelBounds(yl, OCS_DataGrid);
			const Vector3 ptRel = bboxOrigin.getMinimum() - bboxYL.getMinimum();

			// Translate ray so it's relative to the current meta-cube's position in data-grid space and coordinates
			const Ray rayCubeRelDataGridSpace(
				rayTileCubeIntersectRelDataGridSpace.getOrigin() + ptRel,
				rayTileCubeIntersectRelDataGridSpace.getDirection()
			);

			OHTDD_Translate(ptRel);
			fragment.rayQuery(
				walker,
				WorldCellCoords(
					0,
					DIFF(yl0, yl)*nCellsPerCube,	// DEPS: Y-Level computation
					0
				),
				rayCubeRelDataGridSpace
			);
			result.mwf = iMWF->second;
		} else
		{
			const Vector3 vwc((Real)walker.cell->i, (Real)walker.cell->j, (Real)walker.cell->k);
			const AxisAlignedBox bboxCell(vwc, vwc + 1);
			OHTDD_Cube(bboxCell);

			++walker;
		}
	}


	MetaFragment::Container * TerrainTile::acquireMetaWorldFragment( const Channel::Ident channel, const YLevel yl )
	{
		oht_assert_threadmodel(ThrMdl_Single);
		MetaFragMap & mapMF = _index2mapMF[channel];
		MetaFragMap::iterator iMWF = mapMF.find(yl);

		if (iMWF != mapMF.end())
			return iMWF->second;
		else
		{
			AxisAlignedBox bbox = getYLevelBounds(yl, OCS_World);
			MetaFragment::Container * pMWF = _pPage->getFactory().getVoxelFactory(channel) ->createMetaFragment(bbox, yl);
			MetaFragment::Interfaces::Unique fragment = pMWF->acquireInterface();

			_pPage->getManager().transformSpace(OCS_World, OCS_Vertex, bbox);

			// TODO: Threading - Ensure synchronized, revise contract to require client-imposed synchronization and no OGRE calls
			// TODO: Support channels
			_pPage->fireOnCreateMetaRegion(channel, fragment.block, &fragment, bbox);

			attachFragment(channel, fragment, pMWF);
			return pMWF;
		}
	}
	
	void TerrainTile::propagateMetaObject( const Channel::Ident channel, DirtyMWFSet & queues, MetaObject * const pMetaObj )
	{
		oht_assert_threadmodel(ThrMdl_Single);
		const AxisAlignedBox 
			bboxtrsMB = _pPage->getManager().toSpace(OCS_World, OCS_Terrain, pMetaObj->getAABB());	// DEPS: MetaBall World Space

		const YLevel
			ylmb0 = computeYLevel(bboxtrsMB.getMinimum().z - _options.cellScale - 1),
			ylmbN = computeYLevel(bboxtrsMB.getMaximum().z + _options.cellScale + 1);

		MetaFragment::Container * pMWF;

		for (YLevel yli = ylmb0; yli <= ylmbN; ++yli)
		{
			OHT_DBGTRACE("\t\t\tY-Level: " << yli);

			pMWF = acquireMetaWorldFragment(channel, yli);
			MetaFragment::Interfaces::Unique fragment = pMWF->acquireInterface();
			fragment.addMetaObject(pMetaObj);
			fragment.updateGrid();

			queues.insert(DirtyMF(pMWF, DirtyMF::Dirty));
		}
	}

	MetaFragMap::iterator TerrainTile::attachFragment( const Channel::Ident channel, MetaFragment::Interfaces::Unique & fragment, MetaFragment::Container * pMWF )
	{
		oht_assert_threadmodel(ThrMdl_Single);

		MetaFragMap & mapMF = _index2mapMF[channel];

		OgreAssert(mapMF.find(fragment.ylevel) == mapMF.end(), "MetaWorldFragment already exists in this terrain tile");

		return mapMF.insert(mapMF.begin(), MetaFragMap::value_type (fragment.ylevel, pMWF));
	}

	void TerrainTile::applyFragment( const Channel::Ident channel, MetaFragment::Interfaces::const_Basic & basic, MetaFragment::Interfaces::Builder & builder )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		OgreAssert(!basic.isInitialized(), "Expected uninitialized fragment");

		MetaFragMap & mapMF = _index2mapMF[channel];
		MetaFragMap::iterator i, j, k;
		i = j = k = mapMF.find(basic.ylevel);

		OgreAssert(j != mapMF.end(), "Could not find MWF in map");
		--i;
		++k;
		OgreAssert(
			(i == mapMF.end() || i->first < j->first) &&
			(k == mapMF.end() || j->first < k->first),
			"Order assumption violated"
		);

		if (i != mapMF.end() && DIFF(j->first, i->first) == 1)
			builder.linkNeighbor(OrthoN_BELOW, i->second);
		if (k != mapMF.end() && DIFF(j->first, k->first) == -1)
			builder.linkNeighbor(OrthoN_ABOVE, k->second);

		_pPage->linkFragmentHorizontalInternal(channel, this, j);
	}

	void TerrainTile::addMetaObject( const Channel::Ident channel, MetaObject * const pMetaObject )
	{
		oht_assert_threadmodel(ThrMdl_Single);
		const AxisAlignedBox bboxMetaObjectTileSpace = _pPage->getManager().toSpace(OCS_World, OCS_Terrain, pMetaObject->getAABB());
		const Real nTWS = static_cast< Real > (_pPage->getManager().options.getTileWorldSize());

		DirtyMWFSet queues;

		propagateMetaObject (channel, queues, pMetaObject);

		if (!queues.empty())
			_dirtyMF[channel].insert(queues.begin(), queues.end());
	}

	void TerrainTile::loadMetaObject( const Channel::Ident channel, MetaObject * const pMetaObject )
	{
		oht_assert_threadmodel(ThrMdl_Single);

		OgreAssert(
			pMetaObject->getObjectType() != MetaObject::MOT_HeightMap && 
			pMetaObject->getObjectType() != MetaObject::MOT_Invalid, 

			"Invalid meta-object type"
		);

		const AxisAlignedBox 
			bboxtrsMB = _pPage->getManager().toSpace(OCS_World, OCS_Terrain, pMetaObject->getAABB());	// DEPS: MetaBall World Space

		const Real fTileSize = _options.getTileWorldSize();
		const YLevel
			ylmb0 = YLevel::fromYCoord(bboxtrsMB.getMinimum().z - _options.cellScale - 1, fTileSize),
			ylmbN = YLevel::fromYCoord(bboxtrsMB.getMaximum().z + _options.cellScale + 1, fTileSize);

		MetaFragment::Container * pMWF;
		for (YLevel yli = ylmb0; yli <= ylmbN; ++yli)
		{
			OHT_DBGTRACE("\t\t\tY-Level: " << yli);

			pMWF = acquireMetaWorldFragment(channel, yli);
			MetaFragment::Interfaces::Unique fragment = pMWF->acquireInterface();
			fragment.addMetaObject(pMetaObject);
		}
	}

	Ogre::AxisAlignedBox TerrainTile::getYLevelBounds( const YLevel yl, const OverhangCoordinateSpace encsTo /*= OCS_Terrain*/ ) const
	{
		oht_assert_threadmodel(ThrMdl_Single);

		AxisAlignedBox bbox(
			_bbox.minimum.x, _bbox.minimum.y, 0,
			_bbox.maximum.x, _bbox.maximum.y, 0
		);
		bbox.setMinimumZ(yl.toYCoord(_pPage->getManager().options.getTileWorldSize()));
		bbox.setMaximumZ(bbox.getMinimum().z + _pPage->getManager().options.getTileWorldSize());
		_pPage->getManager().transformSpace(OCS_Terrain, encsTo, bbox);
		return bbox;
	}

	MetaFragMap::const_iterator TerrainTile::beginFrags(const Channel::Ident ident) const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		return _index2mapMF[ident].begin();
	}

	MetaFragMap::const_iterator TerrainTile::endFrags(const Channel::Ident ident) const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		return _index2mapMF[ident].end();
	}

	StreamSerialiser & TerrainTile::operator >> (StreamSerialiser & output) const
	{
		oht_assert_threadmodel(ThrMdl_Single);

		uint16 c = 0;
		{ for (Channel::Index< MetaFragMap >::const_iterator j = _index2mapMF.begin(); j != _index2mapMF.end(); ++j, c++); }
		output.write(&c);

		for (Channel::Index< MetaFragMap >::const_iterator j = _index2mapMF.begin(); j != _index2mapMF.end(); ++j)
		{
			output << j->channel;
			const size_t nCountMWF = j->value->size();
			output.write(&nCountMWF);
			for (MetaFragMap::const_iterator i = j->value->begin(); i != j->value->end(); ++i)
			{
				const MetaFragment::Container * pMWF = i->second;
				MetaFragment::Interfaces::Shared fragment = pMWF->acquireInterface();
				fragment >> output;
			}
		}

		//OHT_DBGTRACE("wrote " << nCountMWF << " meta world fragments");
		return output;
	}
	StreamSerialiser & TerrainTile::operator << (StreamSerialiser & input)
	{
		oht_assert_threadmodel(ThrMdl_Single);
		uint16 nCountChannels;
		input.read(&nCountChannels);

		for (uint16 c = 0; c < nCountChannels; ++c)
		{
			size_t nCountMWF;
			MetaFragment::Container * pMWF;
			Channel::Ident channel;

			input >> channel;
			input.read(&nCountMWF);

			for (size_t i = 0; i < nCountMWF; ++i)
			{
				pMWF = _pPage->getFactory().getVoxelFactory(channel) ->createMetaFragment();
				MetaFragment::Interfaces::Builder builder = pMWF->acquireBuilderInterface();
				MetaFragment::Interfaces::Unique unique = pMWF->acquireInterface();
				_pPage->fireOnBeforeLoadMetaRegion(channel, &unique);
				builder << input;
				attachFragment(channel, unique, pMWF);
			}
		}
		//OHT_DBGTRACE("read " << nCountMWF << " meta world fragments");

		return input;
	}

	void TerrainTile::updateVoxels()
	{
		for (Channel::Index< MetaFragMap >::iterator j = _index2mapMF.begin(); j != _index2mapMF.end(); ++j)
		{
			for (MetaFragMap::iterator i = j->value->begin(); i != j->value->end(); ++i)
			{
				MetaFragment::Interfaces::Unique fragment = i->second->acquireInterface();
				fragment.updateGrid();
			}
		}
	}

	void TerrainTile::commitOperation( const bool bUpdate /*= true*/ )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		// See if there are new fragments waiting to be attached to the rendering tree
		for (Channel::Index< DirtyMWFSet >::iterator j = _dirtyMF.begin(); j != _dirtyMF.end(); ++j)
		{
			for (DirtyMWFSet::iterator i = j->value->begin(); i != j->value->end(); ++i)
			{
				MetaFragment::Interfaces::Builder fragment = i->mwf->acquireBuilderInterface();
				MetaFragment::Interfaces::const_Basic basic = static_cast< const MetaFragment::Container * > (i->mwf) ->acquireBasicInterface();
				switch (i->status)
				{
				case DirtyMF::Dirty:
					if (!basic.isInitialized())
					{
						applyFragment(j->channel, basic, fragment);
						initialiseMWF(j->channel, _pPage->getSceneNode(), basic, fragment);
					}
					if (bUpdate)
					{
						i->mwf->acquireInterface().updateSurface();
					}
					break;
				}
			}

			_dirtyMF.erase(j);
		}
	}

	const OverhangTerrainManager & TerrainTile::getManager() const
	{
		return _pPage->getManager();
	}

	bool TerrainTile::isInitialised() const
	{
		oht_assert_threadmodel(ThrMdl_Main);
		return _bInit;
	}

	bool TerrainTile::isParameterized() const
	{
		return _bParameterized;
	}

	YLevel TerrainTile::computeYLevel( const Real z ) const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		return YLevel::fromYCoord(z, _options.getTileWorldSize());
	}

	void TerrainTile::linkUpAllSurfaces()
	{
		oht_assert_threadmodel(ThrMdl_Background);
		OgreAssert(_pPage->getPublic()->getSceneNode() == NULL, "Cannot link-up child surfaces of page already set in scene");

		const size_t n = _options.getTilesPerPage() - 1;

		if (p < n)
			linkNeighbor(VonN_EAST, _pPage->getTerrainTile(p + 1, q));
		if (q < n)
			linkNeighbor(VonN_SOUTH, _pPage->getTerrainTile(p, q + 1));

		for (Channel::Index< MetaFragMap >::iterator k = _index2mapMF.begin(); k != _index2mapMF.end(); ++k)
		{
			MetaFragMap::iterator
				i0 = k->value->begin(),
				i1 = i0;

			if (i0 != k->value->end())
			{
				while (++i1 != k->value->end())
				{
					if (DIFF(i1->first, i0->first) == 1)
					{
						auto fragment = i1->second->acquire< MetaFragment::Interfaces::Builder >();

						fragment.linkNeighbor(OrthoN_BELOW, i0->second);
					}
					++i0;
				}
			}
		}
	}

	void TerrainTile::detachFromScene()
	{
		oht_assert_threadmodel(ThrMdl_Main);

		for (Channel::Index< MetaFragMap >::iterator j = _index2mapMF.begin(); j != _index2mapMF.end(); ++j)
		{
			for (MetaFragMap::iterator i = j->value->begin(); i != j->value->end(); ++i)
			{
				MetaFragment::Interfaces::Builder fragment = i->second->acquireBuilderInterface();
				fragment.detachFromScene();
			}
		}
	}

	void TerrainTile::linkNeighbor( const VonNeumannNeighbor ennNeighbor, TerrainTile * pNeighborTile )
	{
		oht_assert_threadmodel(ThrMdl_Single);

		for (Channel::Index< MetaFragMap >::iterator x = _index2mapMF.begin(); x != _index2mapMF.end(); ++x)
		{
			Channel::Index< MetaFragMap >::iterator y = pNeighborTile->_index2mapMF.find(x->channel);

			if (y != pNeighborTile->_index2mapMF.end())
			{
				MetaFragMap::iterator
					i = x->value->begin(),
					j = y->value->begin();

				while (i != x->value->end() && j != y->value->end())
				{
					while(i->first < j->first)
						++i;
					while(j->first < i->first)
						++j;

					auto fragment = i->second->acquire< MetaFragment::Interfaces::Builder > ();

					fragment.linkNeighbor((OrthogonalNeighbor)ennNeighbor, j->second);

					++i;
					++j;
				}
			}
		}
	}

	void TerrainTile::linkNeighbor(const VonNeumannNeighbor ennNeighbor, const Channel::Ident channel, MetaFragMap::iterator i)
	{
		oht_assert_threadmodel(ThrMdl_Main);

		Channel::Index< MetaFragMap >::iterator k = _index2mapMF.find(channel);
		if (k != _index2mapMF.end())
		{
			MetaFragMap::iterator j = k->value->find(i->first);

			if (j != k->value->end())
			{
				auto fragment = j->second->acquire< MetaFragment::Interfaces::Builder > ();
				fragment.linkNeighbor((OrthogonalNeighbor)ennNeighbor, i->second);
			}
		}
	}

	void TerrainTile::unlinkPageNeighbor( const VonNeumannNeighbor ennNeighbor )
	{
		oht_assert_threadmodel(ThrMdl_Main);
		
		for (Channel::Index< MetaFragMap >::iterator k = _index2mapMF.begin(); k != _index2mapMF.end(); ++k)
		{
			for (MetaFragMap::iterator i = k->value->begin(); i != k->value->end(); ++i)
			{
				auto fragment = i->second->acquire< MetaFragment::Interfaces::Builder > ();
				fragment.unlinkNeighbor(ennNeighbor);
			}
		}
	}

	TerrainTile::DirtyMF::DirtyMF( MetaFragment::Container * pMWF, const Status enStatus ) 
	: mwf(pMWF), _immutable(new MetaFragment::Interfaces::Basic(pMWF->acquireBasicInterface())), status(enStatus)
	{

	}

	TerrainTile::DirtyMF::DirtyMF( DirtyMF && move ) 
	: _immutable(move._immutable), mwf(move.mwf), status(move.status)
	{
		move._immutable = NULL;
	}

	TerrainTile::DirtyMF::DirtyMF( const DirtyMF & copy )
	: _immutable(new MetaFragment::Interfaces::Basic(copy.mwf->acquireBasicInterface())), mwf(copy.mwf), status(copy.status)
	{

	}

	bool TerrainTile::DirtyMF::operator<( const DirtyMF & other ) const
	{
		return _immutable->ylevel < other._immutable->ylevel;
	}

	TerrainTile::DirtyMF::~DirtyMF()
	{
		delete _immutable;
	}


	TerrainTile::ChannelProperties::ChannelProperties()
		: renderq(RENDER_QUEUE_MAIN) {}

}///namespace Ogre
