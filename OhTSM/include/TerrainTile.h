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

#ifndef _OVERHANGTERRAINTILE_H_
#define _OVERHANGTERRAINTILE_H_

#include <OgreStreamSerialiser.h>
#include <OgreRay.h>

#include <boost/thread.hpp>

#include "Neighbor.h"

#include "OverhangTerrainPrerequisites.h"

#include "OverhangTerrainManager.h"
#include "OverhangTerrainPageInitParams.h"

namespace Ogre
{
	/** Container for a vertical tile of terrain within a page, also container for meta-fragments */
	class TerrainTile : public GeneralAllocatedObject
	{
	public:
		/// Flags indicating what borders of the page this terrain-tile touches (if it does at all)
		const Touch2DSide borders;
		/// 2D index of the terrain-tile per page
		const size_t p, q;

		/** 
		@param p x-component of the 2D index of the terrain-tile in its page
		@param q y-component of the 2D index of the terrain-tile in its page
		@param page The parent page within which this terrain-tile is contained
		@param opts The main top-level OhTSM configuration options
		*/
		TerrainTile(const size_t p, const size_t q, PagePrivateNonthreaded * const page, const OverhangTerrainOptions & opts);
		~TerrainTile();

		/** Links up all meta-fragment neighbors in the terrain-tile
		@remarks Links-up meta-fragment neighbors with meta-fragments in surrounding terrain-tiles provided they are
			all within the same page, and with meta-fragments above and below within this terrain-tile
		*/
		void linkUpAllSurfaces();
		
		/** Links-up meta-fragments in this terrain-tile with corresponding ones in another one
		@remarks Links-up all meta-fragments in this terrain-tile that correspond to meta-fragments in the
			specified terrain-tile according to Y-level.
		@param ennNeighbor Identifies where the specified terrain-tile occurs adjacent to this one
		@param pNeighborTile The neighbor tile to which meta-fragments here-in will be linked
		*/
		void linkNeighbor(const VonNeumannNeighbor ennNeighbor, TerrainTile * pNeighborTile);

		/** Links-up a meta-fragment with a corresponding meta-fragment in this terrain tile
		@remarks The specified iterator pointer identifies a meta-fragment in the terrain-tile adjacent to this one
			at the specified Von Neumann neighborhood neighbor location
		@param ennNeighbor Identifies where the specified meta-fragment iterator occurs adjacent to this terrain-tile
		@param i The iterator pointer to a meta-fragment in an adjacent terrain-tile
		*/
		void linkNeighbor(const VonNeumannNeighbor ennNeighbor, MetaFragMap::iterator i);

		/** Unlinks all meta-fragments linked to the specified neighbor 
		@remarks This method will work for terrain-tiles that do not occur along a page border, but it is intended
			only for those because this method must be called from the main thread and unlinking of internal terrain-
			tiles is unnecessary since they are 99% of the time being disposed of in batch anyhow
		@param ennNeighbor Neighbor to unlink from
		*/
		void unlinkPageNeighbor(const VonNeumannNeighbor ennNeighbor);

		/** Initializes the specified internal neighbor
		@remarks Each terrain-tile keeps an additional auxilary set of neighbors linking terrain-tile objects distinct 
			from the primary ones previously described that link meta-fragments.  One difference is that these
			internal neighbors do not link-up with terrain-tiles in pages other than this one (the one hosting them).
		@param n Identifies where the specified terrain-tile occurs in the neighborhood
		@param t The neighbor terrain-tile to link-up with this one
		*/
		void initNeighbor(const VonNeumannNeighbor n, TerrainTile * t );

		/// Determines if this terrain tile has been initialized yet with a call to the 'initialise' method
		bool isInitialised () const;
		/// Determines if this terrain tile has been parameterized yet with a call to the overloaded operator << (const PageInitParams::TileParams &) method yet
		bool isParameterized () const;

		/// Sets the render queue group of all isosurface renderables hosted by this terrain-tile
		void setRenderQueueGroup(uint8 qid);
		/** Retrieves the bounding box of this terrain tile
		@remarks 
			The coordinates are lazy-generated from the heightmap bbox that is generated during initialization and accounts
			for meta-fragments.  Therefore the altitude of the bounding box is constricted by the heightmap slice 
			intersecting with this terrain-tile unless there are meta-fragments that occur outside those bounds effectively 
			enlarging the bounding region.
			The bounding box is relative to the page center and can be requested in any coordinate space.
		@param encsTo Requested coordinate space of the bounding box, default is world coordinates
		@returns A bounding box relative to the center of the page in the specified coordinate space
		*/
		const AxisAlignedBox& getBBox(const OverhangCoordinateSpace encsTo = OCS_World) const;

