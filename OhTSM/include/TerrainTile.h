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

#include <vector>

#include "Neighbor.h"
#include "ChannelIndex.h"

#include "OverhangTerrainPrerequisites.h"

#include "OverhangTerrainManager.h"
#include "OverhangTerrainPageInitParams.h"

#include "DebugTools.h"

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
		@param descchann The channel descriptor
		@param page The parent page within which this terrain-tile is contained
		@param opts The main top-level OhTSM configuration options
		*/
		TerrainTile(const size_t p, const size_t q, const Channel::Descriptor & descchann, PagePrivateNonthreaded * const page, const OverhangTerrainOptions & opts);
		~TerrainTile();

		/** Links up all meta-fragment neighbors in the terrain-tile
		@remarks Links-up meta-fragment neighbors with meta-fragments in surrounding terrain-tiles provided they are
			all within the same page, and with meta-fragments above and below within this terrain-tile
		*/
		void linkUpAllSurfaces();
		
		/** Links-up meta-fragments in this terrain-tile with corresponding ones in another one
		@remarks Links-up all meta-fragments in this terrain-tile of the specified channel that
			correspond to meta-fragments in the specified terrain-tile of the same channel according
			to Y-level.
		@param ennNeighbor Identifies where the specified terrain-tile occurs adjacent to this one
		@param pNeighborTile The neighbor tile to which meta-fragments here-in will be linked
		*/
		void linkNeighbor(const VonNeumannNeighbor ennNeighbor, TerrainTile * pNeighborTile);

		/** Links-up a meta-fragment with a corresponding meta-fragment in this terrain tile
		@remarks The specified iterator pointer identifies a meta-fragment in the terrain-tile adjacent to this one
			at the specified Von Neumann neighborhood neighbor location for the specified channel
		@param ennNeighbor Identifies where the specified meta-fragment iterator occurs adjacent to this terrain-tile
		@param channel The channel of the meta-fragment within which to also link an associated meta-fragment
		@param i The iterator pointer to a meta-fragment in an adjacent terrain-tile
		*/
		void linkNeighbor(const VonNeumannNeighbor ennNeighbor, const Channel::Ident channel, MetaFragMap::iterator i);

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

		/// Retrieves the horizontally-aligned (aligned to the "ground") bounding-box for this terrain tile
		const BBox2D & getTileBBox () const { return _bbox; }
		/// The horizontally-aligned (aligned to the "ground") position of the tile's center
		const Vector2 & getTilePos () const { return _pos; }

		/// Determines if this terrain tile has been initialized yet with a call to the 'initialise' method
		bool isInitialised () const;
		/// Determines if this terrain tile has been parameterized yet with a call to the overloaded operator << (const PageInitParams::TileParams &) method yet
		bool isParameterized () const;

		/// Sets the render queue group of all isosurface renderables in the specified channel hosted by this terrain-tile
		void setRenderQueueGroup(const Channel::Ident channel, uint8 qid);

		/** Parameterizes this terrain-tile
		@remarks Applies the heightmap portion enclosed by this terrain-tile to this terrain-tile effectively
			computing the bounding box using the minimum and maximum altitude occurring in the heightmap portion
		@see getHeightMapBBox(...)
		*/
		void TerrainTile::operator << ( const PageInitParams::TileParams & params );

		/// Initializes this terrain-tile and its meta-fragments binding it to the specified scene node
		void initialise(SceneNode * pParentSceneNode);

		/// Builds meta-fragments for the space covered by the portion of the heightmap enclosed by this terrain-tile, binds the heightmap to all meta-fragments
		void voxeliseTerrain();

		/// Unlinks the heightmap from all meta-fragments in the terrain channel
		void unlinkHeightMap();

		// Walks all child renderables detaching them from the scene node, then destroys its scene node
		void detachFromScene();

		/// Sets the OGRE material for all isosurface renderables of the specified channel in this terrain-tile's meta-fragments
		void setMaterial(const Channel::Ident channel, const MaterialPtr& m );

		/// Binds the meta-ball to all meta-fragments it intersects of the specified channel and queues it for updating the voxel grid when the main thread gets around to it
		void addMetaObject(const Channel::Ident channel, MetaObject * const pMetaObject);
		/// Used to restore a set of meta-objects from disk for the specified channel, called by the page during a load operation
		void loadMetaObject(const Channel::Ident channel, MetaObject * const pMetaObject);

		/// Returns the total number of meta-fragments in the specified channel of this terrain-tile
		inline
		size_t countFrags(const Channel::Ident channel) const
		{
			oht_assert_threadmodel(ThrMdl_Single);
			Channel::Index< MetaFragMap >::const_iterator j = _index2mapMF.find(channel);

			return j == _index2mapMF.end() ? 0 : j->value->size();
		}

		/// Determines if there is at least one meta-fragment in the specified channel of this terrain-tile
		inline
		bool hasMetaFrags(const Channel::Ident channel) const
		{
			oht_assert_threadmodel(ThrMdl_Single);
			return _index2mapMF.find(channel) == _index2mapMF.end() || !_index2mapMF[channel] .empty();
		}

		/// Begin iteration over the list of meta-fragments in the specified channel of this terrain-tile
		MetaFragMap::const_iterator beginFrags (const Channel::Ident ident) const;
		/// Last iterator pointer to the list of meta-fragments in the specified channel of this terrain-tile
		MetaFragMap::const_iterator endFrags(const Channel::Ident ident) const;

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
			The search would complete if either the ray hit something or the distance limit of the parameters object was reached or the page
			boundaries were reached.  The query is limited to the channels specified in the parameters object.  Must be called from the main thread.
		@param result The result of the ray query is stored here
		@param params Parameters influencing the ray query including the set of channels to restrict the query to and the maximum length of
			the ray to search treating the ray as a line segment to eliminate the inevitable infinite search for true orthodox rays or
			specify zero for no limit
		@param distance The distance traversed so far in this query since the first terrain-tile in the parent page was queried
		@param i The ray iterator for traversing terrain tiles and metafragments within this page initialized with the page-local world-space ray
		@returns True if the intersection passed and a triangle in a surface was intersected by the ray, false if nothing was hit
		*/
		bool rayIntersects(
			OverhangTerrainManager::RayResult & result, 
			const OverhangTerrainManager::RayQueryParams & params,
			const Real distance,
			DiscreteRayIterator & i
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

		/** Computes the bounding region of a voxel cube in some space given a Y-level based on this terrain-tile's position
		@param yl The Y-level to influence the bounds
		@param encsTo The coordinate space to return the bounds in
		@returns A bounding box of the region in the specified coordinate space relative to the page as a multiple of the specified Y-level */
		AxisAlignedBox getYLevelBounds (const YLevel yl, const OverhangCoordinateSpace encsTo = OCS_Terrain) const;
		
		/// Creates or retrieves a meta-fragment in the specified channel of this terrain-tile at the specified Y-level which is analogous to a vertical coordinate
		MetaFragment::Container * acquireMetaWorldFragment (const Channel::Ident channel, const YLevel yl);

		/** Initializes the specified meta-fragment
		@remarks Initializes the meta-fragment, creates a new scene node linked to the specified one, positions it, binds the fragment's 
			isosurface renderable to it, fires the onInitMetaRegion event.
		@param channel The channel of the meta-fragment
		@param pParentSceneNode Used to create a child scene node that the fragment's renderable will be bound to
		@param basic Read=only access facet to basic properties of the meta-fragment
		@param builder Initialization access facet to the meta-fragment */
		void initialiseMWF( const Channel::Ident channel, SceneNode * pParentSceneNode, MetaFragment::Interfaces::const_Basic & basic, MetaFragment::Interfaces::Builder & builder);

		/** Adds the specified meta-fragment to the specified channel of this terrain-tile
		@remarks Does not initialize the meta-fragment, only links it to an internal vector object associated with the channel, so implementors must still call applyFragment.
		@param channel The channel of the meta-fragment
		@param fragment Exclusive access for adding the meta-heightmap metaobject to the meta-fragment
		@param pMWF A pointer to the meta-fragment itself 
		@returns An iterator pointer to the newly-inserted meta-fragment in this terrain-tile's internal vector object */
		MetaFragMap::iterator attachFragment( const Channel::Ident channel, MetaFragment::Interfaces::Unique & fragment, MetaFragment::Container * pMWF );

		/** Initializes / prepares the specified meta-fragment
		@remarks Ensures that the fragment is fully initialized and attached to the world, does nothing if it already is.
			Links-up neighbors from within the same page of the same channel.  Must be called from the main thread.
		@param channel The channel of the meta-fragment
		@param basic Used to read the fragment's Y-level
		@param builder Initialization access to the meta-fragment in-case it must be initialized */
		void applyFragment ( const Channel::Ident channel, MetaFragment::Interfaces::const_Basic & basic, MetaFragment::Interfaces::Builder & builder );

	private:
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

		/// Meta-fragments from bottom to top (y direction) indexed by Y-level distributed among channels
		Channel::Index< MetaFragMap > _index2mapMF;

		/// Various channel-specific properties to maintain
		struct ChannelProperties
		{
			/// Current render queue ID for the channel
			uint8 renderq;
			/// Current OGRE material for renderables of the channel
			MaterialPtr material;

			ChannelProperties();
		};
		/// Index of channel-specific properties
		Channel::Index< ChannelProperties > _properties;

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

		/// Horizontally-aligned (aligned to the "ground") bounding-box for this terrain tile
		BBox2D _bbox;
		/// Horizontally-aligned (aligned to the "ground") position of the tile's center
		Vector2 _pos;

		typedef std::set< DirtyMF > DirtyMWFSet;
		/// Set of meta-fragments that require updated voxel grids distributed by channel
		Channel::Index< DirtyMWFSet > _dirtyMF;

		/// 2D index for this terrain-tile in its page
		unsigned _x0, _y0;

	protected:

		/** Creates or retrieves, prepares, initializes and updates the 3D voxel grids of the meta-fragments of the specified channel affected by the metaobject.
		@remarks The specified metaobject is added to each of the meta-fragments it covers in the specifid channel of this terrain-tile.
			Meta-fragments are created, prepared, and initialized as necessary to accommodate the metaobject.
			The 3D voxel grids are updated for all of these meta-fragments to reflect the new metaobject.
		@param channel The channel to propagate the meta-object into
		@param queues All meta-fragments affected by the metaobject are placed into this queue object
		@param pMetaObject The metaobject that will be applied to this terrain-tile	*/
		void propagateMetaObject (const Channel::Ident channel, DirtyMWFSet & queues, MetaObject * const pMetaObj);
	};
}//namespace OGRE
#endif// _OGRE_TERRAIN_TILE_H_
