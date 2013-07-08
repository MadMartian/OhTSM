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
OverhangTerrainSceneManager.h  -  description
---------------------
  begin                : Mon Sep 23 2002
  copyright            : (C) 2002 by Jon Anderson
  email                : janders@users.sf.net

  Enhancements 2003 - 2004 (C) The OGRE Team

  Modified by Martin Enge (martin.enge@gmail.com) 2007 to fit into the OverhangTerrain Scene Manager
  Modified by Jonathan Neufeld (http://www.extollit.com) 2011-2013 to implement Transvoxel
  Transvoxel conceived by Eric Lengyel (http://www.terathon.com/voxels/)

***************************************************************************/

#ifndef __OverhangTerrainSceneManager_H__
#define __OverhangTerrainSceneManager_H__

#include <OgreIteratorWrappers.h>
#include <OgreSceneQuery.h>
#include <OgreSceneManager.h>

#include "OverhangTerrainPrerequisites.h"
#include "OverhangTerrainManager.h"

namespace Ogre
{

	/** Ray scene query applicable to objects managed by OhTSM */
	class _OverhangTerrainPluginExport OverhangTerrainRaySceneQuery : public DefaultRaySceneQuery
	{
	protected:
		WorldFragment _frag;

	public:
		OverhangTerrainRaySceneQuery(SceneManager* creator);
		~OverhangTerrainRaySceneQuery();

		/** See RayScenQuery. */
		void execute(RaySceneQueryListener* listener);
	};

	/** Original: This is a basic SceneManager for organizing TerrainTiles into a total landscape.
	  *@author Jon Anderson
	  *...Actually this is a modified version allowing for Overhangs created by Metaballs
	  *and a MetaHeightmap. It is a bit of a hack, and especially - no work has gone 
	  *into ascertaining that all the basic SceneManager functions work as expected.
	  *You have been warned...
	  */
	class _OverhangTerrainPluginExport OverhangTerrainSceneManager : public SceneManager
	{
	public:
		OverhangTerrainSceneManager(const String& name);
		virtual ~OverhangTerrainSceneManager( );

		/// @copydoc SceneManager::getTypeName
		const String& getTypeName(void) const;

		// Call after setting the world geometry, used to belong to setWorldGeometry
		void initialise();

		/** Creates a RaySceneQuery for this scene manager. 
		@remarks
			This method creates a new instance of a query object for this scene manager, 
			looking for objects which fall along a ray. See SceneQuery and RaySceneQuery 
			for full details.
		@par
			The instance returned from this method must be destroyed by calling
			SceneManager::destroyQuery when it is no longer required.
		@param ray Details of the ray which describes the region for this query.
		@param mask The query mask to apply to this query; can be used to filter out
			certain objects; see SceneQuery for details.
		*/
		RaySceneQuery* 
			createRayQuery(const Ray& ray, unsigned long mask = 0xFFFFFFFF);

		/** Overridden in order to store the first camera created as the primary
			one, for determining error metrics and the 'home' terrain page.
		*/
		Camera* createCamera( const String &name );
		/// Gets the main top-level terrain options used in all of OhTSM
		const OverhangTerrainOptions& getOptions(void) { return _options; }

		/// Updates the main top-level terrain options used in all of OhTSM, but does not propagate the changes to anywhere other than persisting the options object herein
		void setOptions (const OverhangTerrainOptions & opts);

		/** Sets the 'primary' camera, i.e. the one which will be used to determine
			the 'home' terrain page, and to calculate the error metrics. 
		*/
		virtual void setPrimaryCamera(const Camera* cam);

		// Attaches the specified page's scene node to the scene manager, called by the paging system
		virtual void attachPage (PageSection * page);

		/// Get the SceneNode under which all terrain nodes are attached.
		SceneNode* getTerrainRootNode(void) const { return _pRoot; }
		/** Overridden from SceneManager */
		void clearScene(void);

		/// Shutdown cleanly before we get destroyed
		void shutdown(void);

		/// Removes all objects from the scene created by DebugDisplay
		void clearDebugObjects();

		/** Adds a metaball to the scene.
		@remarks A background request is initiated for adding the metaball to the scene and updating the respective voxel grids.
		@param position The absolute world position of the metaball
		@param radius The radius of the metaball sphere in world units
		@param excavating Whether or not the metaball carves out empty space or fills it in with solid
		@param synchronous Whether or not to execute the operation in this thread and not leverage threading capability */
		inline void addMetaBall (const Vector3 & position, const Real radius, const bool excavating = true) 
			{ _pTerrainManager->addMetaBall(position, radius, excavating); }	// TODO: Account for terrain origin

		/// @returns The overhang terrain manager responsible for handling page load/unload and deformation
		inline const OverhangTerrainManager * getTerrainManager () const
			{ return _pTerrainManager; }
		/// @returns The overhang terrain manager responsible for handling page load/unload and deformation
		inline OverhangTerrainManager * getTerrainManager ()
			{ return _pTerrainManager; }

	protected:

		/// The node to which all terrain tiles are attached
		SceneNode * _pRoot;
		/// Terrain size, detail etc
		OverhangTerrainOptions _options;
    
		/// Sets the overhang terrain manager responsible for handling page load/unload and deformation
		inline void setTerrainManager (OverhangTerrainManager * tmgr)
			{ _pTerrainManager = tmgr; }

	private:
		/// The overhang terrain manager responsible for handling page load/unload and deformation
		OverhangTerrainManager * _pTerrainManager;

		friend class OverhangTerrainManager;
	};

	/// Factory for OverhangTerrainSceneManager
	class OverhangTerrainSceneManagerFactory : public SceneManagerFactory
	{
	protected:
		void initMetaData(void) const;

	public:
		OverhangTerrainSceneManagerFactory();
		~OverhangTerrainSceneManagerFactory();
		/// Factory type name
		static const String FACTORY_TYPE_NAME;
		SceneManager* createInstance(const String& instanceName);
		void destroyInstance(SceneManager* instance);
	};

}

#endif