		/** Retrieves the bounding box of the portion of the heightmap enclosed by this terrain-tile
		@remarks 
			The bounding box does not account for any meta-fragments in the terrain-tile, it is representative
			of the portion of the heightmap enclosed by this terrain tile constricted precisely to the minimum 
			and maximum altitudes in the heightmap portion.
			The bounding box is relative to the page center and can be requested in any coordinate space.
		@param encsTo Requested coordinate space of the bounding box, default is world coordinates
		@returns A bounding box relative to the center of the page in the specified coordinate space
		*/
		const AxisAlignedBox& getHeightMapBBox(const OverhangCoordinateSpace encsTo = OCS_World) const;

		// Retrieves the position aligned to the horizontal plane based at upper-left corner of the terrain-tile in the specified coordinate space
		const Vector3& getPosition(const OverhangCoordinateSpace encsTo = OCS_World) const;
		// Retrieves the position aligned to the horizontal plane based at the center of the terrain-tile in the specified coordinate space
		const Vector3& getCenter(const OverhangCoordinateSpace encsTo = OCS_World) const;

		/** Parameterizes this terrain-tile
		@remarks Applies the heightmap portion enclosed by this terrain-tile to this terrain-tile effectively
			computing the bounding box using the minimum and maximum altitude occurring in the heightmap portion
		@see getHeightMapBBox(...)
		*/
		void TerrainTile::operator << ( const PageInitParams::TileParams & params );

		/// Initializes this terrain-tile and its meta-fragments binding it to the specified scene node
		void initialise(SceneNode * pParentSceneNode);

		/// Builds meta-fragments for the space covered by the portion of the heightmap enclosed by this terrain-tile
		void voxelise();

		// Walks all child renderables detaching them from the scene node, then destroys its scene node
		void detachFromScene();

		/// Sets the OGRE material for all isosurface renderables in this terrain-tile's meta-fragments
		void setMaterial(const MaterialPtr& m );
		/** Returns the terrain height at the given coordinates clamped to the page bounds */
		Real height(const signed x, const signed y) const;

		/// Binds the meta-ball to all meta-fragments it intersects and queues it for updating the voxel grid when the main thread gets around to it
		void addMetaBall(MetaBall * const pMetaBall);
		/// Used to restore a set of meta-objects from disk, called by the page during a load operation
		void loadMetaObject(MetaObject * const pMetaObject);

		/// Returns the total number of meta-fragments in this terrain-tile
		inline
		size_t countFrags() const
		{
			oht_assert_threadmodel(ThrMdl_Single);
			return _mapMF.size();
		}

		/// Determines if there is at least one meta-fragment in this terrain-tile
		inline
		bool hasMetaFrags() const
		{
			oht_assert_threadmodel(ThrMdl_Single);
			return !_mapMF.empty();
		}

		/// Begin iteration over the list of meta-fragments in this terrain-tile
		MetaFragMap::const_iterator beginFrags () const;
		/// Last iterator pointer to the list of meta-fragments in this terrain-tile
		MetaFragMap::const_iterator endFrags() const;

		/// Retrieves the OhTSM manager singleton for managing paged-terrain in the scene
		const OverhangTerrainManager & getManager() const;

		/** Processes all previously queued meta-objects (e.g. as with addMetaBall(...))
		@remarks Prepares all meta-fragments necessary to render content from the meta-objects and optionally updates the voxel grid.
			This must be called from the main thread.
			The update flag is used to conditionally update the voxel grid because it is a CPU-intensive process.
		@param bUpdate True to update voxel-grids as well (expensive), false to just prepare the dependent meta-fragments (cheap)
		*/
		void commitOperation(const bool bUpdate = true);

		/// Updates the 3D voxel grids of every meta-fragment in this terrain-tile
		void updateVoxels();

