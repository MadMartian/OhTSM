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
#ifndef __OVERHANGTERRAINPAGESECTION_H__
#define __OVERHANGTERRAINPAGESECTION_H__

#include <iterator>
#include <vector>
#include <set>

#include <boost/thread.hpp>

#include "Neighbor.h"
#include "Util.h"

#include <OgreRenderQueue.h>

#include "OverhangTerrainPrerequisites.h"

#include "OverhangTerrainManager.h"
#include "OverhangTerrainPageInitParams.h"
#include "OverhangTerrainListener.h"
#include "OverhangTerrainPage.h"

#include "MetaObject.h"
#include "MetaFactory.h"
#include "ChannelIndex.h"

namespace Ogre {
	class PagePrivateNonthreaded;

	/** Iterator pattern for walking all metaobjects that occur in the channel of a page */
	class _OverhangTerrainPluginExport MetaObjectIterator : public std::iterator< std::input_iterator_tag, MetaObject >
	{
	private:
		/// The number of terrain tiles per page along one side
		const size_t _nTileCount;
		/// Special access to some private members of the hosting page
		const PagePrivateNonthreaded * _pPage;
		/// Exclusive access to the current meta-fragment during iteration
		MetaFragment::Interfaces::Unique * _pUniqueInterface;
		/// Iterator for the meta-fragments in the current terrain tile during iteration
		MetaFragMap::const_iterator _iFrags;
		/// Iterator for the metaobjects in the current meta-fragment during iteration
		MetaObjsList::const_iterator _iObjects;
		/// The current terrain tile during iteration
		TerrainTile * _pTile;
		/// The channel of meta-fragments to isolate the search to
		const Channel::Ident _channel;

		/// Tracks all the visited metaobjects so far, used to ensure a metaobject isn't visited more than once
		std::set< MetaObject * > _setVisitedObjs;
		/// Refine the iteration to only those metaobjects who conform to these types
		std::set< MetaObject::MOType > _setTypeFilter;

		/// Current page tile offset during iteration
		size_t _nTileP, _nTileQ;

		enum CRState
		{
			CRS_Default = 1
		};

		OHT_CR_CONTEXT;

		/// Performs a single iteration
		void process ();

	public:
		/** 
		@param pPage Private access to the page to iterate metaobjects of
		@param channel The channel of meta-objects to search through
		@param enmoType Inline list of metaobject types to refine the search to
		*/
		MetaObjectIterator (const PagePrivateNonthreaded * pPage, const Channel::Ident channel, MetaObject::MOType enmoType, ...);
		~MetaObjectIterator();

		inline MetaObject & operator*() { return **_iObjects; }
		inline MetaObject * operator->() { return *_iObjects; }
		inline operator bool () { return !OHT_CR_TERM; }
		inline MetaObjectIterator & operator++()
		{
			process();
			return *this;
		}
		inline MetaObjectIterator operator++(int)
		{
			MetaObjectIterator old = *this;

			process();
			return old;
		}
	};

	/** Iterates through all meta-fragments occurring in the channel of a page */
	class _OverhangTerrainPluginExport MetaFragmentIterator : public std::iterator< std::input_iterator_tag, MetaFragment::Container >
	{
	private:
		/// The channel of meta-fragments sought through
		const Channel::Ident _channel;
		/// Special access to some private members of the hosting page
		const PagePrivateNonthreaded * _pPage;
		/// Iterator for the meta-fragments in the current terrain tile during iteration
		MetaFragMap::const_iterator _iFrags;
		/// The current terrain tile during iteration
		TerrainTile * _pTile;
		/// Current page tile offset during iteration
		size_t _i, _j, 
		/// The number of terrain tiles per page along one side
			_nTileCount;

		enum CRState
		{
			CRS_Default = 1
		};

		OHT_CR_CONTEXT;

		/// Performs a single iteration
		void process();

	public:
		/** 
		@param channel The channel to limit the search for meta-fragments to
		@param pPage Private access to the page to iterate metaobjects of
		*/
		MetaFragmentIterator (const Channel::Ident channel, const PagePrivateNonthreaded * pPage);
		~MetaFragmentIterator();

