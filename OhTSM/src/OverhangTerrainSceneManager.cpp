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
-----------------------------------------------------------------------------
*/
/***************************************************************************
OverhangTerrainSceneManager.cpp  -  description
-------------------
begin                : Mon Sep 23 2002
copyright            : (C) 2002 by Jon Anderson
email                : janders@users.sf.net

Enhancements 2003 - 2004 (C) The OGRE Team

Hacked by Martin Enge (martin.enge@gmail.com) 2007 to fit into the OverhangTerrain Scene Manager
Modified by Jonathan Neufeld (http://www.extollit.com) 2011-2013 to implement Transvoxel
Transvoxel conceived by Eric Lengyel (http://www.terathon.com/voxels/)

***************************************************************************/
#include "pch.h"

#include "OverhangTerrainSceneManager.h"

#include <OgreImage.h>
#include <OgreConfigFile.h>
#include <OgreMaterial.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreCamera.h>
#include <OgreException.h>
#include <OgreStringConverter.h>
#include <OgreRenderSystem.h>
#include <OgreRenderSystemCapabilities.h>
#include <OgreGpuProgram.h>
#include <OgreGpuProgramManager.h>
#include <OgreLogManager.h>
#include <OgreResourceGroupManager.h>
#include <OgreMaterialManager.h>

#include <fstream>

#include "PageSection.h"

#include "DebugTools.h"

#include "CubeDataRegion.h"
#include "IsoSurfaceBuilder.h"
#include "MetaWorldFragment.h"
#include "IsoSurfaceRenderable.h"

#include "MetaBall.h"
#include "RenderManager.h"

#define TERRAIN_MATERIAL_NAME "OverhangTerrainSceneManager/Terrain"

namespace Ogre
{
	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	OverhangTerrainSceneManager::OverhangTerrainSceneManager(const String& name)
		: SceneManager(name), _pTerrainManager(NULL),
		_pRoot(NULL)
	{
		_pRendMan = new RenderManager(this);
		//setDisplaySceneNodes( true );
		//setShowBoxes( true );
	}
	//-------------------------------------------------------------------------
	const String& OverhangTerrainSceneManager::getTypeName(void) const
	{
		return OverhangTerrainSceneManagerFactory::FACTORY_TYPE_NAME;
	}
	//-------------------------------------------------------------------------
	void OverhangTerrainSceneManager::shutdown(void)
	{
	}
	//-------------------------------------------------------------------------
	OverhangTerrainSceneManager::~OverhangTerrainSceneManager()
	{
		delete _pRendMan;
		shutdown();
	}

	//-------------------------------------------------------------------------
	void OverhangTerrainSceneManager::clearScene(void)
	{
		SceneManager::clearScene();
		// SceneManager has destroyed our root
		_pRoot = NULL;
	}

	void OverhangTerrainSceneManager::attachPage( PageSection * page )
	{
		_pRoot->addChild(page->getSceneNode());
	}

	//-------------------------------------------------------------------------
	Camera* OverhangTerrainSceneManager::createCamera( const String &name )
	{
		Camera* c = SceneManager::createCamera(name);

		// Set primary camera, if none
		if (!_options.primaryCamera)
			setPrimaryCamera(c);

		return c;

	}
	//-------------------------------------------------------------------------
	void OverhangTerrainSceneManager::setPrimaryCamera(const Camera* cam)
	{
		_options.primaryCamera = cam;
	}
	//-------------------------------------------------------------------------
	RaySceneQuery*
		OverhangTerrainSceneManager::createRayQuery(const Ray& ray, unsigned long mask)
	{
		OverhangTerrainRaySceneQuery *trsq = new OverhangTerrainRaySceneQuery(this);
		trsq->setRay(ray);
		trsq->setQueryMask(mask);
		return trsq;
	}

	void OverhangTerrainSceneManager::initialise()
	{
		_pRoot = getRootSceneNode() -> createChildSceneNode( "Terrain" );
	}

	void OverhangTerrainSceneManager::setOptions( const OverhangTerrainOptions & opts )
	{
		_options = opts;
		// TODO: Notify any relevant dependencies
	}

	void OverhangTerrainSceneManager::clearDebugObjects()
	{
		OHTDD_Clear();
	}

	//-----------------------------------------------------------------------
	const String OverhangTerrainSceneManagerFactory::FACTORY_TYPE_NAME = "OverhangTerrainSceneManager";
	//-----------------------------------------------------------------------
	OverhangTerrainSceneManagerFactory::OverhangTerrainSceneManagerFactory()
	{
	}
	//-----------------------------------------------------------------------
	OverhangTerrainSceneManagerFactory::~OverhangTerrainSceneManagerFactory()
	{
	}
	//-----------------------------------------------------------------------
	void OverhangTerrainSceneManagerFactory::initMetaData(void) const
	{
		mMetaData.typeName = FACTORY_TYPE_NAME;
		mMetaData.description = "Scene manager which generally organises the scene on "
			"the basis of an octree, but also supports terrain world geometry. ";
		mMetaData.sceneTypeMask = ST_EXTERIOR_CLOSE; // previous compatiblity
		mMetaData.worldGeometrySupported = true;
	}
	//-----------------------------------------------------------------------
	SceneManager* OverhangTerrainSceneManagerFactory::createInstance(
		const String& instanceName)
	{
		OverhangTerrainSceneManager* tsm = new OverhangTerrainSceneManager(instanceName);

		return tsm;

	}
	//-----------------------------------------------------------------------
	void OverhangTerrainSceneManagerFactory::destroyInstance(SceneManager* instance)
	{
		delete instance;
	}


	//-------------------------------------------------------------------------
	OverhangTerrainRaySceneQuery::OverhangTerrainRaySceneQuery(SceneManager* creator)
		: DefaultRaySceneQuery(creator)
	{
		mSupportedWorldFragments.insert(SceneQuery::WFT_SINGLE_INTERSECTION);
	}
	//-------------------------------------------------------------------------
	OverhangTerrainRaySceneQuery::~OverhangTerrainRaySceneQuery()
	{
	}
	//-------------------------------------------------------------------------
	void OverhangTerrainRaySceneQuery::execute(RaySceneQueryListener* listener)
	{
		_frag.fragmentType = SceneQuery::WFT_SINGLE_INTERSECTION;

		const Vector3& dir = mRay.getDirection();
		const Vector3& origin = mRay.getOrigin();

		OverhangTerrainManager::RayResult result = 
			static_cast< OverhangTerrainSceneManager* >( mParentSceneMgr ) 
			-> getTerrainManager() ->rayIntersects(mRay, OverhangTerrainManager::RayQueryParams::from(100000));

		if (result.hit)
		{
			_frag.singleIntersection = result.position;

			const Real fDist = (_frag.singleIntersection - origin).length();
			if (!listener->queryResult(static_cast< MovableObject * > (result.mwf->acquireBasicInterface().surface), fDist))
				return;

			if (!listener->queryResult(&_frag, fDist))
				return;

			DefaultRaySceneQuery::execute(listener);
		}
	}

} //namespace