		/** Compute a ray intersection against meta-fragments in this terrain-tile
		@remarks Traverses meta-fragments in the page starting at this terrain-tile.  When the ray traversal crosses this terrain-tile's 
			boundaries it selects the adjacent terrain-tile and recursively traverses that one calling this method again if the cascade flag 
			is set.  The result of the query is stored in the specified result parameter and method return value.
			The search would complete if either the ray hit something or the distance limit was reached or the page boundaries were reached.
			Must be called from the main thread.
		@param result The result of the ray query is stored here
		@param rayPageRelTerrainSpace The ray in terrain-space (i.e. OverhangCoordinateSpace::OCS_Terrain) relative to the page center
		@param cascadeToNeighbours Whether or not to recursively cascade the search operation to neighboring terrain-tiles in the same page 
			or to terminate after the terrain-tile borders have been reached
		@param distanceLimit The maximum length of the ray to search treating the ray as a line segment to eliminate the inevitable infinite
			search for true orthodox rays or specify zero for no limit
		@returns True if the intersection passed and a triangle in a surface was intersected by the ray, false if nothing was hit
		*/
		bool rayIntersects(
			OverhangTerrainManager::RayResult & result, 
			const Ray& rayPageRelTerrainSpace, 
			bool cascadeToNeighbours = false, 
			Real distanceLimit = 0
		) const;

		/// Persists this terrain-tile and its meta-fragments to the stream, can be background
		StreamSerialiser & operator >> (StreamSerialiser & output) const;
		/// Restores this terrain-tile and meta-fragments from the stream, can be background
		StreamSerialiser & operator << (StreamSerialiser & input);

	protected:
		/** Computes the meta-fragment Y-level from the specified world coordinate
		@remarks Computes the Y-level which is its vertical world coordinate component divided by voxel cube region dimensions */
		// TODO: If page bounding box ever changes, callers to this will break.  It assumes page local z
		YLevel computeYLevel (const Real z) const;

		/** Computes the position in some space given a Y-level based on this terrain-tile's position
		@param yl The Y-level to influence position
		@param encsTo The coordinate space to return the position in 
		@returns A position in the specified coordinate space relative to the page as a multiple of the specified Y-level */
 		Vector3 getYLevelPosition (const YLevel yl, const OverhangCoordinateSpace encsTo = OCS_Terrain) const;
		
		/// Creates or retrieves a meta-fragment in this terrain-tile at the specified Y-level which is analogous to a vertical coordinate
		MetaFragment::Container * acquireMetaWorldFragment (const YLevel yl);

		/// Destroys any lazy-generated information such as bounding boxes
		void dirty();

		/** Initializes the specified meta-fragment
		@remarks Initializes the meta-fragment, creates a new scene node linked to the specified one, positions it, binds the fragment's 
			isosurface renderable to it, fires the onInitMetaRegion event.
		@param pParentSceneNode Used to create a child scene node that the fragment's renderable will be bound to
		@param basic Read=only access facet to basic properties of the meta-fragment
		@param builder Initialization access facet to the meta-fragment */
		void initialiseMWF( SceneNode * pParentSceneNode, MetaFragment::Interfaces::const_Basic & basic, MetaFragment::Interfaces::Builder & builder);

		// Retrieves the meta-heightmap metaobject that all meta-fragments of this terrain-tile are setup with when created
		MetaHeightMap * getMetaHeightMap () { return _pMetaHeightmap; }

		/** Adds the specified meta-fragment to this terrain-tile
		@remarks Does not initialize the meta-fragment, only links it to an internal vector object, so implementors must still call applyFragment.
		@param fragment Exclusive access for adding the meta-heightmap metaobject to the meta-fragment
		@param pMWF A pointer to the meta-fragment itself 
		@returns An iterator pointer to the newly-inserted meta-fragment in this terrain-tile's internal vector object */
		MetaFragMap::iterator attachFragment( MetaFragment::Interfaces::Unique & fragment, MetaFragment::Container * pMWF );

		/** Initializes / prepares the specified meta-fragment
		@remarks Ensures that the fragment is fully initialized and attached to the world, does nothing if it already is.
			Links-up neighbors from within the same page.  Must be called from the main thread.
		@param basic Used to read the fragment's Y-level
		@param builder Initialization access to the meta-fragment in-case it must be initialized */
		void applyFragment ( MetaFragment::Interfaces::const_Basic & basic, MetaFragment::Interfaces::Builder & builder );

	private:
		/// Pointer to the meta-heightmap metaobject added to every meta-fragment of this terrain-tile when it's created
		MetaHeightMap * _pMetaHeightmap;
		/// Material that every meta-fragment is set with
		MaterialPtr _pMaterial;