		inline MetaFragment::Container & operator * () { return *_iFrags->second; }
		inline MetaFragment::Container * operator -> () { return _iFrags->second; }

		inline operator bool () { return !OHT_CR_TERM; }
		inline MetaFragmentIterator & operator ++ ()
		{
			process();
			return *this;
		}
		inline MetaFragmentIterator operator ++ (int)
		{
			MetaFragmentIterator old = *this;
			process();
			return old;
		}
	};

	/** Groups a number of TerrainTiles into a page, which is the unit of loading / unloading.
	@remarks This class used to be designed to prevent single-hit page loads
	*/
	class PageSection : public GeneralAllocatedObject, public IOverhangTerrainPage
	{
	public:
		/// The overhang-terrain manager singleton
		const OverhangTerrainManager * const manager;
		/// The terrain slot hosting this object
		OverhangTerrainSlot * const slot;

		/**
		@param mgr The overhang-terrain manager singleton
		@param pSlot The terrain slot within which this page section resides (one-to-one relationship)
		@param pMetaFactory The object factory singleton
		@param descchann The channel descriptor
		*/
		PageSection(const OverhangTerrainManager * mgr, OverhangTerrainSlot * pSlot, MetaBaseFactory * const pMetaFactory, const Channel::Descriptor & descchann);
		virtual ~PageSection();

		/// Initializes all terrain tiles with the specified initialization parameters
		virtual void operator << (const PageInitParams & params);

		/** Links-up all meta-fragments and updates them
		@remarks Ensures all meta-fragments are created that are necessary and then links them up between terrain tiles
			and vertically, does not link-up across pages.  Additionally this method updates the voxel grids for each meta-fragment
		*/
		virtual void conjoin();

		/// Adds a listener to the page channel to receive events fired by this page that occur in the specified channel
		virtual void addListener (const Channel::Ident channel, IOverhangTerrainListener * pListener);
		/// Removes a listener from the page previously added for the specified channel
		virtual void removeListener (const Channel::Ident channel, IOverhangTerrainListener * pListener);

		/// @returns The center of the page according to its parent node in the scene
		virtual Vector3 getPosition() const;
		/// @returns The bounding box of the page according to the scene
		virtual const AxisAlignedBox & getBoundingBox() const;
		
		/// @returns The scene node that this page has renderables attached to as children
		virtual const SceneNode * getSceneNode () const { return _pScNode; }
		/// @returns The scene node that this page has renderables attached to as children
		virtual SceneNode * getSceneNode () { return _pScNode; }

		/// Serializes the page and all of its contents to the stream
		virtual StreamSerialiser & operator >> (StreamSerialiser & output) const;
		/// Deserializes the stream to this page and its children, does not initialize them
		virtual StreamSerialiser & operator << (StreamSerialiser & input);

		/** Initialize this page and its terrain tiles
		@param pScNode The scene node to attach this page and its renderables to
		*/
		void initialise(SceneNode * const pScNode);

		/// Determines whether this page and/or its contents have become inconsistent with permanent storage
		bool isDirty() const;

		/** Commits any pending operations
		@remarks Executes finalizing tasks for pending operations queued by another thread such as adding metaobjects
		*/
		void commitOperation();

		/** Add a new metaball to the page 
		@see addMetaBall
		*/
		void addMetaBall(const Vector3 & position, Real radius, bool excavating = true);

		/** Sets the render queue group for the specified channel within which the tiles should be rendered. */
		void setRenderQueue(const Channel::Ident channel, uint8 qid);

		// Sets the rendering material of all terrain tiles in the specified channel of this page
		void setMaterial (const Channel::Ident channel, const MaterialPtr & m);
		// Sets the position of terrain tiles in this page relative to the specified world position
		void setPosition(const Vector3 & pt);
		
		/// Links this page with a neighboring page including all of its terrain tiles (restricted to main thread)
		void linkPageNeighbor (PageSection * const pPageNeighbor, const VonNeumannNeighbor ennNeighbor);
		/// Unlinks this page with its neighbors (restricted to main thread)
		void unlinkPageNeighbors();

		// Walks through all child renderables and detaches them from the scene then destroys scene nodes.
		void detachFromScene();

