/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2006 Torus Knot Software Ltd
Also see acknowledgments in Readme.html

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

You may alternatively use this source under the terms of a specific version of
the OGRE Unrestricted License provided you have obtained such a license from
Torus Knot Software Ltd.

Hacked by Martin Enge (martin.enge@gmail.com) 2007 to fit into the OverhangTerrain Scene Manager
Modified (2013) by Jonathan Neufeld (http://www.extollit.com) to implement Transvoxel
Transvoxel conceived by Eric Lengyel (http://www.terathon.com/voxels/)

-----------------------------------------------------------------------------
*/

#include "pch.h"

#include <OgreAxisAlignedBox.h>

#include "PageSection.h"
#include "TerrainTile.h"
#include "IsoSurfaceRenderable.h"
#include "MetaWorldFragment.h"
#include "MetaBall.h"
#include "MetaHeightMap.h"
#include "OverhangTerrainManager.h"
#include "Util.h"
#include "DebugTools.h"

namespace Ogre
{
	const uint32 PageSection::CHUNK_ID = StreamSerialiser::makeIdentifier("OHPS");
	const uint16 PageSection::VERSION = 1;

    //-------------------------------------------------------------------------
    PageSection::PageSection(const OverhangTerrainManager * mgr, MetaFactory * const pMetaFactory)
		: manager(mgr), _nTileCount(mgr->options.getTilesPerPage()), _pFactory(pMetaFactory),
		 _pScNode(NULL), _bDirty(false), _pPrivate(NULL), _vHeightmap(NULL)
    {
		oht_assert_threadmodel(ThrMdl_Main);

		_pPrivate = new PagePrivateNonthreaded(this);

		_vpNeighbors[VonN_NORTH] =
		_vpNeighbors[VonN_SOUTH] =
		_vpNeighbors[VonN_EAST] =
		_vpNeighbors[VonN_WEST] = NULL;

		// Set up an empty array of TerrainTile pointers
        size_t i, j;
        for ( i = 0; i < _nTileCount; i++ )
        {
            _vTiles.push_back( TerrainRow() );

            for ( j = 0; j < _nTileCount; j++ )
            {

				// Create scene node for the tile and the TerrainRenderable
                _vTiles[ i ].push_back( OGRE_NEW TerrainTile(i,j, _pPrivate, manager->options) );
            }
        }

        for ( size_t j = 0; j < _nTileCount; j++ )
        {
            for ( size_t i = 0; i < _nTileCount; i++ )
            {
                if ( j != _nTileCount - 1 )
                {
                    _vTiles[ i ][ j ] -> initNeighbor( VonN_SOUTH, _vTiles[ i ][ j + 1 ] );
                    _vTiles[ i ][ j + 1 ] -> initNeighbor( VonN_NORTH, _vTiles[ i ][ j ] );
                }

                if ( i != _nTileCount - 1 )
                {
                    _vTiles[ i ][ j ] -> initNeighbor( VonN_EAST, _vTiles[ i + 1 ][ j ] );
                    _vTiles[ i + 1 ][ j ] -> initNeighbor( VonN_WEST, _vTiles[ i ][ j ] );
                }
            }
        }
    }
    //-------------------------------------------------------------------------
    PageSection::~PageSection()
    {
		OHT_DBGTRACE("Delete Page " << this);
		oht_assert_threadmodel(ThrMdl_Single);

		// TODO: Fails when calling removeAllTerrains(), compensate, assertion is invalid
		OgreAssert(_pScNode == NULL, "Scene node must be detached first");

		typedef std::list< MetaObject * > MOList;
		MOList poplist;

		for (MetaObjectIterator i = iterateMetaObjects(); i; ++i)
			poplist.push_back(&(*i));
		for (MOList::iterator i = poplist.begin(); i != poplist.end(); ++i)
			delete *i;

		Terrain2D::iterator i, iend;
		iend = _vTiles.end();
		for (i = _vTiles.begin(); i != iend; ++i)
		{
			TerrainRow::iterator j, jend;
			jend = i->end();
			for (j = i->begin(); j != jend; ++j)
			{
				OGRE_DELETE *j;
				*j = NULL;
			}
		}


		// TODO: Deconstruct multi-referenced meta objects

		delete _pPrivate;
		delete [] _vHeightmap;
    }

	void PageSection::initialise( SceneNode * const pScNode )
	{
		oht_assert_threadmodel(ThrMdl_Background);

		OHT_DBGTRACE("Beginning Initialization for page");
		OgreAssert(_pScNode == NULL, "Cannot initialize more than once");
		_pScNode = pScNode;
		
		const size_t nTPP = manager->options.getTilesPerPage();
		SceneManager * const pSceneMgr = _pScNode->getCreator();

		for (Terrain2D::iterator j = _vTiles.begin(); j != _vTiles.end(); ++j)
			for (TerrainRow::iterator i = j->begin(); i != j->end(); ++i)
				(*i)->initialise(_pScNode);

		updateBBox();

		OHT_DBGTRACE("Initialization complete");
	}

	void PageSection::operator << (const PageInitParams & params)
	{
		oht_assert_threadmodel(ThrMdl_Background);

		OgreAssert(_vHeightmap == NULL, "Cannot load params more than once");

		_vHeightmap = new Real[params.countVerticesPerPage];
		memcpy(_vHeightmap, params.heightmap, params.countVerticesPerPage * sizeof(*_vHeightmap));

		_x = params.pageX;
		_y = params.pageY;

		for ( size_t p = 0; p < _vTiles.size(); ++p )
			for ( size_t q = 0; q < _vTiles[p].size(); ++q )
				*_vTiles[p][q] << params.getTile(p, q);
	}
	
	void PageSection::conjoin()
	{
		oht_assert_threadmodel(ThrMdl_Background);

		for (Terrain2D::iterator j = _vTiles.begin(); j != _vTiles.end(); ++j)
			for (TerrainRow::iterator i = j->begin(); i != j->end(); ++i)
				(*i)->voxelise();

		for (Terrain2D::iterator j = _vTiles.begin(); j != _vTiles.end(); ++j)
			for (TerrainRow::iterator i = j->begin(); i != j->end(); ++i)
				(*i)->linkUpAllSurfaces();

		for (Terrain2D::iterator j = _vTiles.begin(); j != _vTiles.end(); ++j)
			for (TerrainRow::iterator i = j->begin(); i != j->end(); ++i)
				(*i)->updateVoxels();
	}

	void PageSection::linkPageNeighbor( PageSection * const pPageNeighbor, const VonNeumannNeighbor ennNeighbor )
	{
		oht_assert_threadmodel(ThrMdl_Main);
		// TODO: lock(this) && lock(pPageNeighbor)

		_vpNeighbors[ennNeighbor] = pPageNeighbor;
		pPageNeighbor->_vpNeighbors[Neighborhood::opposite(ennNeighbor)] = this;

		//OHT_DBGTRACE("Link page[" << mX << ',' << mY << "] with page [" << pPageNeighbor->mX << ',' << pPageNeighbor->mY << "] to the " << neighborName(ennNeighbor));

		enum ClientHost
		{
			Host = 0,
			Client = 1
		};
		const size_t dN = _nTileCount - 1;
		size_t ij, ii, i0[2], j0[2];

		switch (ennNeighbor)
		{
		case VonN_NORTH:
			ii = 1;
			ij = 0;
			i0[Client] = i0[Host] = 0;
			j0[Host] = 0;
			j0[Client] = dN;

			break;
		case VonN_SOUTH:
			ii = 1;
			ij = 0;
			i0[Client] = i0[Host] = 0;
			j0[Host] = dN;
			j0[Client] = 0;

			break;
		case VonN_WEST:
			ii = 0;
			ij = 1;
			i0[Host] = 0;
			i0[Client] = dN;
			j0[Client] = j0[Host] = 0;

			break;
		case VonN_EAST:
			ii = 0;
			ij = 1;
			i0[Host] = dN;
			i0[Client] = 0;
			j0[Client] = j0[Host] = 0;

			break;
		}

		for (
			size_t 
			iHost = i0[Host], iClient = i0[Client], 
			jHost = j0[Host], jClient = j0[Client]; 

			iHost < _nTileCount && jHost < _nTileCount; 

			iHost += ii, iClient += ii,
			jHost += ij, jClient += ij
		)
		{
			_vTiles[iHost][jHost]->linkNeighbor(ennNeighbor, pPageNeighbor->_vTiles[iClient][jClient]);
		}
	}

	void PageSection::linkFragmentHorizontalInternal( TerrainTile * pHost, MetaFragMap::iterator i )
	{
		const size_t 
			p = pHost->p,
			q = pHost->q,

			n = _nTileCount - 1;

		if (q > 0)
		{
			TerrainTile * pTile = _vTiles[p][q - 1];
			pTile->linkNeighbor(VonN_SOUTH, i);
		}

		if (q < n)
		{
			TerrainTile * pTile = _vTiles[p][q + 1];
			pTile->linkNeighbor(VonN_NORTH, i);
		}

		if (p > 0)
		{
			TerrainTile * pTile = _vTiles[p - 1][q];
			pTile->linkNeighbor(VonN_EAST, i);
		}

		if (p < n)
		{
			TerrainTile * pTile = _vTiles[p + 1][q];
			pTile->linkNeighbor(VonN_WEST, i);
		}
	}

	void PageSection::unlinkPageNeighbor( const VonNeumannNeighbor ennNeighbor )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		//OHT_DBGTRACE("Unlink " << this << " from page to the " << neighborName(ennNeighbor));
	
		enum ClientHost
		{
			Host = 0,
			Client = 1
		};
		const size_t dN = _nTileCount - 1;
		size_t ij, ii, i0[2], j0[2];

		switch (ennNeighbor)
		{
		case VonN_NORTH:
			ii = 1;
			ij = 0;
			i0[Client] = i0[Host] = 0;
			j0[Host] = 0;
			j0[Client] = dN;

			break;
		case VonN_SOUTH:
			ii = 1;
			ij = 0;
			i0[Client] = i0[Host] = 0;
			j0[Host] = dN;
			j0[Client] = 0;

			break;
		case VonN_WEST:
			ii = 0;
			ij = 1;
			i0[Host] = 0;
			i0[Client] = dN;
			j0[Client] = j0[Host] = 0;

			break;
		case VonN_EAST:
			ii = 0;
			ij = 1;
			i0[Host] = dN;
			i0[Client] = 0;
			j0[Client] = j0[Host] = 0;

			break;
		}

		for (
			size_t 
			iHost = i0[Host], iClient = i0[Client], 
			jHost = j0[Host], jClient = j0[Client]; 

			iHost < _nTileCount && jHost < _nTileCount; 

			iHost += ii, iClient += ii,
			jHost += ij, jClient += ij
		)
		{
			_vTiles[iHost][jHost]->unlinkPageNeighbor(ennNeighbor);
		}

		_vpNeighbors[ennNeighbor] = _vpNeighbors[ennNeighbor] ->_vpNeighbors[Neighborhood::opposite(ennNeighbor)] = NULL;
	}

	void PageSection::unlinkPageNeighbors()
	{
		oht_assert_threadmodel(ThrMdl_Main);

		if (_vpNeighbors[VonN_NORTH] != NULL)
			unlinkPageNeighbor(VonN_NORTH);
		if (_vpNeighbors[VonN_SOUTH] != NULL)
			unlinkPageNeighbor(VonN_SOUTH);
		if (_vpNeighbors[VonN_EAST] != NULL)
			unlinkPageNeighbor(VonN_EAST);
		if (_vpNeighbors[VonN_WEST] != NULL)
			unlinkPageNeighbor(VonN_WEST);
	}

    //-------------------------------------------------------------------------
	TerrainTile * PageSection::getTerrainTile( const Vector3 & pt, const OverhangCoordinateSpace encsFrom /*= OCS_World */ ) const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		Vector3 ptTerrSpace = manager->toSpace(encsFrom, OCS_Terrain, pt);		
		ptTerrSpace /= manager->options.getTileWorldSize();
		ptTerrSpace += (Real)_nTileCount / 2;

		if (ptTerrSpace.x >= 0.0 && ptTerrSpace.x <= _nTileCount && ptTerrSpace.y >= 0.0 && ptTerrSpace.y <= _nTileCount)
			return _vTiles[(unsigned)ptTerrSpace.x][(unsigned)ptTerrSpace.y];
		else
			return NULL;
    }
	//-------------------------------------------------------------------------
	void PageSection::setRenderQueue(uint8 qid)
	{
		oht_assert_threadmodel(ThrMdl_Main);

		for ( size_t j = 0; j < _nTileCount; j++ )
		{
			for ( size_t i = 0; i < _nTileCount; i++ )
			{
				if ( j != _nTileCount - 1 )
				{
					_vTiles[ i ][ j ]->setRenderQueueGroup(qid);
				}
			}
		}
	}

	void PageSection::setPosition( const Vector3 & pt )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		OgreAssert(_pScNode != NULL, "The page has not been initialized");
		_pScNode->setPosition(pt);
		updateBBox();
	}

	Vector3 PageSection::getPosition() const
	{
		oht_assert_threadmodel(ThrMdl_Main);
	
		OgreAssert(_pScNode != NULL, "The page has not been initialized");
		return _pScNode->getPosition();
	}

	void PageSection::setMaterial( const MaterialPtr & m )
	{
		oht_assert_threadmodel(ThrMdl_Main);
		for ( size_t j = 0; j < _nTileCount; j++ )
		{
			for ( size_t i = 0; i < _nTileCount; i++ )
			{
				_vTiles[ i ][ j ]->setMaterial (m);
			}
		}
	}

	bool PageSection::rayIntersects(OverhangTerrainManager::RayResult & result, const Ray& ray, Real distanceLimit /*= 0*/ ) const
	{
		oht_assert_threadmodel(ThrMdl_Main);
		const Real nHPWsz = manager->options.getPageWorldSize() / 2;
		const Ray rayLocal (
			manager->toSpace(OCS_World, OCS_Terrain, ray.getOrigin() - getPosition()),	// TODO: Verify position
			manager->toSpace(OCS_World, OCS_Terrain, ray.getDirection())
		);
		Vector3 pHit;
		const Vector3 & vecRayLocalOrigin = rayLocal.getOrigin();

		OHT_DBGTRACE("\t\tPage BBox test=" << getBoundingBox());

		// Don't bother testing ray intersections with page boundaries if the ray origin is inside this page, let's just start processing at the tile at the ray origin
		if (
			vecRayLocalOrigin.x >= -nHPWsz && vecRayLocalOrigin.x <= +nHPWsz &&
			vecRayLocalOrigin.y >= -nHPWsz && vecRayLocalOrigin.y <= +nHPWsz
		)	
		{
			OHT_DBGTRACE("\t\t\tCONTAINED");
			pHit = rayLocal.getOrigin();
		}
		else
		{
			const Vector3 
				dir = rayLocal.getDirection();
			Vector3
				pHitX = Vector3::NEGATIVE_UNIT_X - nHPWsz, 
				pHitY = Vector3::NEGATIVE_UNIT_Y - nHPWsz;

			Real 
				nDistX = std::numeric_limits< Real >::max(),
				nDistY = nDistX;

			std::pair< bool, Real > 
				interX, interY;

			const Vector3 plnEdges_D[] = {
				Vector3(-nHPWsz + 1.0f,			0.0f,					0.0f),
				Vector3(0.0f,					-nHPWsz + 1.0f,			0.0f),
				Vector3(+nHPWsz - 1.0f,			0.0f,					0.0f),
				Vector3(0.0f,					+nHPWsz - 1.0f,			0.0f)
			};
			enum PlaneEdge
			{
				PLN_LEFT = 0,
				PLN_TOP = 1,
				PLN_RIGHT = 2,
				PLN_BOTTOM = 3
			};

			OHT_DBGTRACE("\t\tHit Planes:\n" 
				<< "\t\t\t" << plnEdges_D[0]
				<< "\t\t\t" << plnEdges_D[1]
				<< "\t\t\t" << plnEdges_D[2]
				<< "\t\t\t" << plnEdges_D[3]
			);

			if (dir.x > 0)
			{
				interX = rayLocal.intersects(Plane(Vector3::NEGATIVE_UNIT_X, plnEdges_D[PLN_LEFT]));
				if (interX.first && interX.second < nDistX)
				{
					nDistX = interX.second;
					OHT_DBGTRACE("\t\tIntersection LEFT=" << nDistX);
				}
			} else
			if (dir.x < 0)
			{
				interX = rayLocal.intersects(Plane(Vector3::UNIT_X, plnEdges_D[PLN_RIGHT]));
				if (interX.first && interX.second < nDistX)
				{
					nDistX = interX.second;
					OHT_DBGTRACE("\t\tIntersection RIGHT=" << nDistX);
				}
			}

			if (dir.y > 0)
			{
				interY = rayLocal.intersects(Plane(Vector3::NEGATIVE_UNIT_Y, plnEdges_D[PLN_TOP]));
				if (interY.first && interY.second < nDistY)
				{
					nDistY = interY.second;
					OHT_DBGTRACE("\t\tIntersection TOP=" << nDistY);
				}
			} else
			if (dir.y < 0)
			{
				interY = rayLocal.intersects(Plane(Vector3::UNIT_Y, plnEdges_D[PLN_BOTTOM]));
				if (interY.first && interY.second < nDistY)
				{
					nDistY = interY.second;
					OHT_DBGTRACE("\t\tIntersection BOTTOM=" << nDistY);
				}
			}

			if (nDistX < distanceLimit || !distanceLimit)
			{
				pHitX = rayLocal.getPoint(nDistX);
				OHT_DBGTRACE("\t\tHit X-wise=" << (manager->toSpace(OCS_Terrain, OCS_World, pHitX) + getPosition()));
			}
			if (nDistY < distanceLimit || !distanceLimit)
			{
				pHitY = rayLocal.getPoint(nDistY);
				OHT_DBGTRACE("\t\tHit Y-wise=" << (manager->toSpace(OCS_Terrain, OCS_World, pHitY) + getPosition()));
			}

			if (pHitX.x >= -nHPWsz && pHitX.x <= +nHPWsz && pHitX.y >= -nHPWsz && pHitX.y <= +nHPWsz)
				pHit = pHitX;
			else
			if (pHitY.x >= -nHPWsz && pHitY.x <= +nHPWsz && pHitY.y >= -nHPWsz && pHitY.y <= +nHPWsz)
				pHit = pHitY;
			else
				return false;
		}

		const TerrainTile * pTile = getTerrainTile(pHit, OCS_Terrain);

		// Outstanding bug tracker (<ray origin> : <ray direction>):
		// - {x=-2450.0525 y=255.06381 z=5713.2734 } : {x=0.79931283 y=-0.16910532 z=-0.57679886 }

		OgreAssert(pTile != NULL, "Something's wrong, should always get a tile here but got NULL instead");

		OHT_DBGTRACE("\t\tTile bbox intersection=" << (manager->toSpace(OCS_Terrain, OCS_World, pHit) + getPosition()));

		OHTDD_Translate(-getPosition());

		if (pTile->rayIntersects(result, rayLocal, true, distanceLimit))
		{
			OverhangTerrainManager::transformSpace(OCS_Terrain, manager->options.alignment, OCS_World, result.position, manager->options.cellScale);

			OHTDD_Color(DebugDisplay::MC_Turquoise);
			OHTDD_Line(ray.getOrigin() - getPosition(), result.position);

			result.position += getPosition();	// TODO: Verify position
				
			return true;
		} else
		{
			OHTDD_Color(DebugDisplay::MC_Yellow);
			OHTDD_Line(ray.getOrigin() - getPosition(), ray.getPoint(distanceLimit) - getPosition());

			return false;
		}
	}

	void PageSection::operator << ( const MetaObjsList & objs )
	{
		oht_assert_threadmodel(ThrMdl_Background);

		struct ObjectMeta {
			MetaObject * object;
			AxisAlignedBox bbox;
		} * vObjMeta = new ObjectMeta[objs.size()];

		unsigned int c = 0;
		for (MetaObjsList::const_iterator i = objs.begin(); i != objs.end(); ++i)
		{
			ObjectMeta & objmeta = vObjMeta[c++];
			objmeta.object = *i;
			objmeta.bbox = manager->toSpace(OCS_World, OCS_Terrain, (*i)->getAABB());
		}

		for (Terrain2D::iterator j = _vTiles.begin(); j != _vTiles.end(); ++j)
			for (TerrainRow::iterator i = j->begin(); i != j->end(); ++i)
				for (unsigned c = 0; c < objs.size(); ++c)
					if (
						vObjMeta[c].object->getObjectType() != MetaObject::MOT_HeightMap &&
						vObjMeta[c].object->getObjectType() != MetaObject::MOT_Invalid &&

						(*i)->getBBox(OCS_Terrain).intersects(vObjMeta[c].bbox)
					)
						(*i)->loadMetaObject(vObjMeta[c].object);

		delete [] vObjMeta;
	}

	void PageSection::addMetaBallImpl( MetaBall * const pMetaBall )
	{
		oht_assert_threadmodel(ThrMdl_Background);

		const AxisAlignedBox bboxMetaObjectTileSpace = manager->toSpace(OCS_World, OCS_Vertex, pMetaBall->getAABB());
		const Real
			nTS = manager->options.getTileWorldSize() / manager->options.cellScale,
			yext = bboxMetaObjectTileSpace.getMaximum().y + nTS + 1,
			xext = bboxMetaObjectTileSpace.getMaximum().x + nTS + 1;
		Vector3 vecTileWalk;

		OHT_DBGTRACE(
			"\taddMetaObject: " << pMetaBall->getPosition() << ", " << 
			"type=" << pMetaBall->getObjectType() << ", " << 
			"bbox=" << pMetaBall->getAABB()
		);

		// TODO: This algorithm is probably wrong, see corresponding code in OverhangTerrainGroup
		for (vecTileWalk.y = bboxMetaObjectTileSpace.getMinimum().y - 1; vecTileWalk.y < yext; vecTileWalk.y += nTS)
		{
			for (vecTileWalk.x = bboxMetaObjectTileSpace.getMinimum().x - 1; vecTileWalk.x < xext; vecTileWalk.x += nTS)
			{
				TerrainTile * pTile = getTerrainTile (vecTileWalk, OCS_Vertex);

				if (pTile != NULL)
				{
					OHT_DBGTRACE("\t\tTile, " << pTile->getBBox());
					pTile->addMetaBall(pMetaBall);
				}
			}
		}
	}

	void PageSection::addMetaBall( const Vector3 & position, Real radius, bool excavating /*= true*/ )
	{ 
		oht_assert_threadmodel(ThrMdl_Background);
		OgreAssert(_pScNode != NULL, "The page has not been initialized");

		addMetaBallImpl(_pFactory->createMetaBall(position, radius, excavating));
		_bDirty = true;
	}

	StreamSerialiser & PageSection::operator >> (StreamSerialiser & output) const
	{
		oht_assert_threadmodel(ThrMdl_Background);

		output.writeChunkBegin(CHUNK_ID, VERSION);

		for (size_t j = 0; j < _nTileCount; ++j)
			for (size_t i = 0; i < _nTileCount; ++i)
				*_vTiles[i][j] >> output;

		for (MetaObjectIterator i = iterateMetaObjects(); i; ++i)
		{
			const MetaObject::MOType enmot = i->getObjectType();

			output.write(&enmot);
			*i >> output;
		}
		const MetaObject::MOType enmot = MetaObject::MOT_Invalid;
		output.write(&enmot);

		output.writeChunkEnd(CHUNK_ID);

		return output;
	}
	StreamSerialiser & PageSection::operator << (StreamSerialiser & input)
	{
		oht_assert_threadmodel(ThrMdl_Background);

		if (!input.readChunkBegin(CHUNK_ID, VERSION))
			OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND, "Stream does not contain PageSection object data", __FUNCTION__);

		for (size_t j = 0; j < _nTileCount; ++j)
			for (size_t i = 0; i < _nTileCount; ++i)
				*_vTiles[i][j] << input;

		MetaObject::MOType enmot;
		MetaObject * mo;
		MetaObjsList vMetaObjs;

		do 
		{
			input.read(&enmot);
			switch (enmot)
			{
			case MetaObject::MOT_MetaBall:
				mo = _pFactory->createMetaBall();
				*mo << input;
				vMetaObjs.push_back(mo);
				break;
			}
		} while (enmot != MetaObject::MOT_Invalid);

		input.readChunkEnd(CHUNK_ID);

		*this << vMetaObjs;

		return input;
	}

	void PageSection::commitOperation()
	{
		oht_assert_threadmodel(ThrMdl_Main);

		// TODO: This is expensive
		for (size_t j = 0; j < _nTileCount; ++j)
			for (size_t i = 0; i < _nTileCount; ++i)
				_vTiles[i][j] -> commitOperation();
	}

	void PageSection::addListener( IOverhangTerrainListener * pListener )
	{
		oht_assert_threadmodel(ThrMdl_Single);
			
		_vListeners.insert(pListener);
	}

	void PageSection::removeListener( IOverhangTerrainListener * pListener )
	{
		oht_assert_threadmodel(ThrMdl_Single);

		_vListeners.erase(pListener);
	}

	void PageSection::fireOnBeforeLoadMetaRegion( MetaFragment::Interfaces::Unique * pFragment )
	{
		oht_assert_threadmodel(ThrMdl_Background);

		OverhangTerrainSupportsCustomData cube (pFragment->custom);

		for (ListenerSet::iterator i = _vListeners.begin(); i != _vListeners.end(); ++i)
		{
			if ((*i)->onBeforeLoadMetaRegion(this, &cube))
				break;
		}
	}

	void PageSection::fireOnCreateMetaRegion( Voxel::CubeDataRegion * pCubeDataRegion, MetaFragment::Interfaces::Unique * pUnique, const Vector3 & vertpos )
	{
		oht_assert_threadmodel(ThrMdl_Background);

		// Coordinates in page space are based at horizontal center so the x and y coordinates have to be rebased relative to the top-left corner of the page
		const Real nHPTVXsz = manager->options.getPageWorldSize() / (2.0f * manager->options.cellScale);
		const Real 
			xc = vertpos.x + nHPTVXsz,
			yc = vertpos.y + nHPTVXsz;

		OverhangTerrainMetaCube cube (
			pCubeDataRegion,
			pUnique,

			static_cast< int > (xc), 
			static_cast< int > (yc), 
			static_cast< int > (vertpos.z),

			static_cast< int > (xc + manager->options.tileSize - 1), 
			static_cast< int > (yc + manager->options.tileSize - 1), 
			static_cast< int > (vertpos.z + manager->options.tileSize - 1)
		);

		for (ListenerSet::iterator i = _vListeners.begin(); i != _vListeners.end(); ++i)
		{
			if ((*i)->onCreateMetaRegion(this, &cube))
				break;
		}
	}

	void PageSection::fireOnInitMetaRegion(MetaFragment::Interfaces::const_Basic * pBasic, MetaFragment::Interfaces::Builder * pBuilder)
	{
		oht_assert_threadmodel(ThrMdl_Main);

		OverhangTerrainRenderable cube (pBuilder);

		for (ListenerSet::iterator i = _vListeners.begin(); i != _vListeners.end(); ++i)
		{
			if ((*i)->onInitMetaRegion(this, &cube))
				break;
		}
	}

	void PageSection::fireOnDestroyMetaRegion( MetaFragment::Interfaces::Unique * pUnique )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		OverhangTerrainSupportsCustomData custom(pUnique->custom);

		for (ListenerSet::iterator i = _vListeners.begin(); i != _vListeners.end(); ++i)
		{
			if ((*i)->onDestroyMetaRegion(this, &custom))
				break;
		}
	}

	void PageSection::updateBBox()
	{
		oht_assert_threadmodel(ThrMdl_Main);

		const Vector3 vecOffsetHalf = Vector3(1, 0, 1) * (manager->options.getPageWorldSize() / 2);
		_bbox = AxisAlignedBox(getPosition() - vecOffsetHalf, getPosition() + vecOffsetHalf);
	}

	const AxisAlignedBox & PageSection::getBoundingBox() const
	{
		oht_assert_threadmodel(ThrMdl_Main);
		return _bbox;
	}

	bool PageSection::isDirty() const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		return _bDirty;
	}

	void PageSection::detachFromScene()
	{
		oht_assert_threadmodel(ThrMdl_Main);

		for (Terrain2D::iterator j = _vTiles.begin(); j != _vTiles.end(); ++j)
			for (TerrainRow::iterator i = j->begin(); i != j->end(); ++i)
				(*i)->detachFromScene();

		if (_pScNode != NULL)
		{
			_pScNode->getCreator()->destroySceneNode(_pScNode);
			_pScNode = NULL;
		}
	}

	Ogre::MetaFragmentIterator PageSection::iterateMetaFrags()
	{
		return MetaFragmentIterator(_pPrivate);
	}

	const MetaObjectIterator PageSection::iterateMetaObjects() const
	{
		return MetaObjectIterator(this->_pPrivate, MetaObject::MOT_MetaBall, MetaObject::MOT_Invalid);
	}

	MetaObjectIterator::MetaObjectIterator( const PagePrivateNonthreaded * pPage, MetaObject::MOType enmoType, ... ) 
		: _pPage(pPage), _pUniqueInterface(NULL), _nTileCount(pPage->getManager().options.getTilesPerPage())
	{
		OHT_CR_INIT();
		va_list aenmoTypes;
		va_start (aenmoTypes, enmoType);

		while (MetaObject::MOT_Invalid != enmoType)
		{
			_setTypeFilter.insert(enmoType);
			enmoType = va_arg(aenmoTypes, MetaObject::MOType);
		}
		va_end(aenmoTypes);
		process();
	}

	void MetaObjectIterator::process()
	{
		oht_assert_threadmodel(ThrMdl_Single);
		OHT_CR_START();

		for ( _nTileP = 0; _nTileP < _nTileCount; ++_nTileP)
		{
			for (_nTileQ = 0; _nTileQ < _nTileCount; ++_nTileQ)
			{
				_pTile = _pPage->getTerrainTile(_nTileP, _nTileQ);
				if (_pTile->hasMetaFrags())
				{
					for (_iFrags = _pTile->beginFrags(); _iFrags != _pTile->endFrags(); ++_iFrags)
					{
						_pUniqueInterface = new MetaFragment::Interfaces::Unique (_iFrags->second->acquireInterface());

						for (_iObjects = _pUniqueInterface->beginMetas(); _iObjects != _pUniqueInterface->endMetas(); ++_iObjects)
						{
							if (_setTypeFilter.find((*_iObjects)->getObjectType()) != _setTypeFilter.end() && 
								_setVisitedObjs.find(*_iObjects) == _setVisitedObjs.end())
							{
								_setVisitedObjs.insert(*_iObjects);
								OHT_CR_RETURN_VOID(CRS_Default);
							}
						}

						delete _pUniqueInterface;
						_pUniqueInterface = NULL;
					}
				}
				_pTile = NULL;
			}
		}

		OHT_CR_END();
	}

	MetaObjectIterator::~MetaObjectIterator()
	{
		oht_assert_threadmodel(ThrMdl_Single);
		delete _pUniqueInterface;
	}
	
	MetaFragmentIterator::MetaFragmentIterator( const PagePrivateNonthreaded * pPage)
		: _pPage(pPage), _nTileCount(pPage->getManager().options.getTilesPerPage())
	{
		OHT_CR_INIT();
		process();
	}

	MetaFragmentIterator::~MetaFragmentIterator()
	{
		oht_assert_threadmodel(ThrMdl_Single);
	}

	void MetaFragmentIterator::process()
	{
		oht_assert_threadmodel(ThrMdl_Single);

		OHT_CR_START();

		for (_i = 0; _i < _nTileCount; ++_i)
			for (_j = 0; _j < _nTileCount; ++_j)
			{
				_pTile = _pPage->getTerrainTile(_i, _j);

				if (_pTile->hasMetaFrags())
					for (_iFrags = _pTile->beginFrags(); _iFrags != _pTile->endFrags(); ++_iFrags)
						OHT_CR_RETURN_VOID(CRS_Default);

				_pTile = NULL;
			}

		OHT_CR_END();
	}


	PagePrivateNonthreaded::PagePrivateNonthreaded( PageSection * const page ) 
		: _pPage(page)
	{

	}

}