		/// Array of neighbors adjacent to this terrain-tile within the same page
		TerrainTile *_vpInternalNeighbors [ CountVonNeumannNeighbors ];
		/// Flag indicating whether the 'initialise' method has been called
		bool _bInit, 
		/// Flag indicating whether the overloaded operator << ( const PageInitParams::TileParams & ) method has been called
			_bParameterized;
		/// Main top-level OhTSM configuration properties
		const OverhangTerrainOptions & _options;
		/// Pointer to the hosting page, (provides indirect access to a PageSection object)
		PagePrivateNonthreaded * _pPage;

		/// Meta-fragments from bottom to top (y direction) indexed by Y-level.
		MetaFragMap _mapMF;

		/// Class used to queue a set of meta-fragments that require updated voxel grids
		class DirtyMF
		{
		private:
			/// Basic shared access to this meta-fragment because changes cannot be made until it has been processed out of the queue
			const MetaFragment::Interfaces::Basic * _immutable;

		public:
			/// Only one status is supported right now
			const
			enum Status
			{
				Dirty
			} status;
			
			/// The meta-fragment itself that requires processing
			MetaFragment::Container * const mwf;

			DirtyMF (MetaFragment::Container * pMWF, const Status enStatus);
			DirtyMF(const DirtyMF & copy);
			DirtyMF(DirtyMF && move);
			~DirtyMF();

			bool operator < (const DirtyMF & other) const;
		};
		typedef std::set< DirtyMF > DirtyMWFSet;

		/// Set of meta-fragments that require updated voxel grids
		DirtyMWFSet * _psetDirtyMF;

		/// Lazy-generated set of bounding boxes for each coordinate system type that account for heightmap and meta-fragments
		mutable AxisAlignedBox * _vpBBox[NumOCS];
		/// Set of bounding-boxes for each coordinate system type that account excuslively for heightmap
		AxisAlignedBox _vHMBBox[NumOCS];
		/// Set of position vectors for each coordinate system type for the center position of this terrain-tile
		Vector3 _vCenterPos[NumOCS];
		/// 2D index for this terrain-tile in its page
		unsigned _x0, _y0;

		/** Returns the terrain-tile adjacent to this one in the direction consistent with the specified ray
		@param rayPageRelTerrainSpace The ray relative to the page center in terrain-space (OverhangCoordinateSpace::OCS_Terrain)
		@param distanceLimit The maximum length of the ray to search treating the ray as a line segment to eliminate the inevitable infinite
			search for true orthodox rays or specify zero for no limit
		@returns The terrain-tile adjacent to this terrain-tile in the direction that the ray is aiming at or NULL if the page boundaries have been reached */
		const TerrainTile* raySelectNeighbour(const Ray& rayPageRelTerrainSpace, Real distanceLimit = 0) const;

		/** Tests whether the ray intersects a triangle in one of this terrain-tile's meta-fragment's isosurfaces
		@remarks The search starts from a specific meta-fragment of this terrain-tile found by a previous call to rayIntersects.  The search continues until
			either the terrain-tile boundary has been reached or the distance limit has been reached.
		@param result This will be populated with the results of the query
		@param rayPageRelTerrainSpace The ray we're searching with in terrain-space (OverhangCoordinateSpace::OCS_Terrain) relative to the page center
		@param tile Ray offset where the ray touches the first meta-fragment in this terrain-tile
		@param nLimit The maximum length of the ray to search treating the ray as a line segment to eliminate the inevitable infinite
			search for true orthodox rays or specify zero for no limit
		@returns A pair of values, the first being a boolean indicating whether the ray intersected a triangle or not, and the second being the distance 
			in world-space from the point where the ray crossed this terrain-tile's border to the point of triangle intersection.
		*/
		std::pair< bool, Real > rayIntersectsMetaWorld(
			OverhangTerrainManager::RayResult & result, 
			const Ray & rayPageRelTerrainSpace, 
			const Real tile, 
			const Real nLimit
		) const;

	protected:

		/** Creates or retrieves, prepares, initializes and updates the 3D voxel grids of the meta-fragments affected by the metaobject.
		@remarks The specified metaobject is added to each of the meta-fragments it covers in this terrain-tile.  
			Meta-fragments are created, prepared, and initialized as necessary to accommodate the metaobject.
			The 3D voxel grids are updated for all of these meta-fragments to reflect the new metaobject.
		@param queues All meta-fragments affected by the metaobject are placed into this queue object
		@param pMetaObject The metaobject that will be applied to this terrain-tile	*/
		void propagateMetaObject (DirtyMWFSet & queues, MetaObject * const pMetaObj);
	};
}//namespace OGRE
#endif// _OGRE_TERRAIN_TILE_H_