		/** Queries the page's isosurfaces for intersection with the specified ray
		@remarks This method is restricted to the main thread
		@param result Receives the isosurface triangle intersection result
		@param rayPageLocalWorldSpace Ray in world-space coordinates relative to this page's position
		@param params Query parameters relevant to the current query
		@param distance The current distance traversed so far in the current query (used to compare with the maximum limit in the params parameter)
		@returns True when an intersection with an isosurface and triangle was found somewhere within this page */
		bool rayIntersects(
			OverhangTerrainManager::RayResult & result,
			const Ray& rayPageLocalWorldSpace,
			const OverhangTerrainManager::RayQueryParams & params,
			const Real distance
		) const;

		/// Retrieves an iterator for all meta-fragments in the specified channel of this page
		MetaFragmentIterator iterateMetaFrags (const Channel::Ident channel);

	private:
		typedef std::vector < TerrainTile * > TerrainRow;
		typedef std::vector < TerrainRow > Terrain2D;
		typedef std::set< IOverhangTerrainListener * > ListenerSet;

		/// The channel descriptor
		const Channel::Descriptor _descchann;

		/// An object that terrain tiles can use to gain special access to this class without resorting to be-friending the enitre class
		PagePrivateNonthreaded * _pPrivate;
		friend PagePrivateNonthreaded;

		/// Page neighbors to other pages in a Von Neumann neighborhood
		PageSection * _vpNeighbors[CountVonNeumannNeighbors];

		/// Page x/y indices in the scene, used to uniquely identify a page
		int32 _x, _y;
		/// The object factory singleton
		MetaBaseFactory * const _pFactory;
		/// The number of tiles per page along one side
		const size_t _nTileCount;
		/// The scene node that this page and its renderables is bound to
		SceneNode * _pScNode;
		/// The bounding box of the page according to the scene
		AxisAlignedBox _bbox;
		/// The 2-dimensional array of the terrain tiles
		Terrain2D _vTiles;
		/// The set of listeners distributed by channel
		Channel::Index< ListenerSet > _vvListeners;
		/// Flag indicating if this page is inconsistent with permanent storage
		bool _bDirty;

		/// Pointer to the meta-heightmap metaobject added to every meta-fragment of this terrain-tile when it's created
		MetaHeightMap * _pMetaHeightmap;

		/// Used for serialization
		static const uint32 CHUNK_ID;
		static const uint16 VERSION;

		/// Unlinks the neighbor at the specified side, result is dual so it does not need to also be called for the neighbor too
		void unlinkPageNeighbor (const VonNeumannNeighbor ennNeighbor);
		/// Finds the appropriate terrain tiles of the specified channel to add the specified metaball to and then does so, does not update voxels
		void addMetaObjectImpl(const Channel::Ident channel, MetaObject * const pMetaBall);
		/// Loads the specified list of metaobjects into the specified channel of this page
		void loadMetaObjects (const Channel::Ident channel, const MetaObjsList & objs);
		/// Updates the bbox to reflect the current position
		void updateBBox ();

		/// Dispatches the before-load event to all listeners of the specified channel
		void fireOnBeforeLoadMetaRegion (const Channel::Ident channel, MetaFragment::Interfaces::Unique * pFragment);
		/// Dispatches the create-meta-region event to all listeners of the specified channel, the bounding-box is in vertex-space
		void fireOnCreateMetaRegion (const Channel::Ident channel, Voxel::CubeDataRegion * pCubeDataRegion, MetaFragment::Interfaces::Unique * pUnique, const AxisAlignedBox & bbox);
		/// Dispatches the init-meta-region event to all listeners of the specified channel
		void fireOnInitMetaRegion (const Channel::Ident channel, MetaFragment::Interfaces::const_Basic * pBasic, MetaFragment::Interfaces::Builder * pBuilder);
		/// Dispatches the destroy-meta-region event to all listeners of the specified channel
		void fireOnDestroyMetaRegion( const Channel::Ident channel, MetaFragment::Interfaces::Unique * pUnique );
		/// @returns True if the scene node is set
		inline bool isLoaded () const { return _pScNode != NULL; }
		/// @returns The terrain tile that the specified point intersects
		TerrainTile * getTerrainTile( const Vector3 & pt, const OverhangCoordinateSpace encsFrom = OCS_World ) const;
		/// @returns The terrain tile at the specified 2-dimensional array indices in the page
		inline TerrainTile * getTerrainTile (const size_t i, const size_t j) const { return _vTiles[i][j]; }
		/// @returns Iterator for all the metaobjects in the specified channel of the page
		const MetaObjectIterator iterateMetaObjects (const Channel::Ident channel) const;

