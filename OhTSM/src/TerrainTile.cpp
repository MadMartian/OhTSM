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
	TerrainTile::TerrainTile(const size_t p, const size_t q, PagePrivateNonthreaded * const page, const OverhangTerrainOptions & opts)
	: p(p), q(q), 
	borders(
		getTouch2DSide(
			getTouchStatus(p, 0, opts.getTilesPerPage() - 1), 
			getTouchStatus(q, 0, opts.getTilesPerPage() - 1)
		)
	),
	_psetDirtyMF(NULL), _options(opts), _pPage(page), _pMetaHeightmap(NULL), _bInit(false), _bParameterized(false)
	{
		_pMetaHeightmap = new MetaHeightMap(this);

		for ( int i = 0; i < CountVonNeumannNeighbors; i++ )
			_vpInternalNeighbors[ i ] = NULL;

		for (int i = 0; i < NumOCS; ++i)
			_vpBBox[i] = NULL;
	}

	TerrainTile::~TerrainTile()
	{
		oht_assert_threadmodel(ThrMdl_Single);

		OHT_DBGTRACE("Delete MWF: " << _mapMF.size());

		for(MetaFragMap::iterator it = _mapMF.begin(); it != _mapMF.end(); ++it)
		{
			{
				MetaFragment::Interfaces::Unique fragment = it->second->acquireInterface();
				_pPage->fireOnDestroyMetaRegion(&fragment);
			}
			delete it->second;
		}

		delete _psetDirtyMF;
		delete _pMetaHeightmap;

		for (int i = 0; i < NumOCS; ++i)
			delete _vpBBox[i];
	}

	void TerrainTile::initNeighbor(const VonNeumannNeighbor n, TerrainTile * t )
	{
		oht_assert_threadmodel(ThrMdl_Single);
		OgreAssert(_vpInternalNeighbors[n] == NULL, "Neighbor already initialized");
		_vpInternalNeighbors[n] = t;
	}

	void TerrainTile::setRenderQueueGroup(uint8 qid)
	{
		oht_assert_threadmodel(ThrMdl_Main);
		for(MetaFragMap::iterator i = _mapMF.begin(); i != _mapMF.end(); ++i)
		{
			i->second->acquireBasicInterface().surface->setRenderQueueGroup(qid);
		}
	}

	void TerrainTile::operator << ( const PageInitParams::TileParams & params )
	{
		oht_assert_threadmodel(ThrMdl_Background);

		//calculate min and max heights;
		Real 
			min = std::numeric_limits< Real >::max(),
			max = -min;

		const int 
			endx = params.vx0 + _options.tileSize,
			endz = params.vy0 + _options.tileSize;

		_x0 = params.vx0;
		_y0 = params.vy0;
			
		for ( int j = params.vy0; j < endz; j++ )
		{
			for ( int i = params.vx0; i < endx; i++ )
			{
				Real height = params.heightmap[j * _options.pageSize + i];
				height = height * _options.heightScale; // mOptions->scale height 

				if ( height < min )
					min = ( Real ) height;

				if ( height > max )
					max = ( Real ) height;
			}
		}

		const Vector3 vecOffsetHalf = Vector3(1, 0, 1) * (_options.getPageWorldSize() / 2);
		_vHMBBox[OCS_World].setExtents(
			( Real ) params.vx0 * _options.cellScale - vecOffsetHalf.x, 
			min, 
			( Real ) params.vy0 * _options.cellScale - vecOffsetHalf.z,
			( Real ) ( endx - 1 ) * _options.cellScale - vecOffsetHalf.x, 
			max,
			( Real ) ( endz - 1 ) * _options.cellScale - vecOffsetHalf.z
		);
		_vHMBBox[OCS_Vertex] = 
		_vHMBBox[OCS_DataGrid] = 
		_vHMBBox[OCS_PagingStrategy] = 
		_vHMBBox[OCS_Terrain] = _vHMBBox[OCS_World];
		OverhangTerrainManager::transformSpace(OCS_World, _options.alignment, OCS_Terrain, _vHMBBox[OCS_Terrain]);
		OverhangTerrainManager::transformSpace(OCS_World, _options.alignment, OCS_PagingStrategy, _vHMBBox[OCS_PagingStrategy]);
		OverhangTerrainManager::transformSpace(OCS_World, _options.alignment, OCS_Vertex, _vHMBBox[OCS_Vertex], _options.cellScale);
		OverhangTerrainManager::transformSpace(OCS_World, _options.alignment, OCS_DataGrid, _vHMBBox[OCS_DataGrid], _options.cellScale);
	
		_vCenterPos[OCS_World] = Vector3( 
			( params.vx0 * _options.cellScale + (endx - 1) * _options.cellScale ) / 2 - vecOffsetHalf.x,
			0,
			( params.vy0 * _options.cellScale + (endz - 1) * _options.cellScale ) / 2 - vecOffsetHalf.z 
		);
		_vCenterPos[OCS_PagingStrategy] = 
			_vCenterPos[OCS_Terrain] = 
			_vCenterPos[OCS_Vertex] = 
			_vCenterPos[OCS_DataGrid] = 
			_vCenterPos[OCS_World];
		OverhangTerrainManager::transformSpace(OCS_World, _options.alignment, OCS_Vertex, _vCenterPos[OCS_Vertex], _options.cellScale);
		OverhangTerrainManager::transformSpace(OCS_World, _options.alignment, OCS_DataGrid, _vCenterPos[OCS_DataGrid], _options.cellScale);
		OverhangTerrainManager::transformSpace(OCS_World, _options.alignment, OCS_PagingStrategy, _vCenterPos[OCS_PagingStrategy]);
		OverhangTerrainManager::transformSpace(OCS_World, _options.alignment, OCS_Terrain, _vCenterPos[OCS_Terrain]);

		_bParameterized = true;
	}

	void TerrainTile::initialise(SceneNode * pParentSceneNode)
	{
		oht_assert_threadmodel(ThrMdl_Main);

		for (MetaFragMap::iterator i = _mapMF.begin(); i != _mapMF.end(); ++i)
		{
			MetaFragment::Container * pMWF = i->second;
			const MetaFragment::Container * pConstMWF = i->second;
			initialiseMWF(pParentSceneNode, pConstMWF->acquireBasicInterface(), pMWF->acquireBuilderInterface());
		}

		_bInit = true;
	}

	void TerrainTile::voxelise()
	{
		oht_assert_threadmodel(ThrMdl_Background);

		_pMetaHeightmap->updatePosition();

		const YLevel
			ylmb0 = computeYLevel(_vHMBBox[OCS_Terrain].getMinimum().z-1),
			ylmbN = computeYLevel(_vHMBBox[OCS_Terrain].getMaximum().z+1);

		for (YLevel yli = ylmb0; yli <= ylmbN; ++yli)
			acquireMetaWorldFragment(yli);
	}

	void TerrainTile::initialiseMWF( SceneNode * pParentSceneNode, MetaFragment::Interfaces::const_Basic & basic, MetaFragment::Interfaces::Builder & builder )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		StringUtil::StrStreamType 
			ssSceneNodeName, 
			ssISRName;

		ssSceneNodeName << "MWF[" << p << ',' << q << ';' << basic.ylevel << "] (" << _pPage->getPageX() << 'x' << _pPage->getPageY() << ')';
		Vector3 ptChildPos = getYLevelPosition(basic.ylevel, OCS_Terrain) + _pPage->getManager().options.getTileWorldSize() / 2.0f;
		_pPage->getManager().transformSpace(OCS_Terrain, OCS_World, ptChildPos);

		OHT_DBGTRACE("Create Scene Node at " << ptChildPos << " relative to " << pParentSceneNode->getPosition());
		SceneNode * pScNode = pParentSceneNode->createChildSceneNode(ssSceneNodeName.str(), ptChildPos);

		ssISRName << "ISR[" << p << ',' << q << ';' << basic.ylevel << "] (" << _pPage->getPageX() << 'x' << _pPage->getPageY() << ')';

		builder.initialise(_options.primaryCamera, pScNode, ssISRName.str());
		builder.setMaterial(_pMaterial);
		_pPage->fireOnInitMetaRegion(&basic, &builder);
	}

	void TerrainTile::setMaterial(const MaterialPtr& m )
	{
		oht_assert_threadmodel(ThrMdl_Main);
		for(MetaFragMap::iterator i = _mapMF.begin(); i != _mapMF.end(); ++i)
		{
			MetaFragment::Interfaces::Builder fragment = i->second->acquireBuilderInterface();
			fragment.setMaterial(m);
		}
		_pMaterial = m;
	}

	const AxisAlignedBox& TerrainTile::getHeightMapBBox( const OverhangCoordinateSpace encsTo /*= OCS_World*/ ) const
	{
		return _vHMBBox[encsTo];
	}

	const AxisAlignedBox& TerrainTile::getBBox(const OverhangCoordinateSpace encsTo /*= OCS_World*/) const
	{
		oht_assert_threadmodel(ThrMdl_Single);

		const OverhangTerrainManager & manager = _pPage->getManager();

		if (_vpBBox[encsTo] == NULL)
		{
			OgreAssert(isParameterized(), "Cannot call getBBox before TT has been parameterized");

			if (_vpBBox[OCS_Terrain] == NULL)
			{
				YLevel	nYLevel0 = YLevel::MAX, 
						nYLevel1 = YLevel::MIN;

				for (MetaFragMap::const_iterator i = _mapMF.begin(); i != _mapMF.end(); ++i)
				{
					if (i->first < nYLevel0)
						nYLevel0 = i->first;
					if (i->first > nYLevel1)
						nYLevel1 = i->first;
				}

				const Real 
					z1 = manager.toSpace(nYLevel1 + 1, OCS_Terrain).z,
					z0 = manager.toSpace(nYLevel0, OCS_Terrain).z;

				// TODO: Is this the most efficient / best way of doing this?
				AxisAlignedBox * pbbox =
					_vpBBox[OCS_Terrain] = 
					new AxisAlignedBox(_vHMBBox[OCS_Terrain]);

				if (z1 > pbbox->getMaximum().z)
					pbbox->setMaximumZ(z1);
				if (z0 < pbbox->getMinimum().z)
					pbbox->setMinimumZ(z0);
			}

			if (encsTo != OCS_Terrain)
				_vpBBox[encsTo] = new AxisAlignedBox(_pPage->getManager().toSpace(OCS_Terrain, encsTo, *_vpBBox[OCS_Terrain]));
		}
		return *_vpBBox[encsTo];
	}

	const Vector3& TerrainTile::getCenter( const OverhangCoordinateSpace encsTo /*= OCS_World*/ ) const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		return _vCenterPos[encsTo];
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
		tPos -= getPosition(OCS_Terrain);

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

	bool TerrainTile::rayIntersects(OverhangTerrainManager::RayResult & result, const Ray& rayPageRelTerrainSpace, bool bCascade /* = false */, Real nLimit /* = 0 */) const
	{
		oht_assert_threadmodel(ThrMdl_Main);
		std::pair<bool, Real> intersection;

		result.hit = false;
		
		intersection = rayPageRelTerrainSpace.intersects(getBBox(OCS_Terrain));
		OHT_DBGTRACE(
			"\t\tTile:" 
			<< AxisAlignedBox(
				getBBox().getMinimum() + _pPage->getPublic()->getPosition(), 
				getBBox().getMaximum() + _pPage->getPublic()->getPosition()
			) 
			<< " intersects: " 
			<< (intersection.first ? "true" : "false")
		);
		if (!intersection.first)
		{
			if (bCascade)
			{
				const TerrainTile* pNeighbor = raySelectNeighbour(rayPageRelTerrainSpace, nLimit);
				if (pNeighbor != NULL)
					return pNeighbor->rayIntersects(result, rayPageRelTerrainSpace, bCascade, nLimit);
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
			OHTDD_Translate(-getPosition());
			OHTDD_Coords(OCS_World, OCS_Terrain);

			OHTDD_Color(DebugDisplay::MC_Blue);
			OHTDD_Point(rayPageRelTerrainSpace.getPoint(intersection.second));
			intersection = rayIntersectsMetaWorld(result, rayPageRelTerrainSpace, intersection.second, nLimit);
		}
			
		// Only traverse this test when there have been meta-ball intersections and at least a terrain quad intersection unless we are underground.
		// The case is skipped is whenever the ray passes through this tile entirely above ground (original terrain-renderable height, not meta)

		if (!intersection.first && bCascade)
		{
			const TerrainTile * pNeighbor = raySelectNeighbour(rayPageRelTerrainSpace, nLimit);
			if (pNeighbor != NULL)
				return pNeighbor->rayIntersects(result, rayPageRelTerrainSpace, bCascade, nLimit);
		}

		result.hit = intersection.first;
		result.position = rayPageRelTerrainSpace.getPoint(intersection.second);

#ifdef _DEBUG
		if (result.hit)
			OHT_DBGTRACE("\t\tResult: " << (_pPage->getManager().toSpace(OCS_Terrain, OCS_World, result.position) + _pPage->getPublic()->getPosition()));
#endif // _DEBUG

		return result.hit;
	}

	std::pair< bool, Real > TerrainTile::rayIntersectsMetaWorld( OverhangTerrainManager::RayResult & result, const Ray & rayPageRelTerrainSpace, const Real tile, const Real nLimit ) const
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
		const Vector3 ptOriginCube = getYLevelPosition(yl0, OCS_DataGrid);

		// Translate the intersection point so it's relative to the first would-be meta-cube
		origin -= ptOriginCube;

		OHTDD_Coords(OCS_Terrain, OCS_DataGrid);
		OHTDD_Translate(-ptOriginCube);
		OHTDD_Translate(getBBox(OCS_DataGrid).getMinimum());
		OHTDD_Color(DebugDisplay::MC_Yellow);
		
		// Clamp intersection point start to boundaries
		origin.makeFloor(getBBox(OCS_DataGrid).getSize() - 1.0f / (Real)Voxel::FS_Span);
		origin.makeCeil(Vector3::ZERO);

		const int nCellsPerCube = (int)_options.tileSize-1;

		// Create a ray in data-grid space with origin relative to the first would-be meta-cube intersected
		const Ray rayTileCubeIntersectRelDataGridSpace(origin, direction);
		RayCellWalk walker(rayTileCubeIntersectRelDataGridSpace.getOrigin(), rayTileCubeIntersectRelDataGridSpace.getDirection(), nLimit / _options.cellScale); 

		walker.lod = 0;
		while (
			!walker.intersection &&
				walker && 
				walker.wcell.i >=0 && walker.wcell.i < nCellsPerCube && 
				walker.wcell.k >=0 && walker.wcell.k < nCellsPerCube
		)
		{
			const YLevel yl = YLevel::fromCell(walker.wcell.j, nCellsPerCube) + yl0;	// DEPS: Y-Level computation
			MetaFragMap::const_iterator iMWF = _mapMF.find(yl);	// TODO: Cache this result because yLevel changes very infrequently

			OHT_DBGTRACE(yl);
			if (iMWF != _mapMF.end())
			{
				MetaFragment::Interfaces::Unique fragment = iMWF->second->acquireInterface();
				
				const Vector3
					ptYLevelPos = getYLevelPosition(yl, OCS_DataGrid),
					ptRel = ptOriginCube - ptYLevelPos;

				OHT_DBGTRACE(
					"\t\t\t\tFragment: " <<
					(
						_pPage->getManager().toSpace(
							OCS_DataGrid, OCS_World,
							Vector3(
								(Real)walker.wcell.i, 
								(Real)walker.wcell.j, 
								(Real)walker.wcell.k
							) + ptOriginCube + getBBox(OCS_DataGrid).getMinimum()
						) + _pPage->getPublic()->getPosition()
					)
					<< ", Y-Level " << yl
					<< ", local ray spot " << _pPage->getManager().toSpace(OCS_DataGrid, OCS_World, rayTileCubeIntersectRelDataGridSpace.getPoint(ptRel.length()) + ptOriginCube)
				);

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
				const Vector3 vwc((Real)walker.wcell.i, (Real)walker.wcell.j, (Real)walker.wcell.k);
				const AxisAlignedBox bboxCell(vwc, vwc + 1);
				OHTDD_Cube(bboxCell);

				++walker;
			}
		}
		result.hit = walker.intersection;

		return std::pair< bool, Real > (walker.intersection, walker.distance * _options.cellScale + tile);
	}

	MetaFragment::Container * TerrainTile::acquireMetaWorldFragment( const YLevel yl )
	{
		oht_assert_threadmodel(ThrMdl_Single);
		MetaFragMap::iterator iMWF = _mapMF.find(yl);

		if (iMWF != _mapMF.end())
			return iMWF->second;
		else
		{
			Vector3 pos = getYLevelPosition(yl, OCS_World);
			MetaFragment::Container * pMWF = _pPage->getFactory().createMetaFragment(pos, yl);
			MetaFragment::Interfaces::Unique fragment = pMWF->acquireInterface();

			_pPage->getManager().transformSpace(OCS_World, OCS_Vertex, pos);

			// TODO: Threading - Ensure synchronized, revise contract to require client-imposed synchronization and no OGRE calls
			_pPage->fireOnCreateMetaRegion(fragment.block, &fragment, pos);

			attachFragment(fragment, pMWF);
			return pMWF;
		}
	}
	
	void TerrainTile::propagateMetaObject( DirtyMWFSet & queues, MetaObject * const pMetaObj )
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

			pMWF = acquireMetaWorldFragment(yli);
			MetaFragment::Interfaces::Unique fragment = pMWF->acquireInterface();
			fragment.addMetaObject(pMetaObj);
			fragment.updateGrid();

			queues.insert(DirtyMF(pMWF, DirtyMF::Dirty));
		}
	}

	MetaFragMap::iterator TerrainTile::attachFragment( MetaFragment::Interfaces::Unique & fragment, MetaFragment::Container * pMWF )
	{
		oht_assert_threadmodel(ThrMdl_Single);
		OgreAssert(_mapMF.find(fragment.ylevel) == _mapMF.end(), "MetaWorldFragment already exists in this terrain tile");

		fragment.addMetaObject(getMetaHeightMap());
		return _mapMF.insert(_mapMF.begin(), MetaFragMap::value_type (fragment.ylevel, pMWF));
	}

	void TerrainTile::applyFragment( MetaFragment::Interfaces::const_Basic & basic, MetaFragment::Interfaces::Builder & builder )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		OgreAssert(!basic.isInitialized(), "Expected uninitialized fragment");

		MetaFragMap::iterator i, j, k;
		i = j = k = _mapMF.find(basic.ylevel);

		OgreAssert(j != _mapMF.end(), "Could not find MWF in map");
		--i;
		++k;
		OgreAssert(
			(i == _mapMF.end() || i->first < j->first) && 
			(k == _mapMF.end() || j->first < k->first), 
			"Order assumption violated"
		);

		if (i != _mapMF.end() && DIFF(j->first, i->first) == 1)
			builder.linkNeighbor(OrthoN_BELOW, i->second);
		if (k != _mapMF.end() && DIFF(j->first, k->first) == -1)
			builder.linkNeighbor(OrthoN_ABOVE, k->second);

		_pPage->linkFragmentHorizontalInternal(this, j);
	}

	void TerrainTile::addMetaBall( MetaBall * const pMetaBall)
	{
		oht_assert_threadmodel(ThrMdl_Single);
		const AxisAlignedBox bboxMetaObjectTileSpace = _pPage->getManager().toSpace(OCS_World, OCS_Terrain, pMetaBall->getAABB());
		const Real nTWS = static_cast< Real > (_pPage->getManager().options.getTileWorldSize());

		// Resets cached data dependent on meta-objects
		dirty();

		DirtyMWFSet queues;

		propagateMetaObject (queues, pMetaBall);

		if (!queues.empty())
		{
			if (_psetDirtyMF == NULL)
				_psetDirtyMF = new DirtyMWFSet();

			_psetDirtyMF->insert(queues.begin(), queues.end());
		}
	}

	void TerrainTile::loadMetaObject( MetaObject * const pMetaObject )
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

			pMWF = acquireMetaWorldFragment(yli);
			MetaFragment::Interfaces::Unique fragment = pMWF->acquireInterface();
			fragment.addMetaObject(pMetaObject);
		}
	}

	Ogre::Vector3 TerrainTile::getYLevelPosition( const YLevel yl, const OverhangCoordinateSpace encsTo /*= OCS_Terrain*/ ) const
	{
		oht_assert_threadmodel(ThrMdl_Single);

		Vector3 ptResult = getPosition(OCS_Terrain);
		ptResult.z = yl.toYCoord(_pPage->getManager().options.getTileWorldSize());
		_pPage->getManager().transformSpace(OCS_Terrain, encsTo, ptResult);
		return ptResult;
	}

	MetaFragMap::const_iterator TerrainTile::beginFrags() const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		return _mapMF.begin();
	}

	MetaFragMap::const_iterator TerrainTile::endFrags() const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		return _mapMF.end();
	}

	void TerrainTile::dirty()
	{
		oht_assert_threadmodel(ThrMdl_Single);
		for (int i = 0; i < NumOCS; ++i)
		{
			delete _vpBBox[i];
			_vpBBox[i] = NULL;
		}
	}
	StreamSerialiser & TerrainTile::operator >> (StreamSerialiser & output) const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		const size_t nCountMWF = _mapMF.size();

		output.write(&nCountMWF);
		if (nCountMWF > 0)
			*_pMetaHeightmap >> output;
		for (MetaFragMap::const_iterator i = beginFrags(); i != endFrags(); ++i)
		{
			const MetaFragment::Container * pMWF = i->second;
			MetaFragment::Interfaces::Shared fragment = pMWF->acquireInterface();
			fragment >> output;
		}

		//OHT_DBGTRACE("wrote " << nCountMWF << " meta world fragments");
		return output;
	}
	StreamSerialiser & TerrainTile::operator << (StreamSerialiser & input)
	{
		oht_assert_threadmodel(ThrMdl_Single);
		size_t nCountMWF;
		MetaFragment::Container * pMWF;

		input.read(&nCountMWF);
		if (nCountMWF > 0)
			*getMetaHeightMap() << input;
			
		for (size_t i = 0; i < nCountMWF; ++i)
		{
			pMWF = _pPage->getFactory().createMetaFragment();
			MetaFragment::Interfaces::Builder builder = pMWF->acquireBuilderInterface();
			MetaFragment::Interfaces::Unique unique = pMWF->acquireInterface();
			_pPage->fireOnBeforeLoadMetaRegion(&unique);
			builder << input;
			attachFragment(unique, pMWF);
		}
		//OHT_DBGTRACE("read " << nCountMWF << " meta world fragments");

		return input;
	}

	void TerrainTile::updateVoxels()
	{
		for (MetaFragMap::iterator i = _mapMF.begin(); i != _mapMF.end(); ++i)
		{
			MetaFragment::Interfaces::Unique fragment = i->second->acquireInterface();
			fragment.updateGrid();
		}
	}

	void TerrainTile::commitOperation( const bool bUpdate /*= true*/ )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		// See if there are new fragments waiting to be attached to the rendering tree
		if (_psetDirtyMF != NULL)
		{
			for (DirtyMWFSet::iterator i = _psetDirtyMF->begin(); i != _psetDirtyMF->end(); ++i)
			{
				MetaFragment::Interfaces::Builder fragment = i->mwf->acquireBuilderInterface();
				MetaFragment::Interfaces::const_Basic basic = static_cast< const MetaFragment::Container * > (i->mwf) ->acquireBasicInterface();
				switch (i->status)
				{
				case DirtyMF::Dirty:
					if (!basic.isInitialized())
					{
						applyFragment(basic, fragment);
						initialiseMWF(_pPage->getSceneNode(), basic, fragment);
					}
					if (bUpdate)
					{
						i->mwf->acquireInterface().updateSurface();
					}
					break;
				}
			}

			delete _psetDirtyMF;
			_psetDirtyMF = NULL;
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

	const Vector3& TerrainTile::getPosition( const OverhangCoordinateSpace encsTo /*= OCS_World*/ ) const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		return getBBox(encsTo).getMinimum();
	}

	YLevel TerrainTile::computeYLevel( const Real z ) const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		return YLevel::fromYCoord(z, _options.getTileWorldSize());
	}

	Ogre::Real TerrainTile::height( const signed int x, const signed int y ) const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		using bitmanip::clamp;
		const signed int nN = _options.pageSize - 1;
		return _pPage->getPublic()->height(
			clamp(0, nN, x + (signed int)_x0),
			clamp(0, nN, y + (signed int)_y0)
		);
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

		MetaFragMap::iterator 
			i0 = _mapMF.begin(),
			i1 = i0;

		if (i0 != _mapMF.end())
		{
			while (++i1 != _mapMF.end())
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

	void TerrainTile::detachFromScene()
	{
		oht_assert_threadmodel(ThrMdl_Main);

		for (MetaFragMap::iterator i = _mapMF.begin(); i != _mapMF.end(); ++i)
		{
			MetaFragment::Interfaces::Builder fragment = i->second->acquireBuilderInterface();
			fragment.detachFromScene();
		}
	}

	void TerrainTile::linkNeighbor( const VonNeumannNeighbor ennNeighbor, TerrainTile * pNeighborTile )
	{
		oht_assert_threadmodel(ThrMdl_Single);

		MetaFragMap::iterator 
			i = _mapMF.begin(), 
			j = pNeighborTile->_mapMF.begin();

		while (i != _mapMF.end() && j != pNeighborTile->_mapMF.end())
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

	void TerrainTile::linkNeighbor(const VonNeumannNeighbor ennNeighbor, MetaFragMap::iterator i)
	{
		oht_assert_threadmodel(ThrMdl_Main);

		MetaFragMap::iterator j = _mapMF.find(i->first);

		if (j != _mapMF.end())
		{
			auto fragment = j->second->acquire< MetaFragment::Interfaces::Builder > ();
			fragment.linkNeighbor((OrthogonalNeighbor)ennNeighbor, i->second);
		}
	}

	void TerrainTile::unlinkPageNeighbor( const VonNeumannNeighbor ennNeighbor )
	{
		oht_assert_threadmodel(ThrMdl_Main);
		
		for (MetaFragMap::iterator i = _mapMF.begin(); i != _mapMF.end(); ++i)
		{
			auto fragment = i->second->acquire< MetaFragment::Interfaces::Builder > ();
			fragment.unlinkNeighbor(ennNeighbor);
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

}///namespace Ogre