		/** Links-up a meta-fragment to a terrain tile potentially containing a meta-fragment for a channel
		@remarks Potentially links-up the meta-fragment of the specified terrain-tile (if it exists) with the one specified if one exists in the terrain-tile with the same y-level and channel.
			Both objects must belong to this page and the specified channel.  This method is dual so that it doesn't have to be called again for the reverse direction.
		@param channel The channel to link-up fragments within
		@param pHost If a meta-fragment exists in this terrain tile at the appropriate y-level, it will be linked with the other meta-fragment
		@param i Iterator pointing to a meta-fragment that will be potentially linked to one of the same y-level (if it exists) in the terrain tile */
		void linkFragmentHorizontalInternal (const Channel::Ident channel, TerrainTile * pHost, MetaFragMap::iterator i);
	};

	/** Facet that exposes certain private features of a page that should 
		be made available to terrain tiles without resorting to friends on 
		the PageSection class */
	class PagePrivateNonthreaded
	{
	private:
		/// Page that this object is linked to and manipulates or interrogates
		PageSection * const _pPage;

	protected:
		inline PagePrivateNonthreaded(PageSection * const page);

	public:
		inline const PageSection * getPublic() const { return _pPage; }
		inline PageSection * getPublic() { return _pPage; }

		inline SceneNode * getSceneNode () 
			{ return _pPage->getSceneNode(); }

		inline const OverhangTerrainManager & getManager() const { return *_pPage->manager; }
		inline const MetaBaseFactory & getFactory () const { return *_pPage->_pFactory; }

		inline void linkFragmentHorizontalInternal (const Channel::Ident channel, TerrainTile * pHost, MetaFragMap::iterator i)
			{ _pPage->linkFragmentHorizontalInternal(channel, pHost, i); }

		inline TerrainTile * getTerrainTile( const Vector3 & pt, const OverhangCoordinateSpace encsFrom = OCS_World ) const
			{ return _pPage->getTerrainTile(pt, encsFrom); }
		inline TerrainTile * getTerrainTile (const size_t i, const size_t j) const 
			{ return _pPage->getTerrainTile(i, j); }

		inline int32 getPageX() const { return _pPage->_x; }
		inline int32 getPageY() const { return _pPage->_y; }

		inline const MetaHeightMap * getMetaHeightMap () const { return _pPage->_pMetaHeightmap; }
		inline MetaHeightMap * getMetaHeightMap () { return _pPage->_pMetaHeightmap; }

		inline void fireOnBeforeLoadMetaRegion (const Channel::Ident channel, MetaFragment::Interfaces::Unique * pFragment)
			{ _pPage->fireOnBeforeLoadMetaRegion(channel, pFragment); }
		inline void fireOnCreateMetaRegion ( const Channel::Ident channel, Voxel::CubeDataRegion * pCubeDataRegion, MetaFragment::Interfaces::Unique * pUnique, const AxisAlignedBox & bbox )
			{ _pPage->fireOnCreateMetaRegion( channel, pCubeDataRegion, pUnique, bbox); }
		inline void fireOnInitMetaRegion (const Channel::Ident channel, MetaFragment::Interfaces::const_Basic * pBasic, MetaFragment::Interfaces::Builder * pBuilder)
			{ _pPage->fireOnInitMetaRegion(channel, pBasic, pBuilder); }
		inline void fireOnDestroyMetaRegion (const Channel::Ident channel, MetaFragment::Interfaces::Unique * pUnique )
			{ _pPage->fireOnDestroyMetaRegion(channel, pUnique); }

		friend class PageSection;
	};
}

#endif
