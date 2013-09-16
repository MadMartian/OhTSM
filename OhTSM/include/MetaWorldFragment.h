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

#ifndef WORLD_FRAGMENT_H
#define WORLD_FRAGMENT_H

/***************************************************************
A MetaFragment is the basic building block of  the world - 
it has its own IsoSurface.

When created an IsoSurface should be created...
The MetaFragment will also contain portals to other
MetaFragments for visibility culling (TODO).
****************************************************************/

#include <vector>
#include <deque>
#include <map>
#include <set>

#include <OgreStreamSerialiser.h>
#include <OgreCamera.h>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>

#include "OverhangTerrainPrerequisites.h"

#include "Types.h"
#include "CubeDataRegion.h" // TODO: Remove once migrated Voxel::DataAccessor::EmptySet
#include "IsoSurfaceSharedTypes.h"
#include "MetaObject.h"
#include "Neighbor.h"
#include "MetaFactory.h"

namespace Ogre
{
	/** Meta fragments are exposed through various thread-safe interfaces.  The interface depens on the type 
		of desired access and the overall state of the object.  The state of the object exists in either 
		initialized state or uninitialized state. */
	namespace MetaFragment
	{
		/// Facet of a meta fragment that contains members that are valid after initialization phase
		class _OverhangTerrainPluginExport Post
		{
		public:
			/// The meta-fragment's isosurface renderable
			IsoSurfaceRenderable * surface;
			/// Custom data managed by a custom provider
			ISerializeCustomData * custom;
			/// The discrete voxel cube region owned by the meta fragment
			Voxel::CubeDataRegion * const block;
			/// Y-level of the fragment
			YLevel ylevel;

			Post(Voxel::CubeDataRegion * pCubeDataRegoin, const YLevel & ylevel = YLevel());
		};

		/// Facet of a meta fragment that contains most members
		class _OverhangTerrainPluginExport Core : protected Post
		{
		private:
			/// Render manager for synchronizing iso-surface rendering
			RenderManager * _pRendMan;
			/// Flag to indicate whether the hardware buffers should be reset (cleared and repopulated) whenever the main thread gets around to it
			bool _bResetting;
			/// OGRE scene node to which the isosurface is bound
			SceneNode * _pSceneNode;
			/// List of meta-objects that makes-up this fragment's discrete sample voxel grid
			MetaObjsList _vMetaObjects;

			/// Surrounding meta fragment neighbors in the scene
			Core * _vpNeighbors[CountOrthogonalNeighbors];

			/// Material of the isosurface renderable stored as strings name and group
			struct MaterialInfo
			{
				String name, group;
			} * _pMatInfo;

			/// Material of the isosurface (late-bound)
			MaterialPtr _pMaterial;

			/// Previous WorkQueue request ID (if any) for updating the isosurface
			WorkQueue::RequestID _ridBuilderLast;
			/// LOD of the previous request
			unsigned char _nLOD_Requested0;
			/// Stitch flags of the previous request
			Touch3DFlags _enStitches_Requested0;

		public:
			/// Factory singleton for creating new objects of the associated channel
			const Voxel::MetaVoxelFactory * const factory;

			/**
			@param pRendMan The render manager used to synchronize iso-surface rendering
			@param pFactory The meta factory singleton for creating various new objects of the associated channel
			@param pBlock The 3D voxel grid bound to the meta fragment
			@param ylevel the Y-level of the meta fragment
			*/
			Core(RenderManager * pRendMan, const Voxel::MetaVoxelFactory * pFactory, Voxel::CubeDataRegion * pBlock, const YLevel & ylevel);
			virtual ~Core();

		public: // Simple accessors
			bool isInitialized() const;

		public: // Builder phase
			/** Initializes the meta fragment by creating the isosurface renderable and binding it to the scene
			@param pPrimaryCam The primary camera used by the scene
			@parma pSceneNode The scene node that the isosurface renderable will be bound to
			@param sSurfName Mandatory OGRE name for the isosurface renderable
			*/
			void initialise( const Camera * pPrimaryCam, SceneNode * pSceneNode, const String & sSurfName /*= StringUtil::BLANK*/ );

			/// Bind a new camera to the isosurface renderable
			void bindCamera( const Camera * pCam );

			/** Link-up a neighbor 
			@remarks Links-up this meta fragment to a neighbor at the specified side, and it is unnecessary to call the method
				again for the neighboring fragment because the operation is dual for both fragments to each other
			@param on The disposition of the neighbor relative to this meta fragment
			@param pMF The neighbor meta fragment to link-up
			*/
			void linkNeighbor(const OrthogonalNeighbor on, MetaFragment::Container * pMF);

			/// Retrieves the material bound to the isosurface renderable
			const MaterialPtr & getMaterial () const;
			/// Sets the material bound to the isosurface renderable
			void setMaterial (const MaterialPtr & pMat);

			/// Used for serialization
			StreamSerialiser & operator >> (StreamSerialiser & output) const;
			StreamSerialiser & operator << (StreamSerialiser & input);

			/// Unlinks a neighbor at the specified border, operation is dual and this method need not be called for the neighbor
			void unlinkNeighbor(const VonNeumannNeighbor vnnNeighbor);

			/// Detatches the isosurface renderable from the scene
			void detachFromScene();

		public: // Mutation phase
			/** Perform a ray query on this meta fragment
			@param ray The ray we're conducting the query for
			@param limit The maximum distance away from the ray origin to query
			*/
			std::pair< bool, Real > rayQuery(const Ray & ray, const Real limit);

			/** Rebuild the isosurface renderable
			@remarks This does not actually do any heavy-lifting, it just sets flags in preparation for rebuilding the isosurface which is done elsewhere
			*/
			void updateSurface();
			/// Resamples all meta-objects and rebuilds the discrete 3D voxel field
			Voxel::DataAccessor::EmptySet updateGrid();

			/** Generates an isosurface configuration for the specified LOD and stitch flags
			@remarks Synchronously invokes IsoSurfaceBuilder to extract a new isosurface by discretely sampling a 3D voxel field.
				Does not generate a configuration if one already exists, can potentially recreate the hardware buffers
			@param nLOD The LOD of the new surface to generate
			@param enStitches The stitch flags and hence transition cells required for the new surface
			@returns Always returns true
			*/
			bool generateConfiguration( const unsigned nLOD, const Touch3DFlags enStitches );
			/** Requests an isosurface configuration for the specified LOD and stitch flags
			@remarks Asynchronously queues a request to invoke IsoSurfaceBuilder to extract a new isosurface by discretely sampling a 3D voxel field.
				Does not generate a configuration if one already exists, can potentially recreate the hardware buffers.
				Does not always successfully queue a request, depends on whether dependent resources are available or occupied.
			@param nLOD The LOD of the new surface to generate
			@param enStitches The stitch flags and hence transition cells required for the new surface
			@returns True if the request was queued, false if dependent resources were occupied
			*/
			bool requestConfiguration( const unsigned nLOD, const Touch3DFlags enStitches );

			/// Adds a metaobject to this world fragment, does not update the 3D voxel grid
			void addMetaObject( MetaObject * const mo );
			/// Searches for and removes the specified meta-object, returns true if found and removed
			bool removeMetaObject( const MetaObject * const mo );
			/// Removes all metaobjects from this world fragment, does not update the 3D voxel grid
			void clearMetaObjects();

		public: // Query phase

			/// Begin iterator pattern to the list of meta objects, element type is a MetaObject pointer
			inline
				MetaObjsList::const_iterator beginMetas() const { return _vMetaObjects.begin(); }

			/// End iterator pattern to the list of meta objects, element type is a MetaObject pointer
			inline
				MetaObjsList::const_iterator endMetas () const { return _vMetaObjects.end(); }

			/// @returns True if there are no metaobjects in this world fragment
			inline
				bool empty() const { return _vMetaObjects.empty(); }

			/// Determines which of this meta-fragment's neighbors have higher resolution and require stitching (transition cells)
			Touch3DFlags getNeighborFlags( const unsigned nLOD ) const;

			/// Retrieve a const neighbor
			const Container * neighbor(const Moore3DNeighbor enNeighbor) const;

			// Retrieve a mutable neighbor
			Container * neighbor(const Moore3DNeighbor enNeighbor);
		};

		/// Set of thread-safe interfaces that behave as distinct facets to the nature of a meta-fragment
		namespace Interfaces
		{
			/// Base class for an exlcusive lock thread-safe interface
			template< typename LOCKTYPE >
			class template_Lockable
			{
			public:
				typedef LOCKTYPE LockType;

			protected:
				LOCKTYPE _lock;

			private:
				template_Lockable(const template_Lockable &);

			public:
				template_Lockable(template_Lockable && move)
					: _lock(static_cast< LOCKTYPE && > (move._lock)) {}
				explicit template_Lockable(boost::shared_mutex & m)
					: _lock(m) {}
			};

			/// Base class for an exclusive lock that used to be a shared lock (i.e. was previously upgraded) thread-safe interface
			template< typename UPLOCKTYPE, typename SHAREDLOCKTYPE >
			class template_LockUpgradable
			{
			public:
				typedef UPLOCKTYPE UpgradedLockType;
				typedef SHAREDLOCKTYPE SharedLockType;

			protected:
				UPLOCKTYPE _lock;

			private:
				template_LockUpgradable(const template_LockUpgradable &);

			public:
				template_LockUpgradable(template_LockUpgradable && move)
					: _lock(static_cast< UPLOCKTYPE && > (move._lock)) {}
				explicit template_LockUpgradable(SHAREDLOCKTYPE & l)
					: _lock(l) {}
			};

			typedef template_Lockable< boost::shared_lock< boost::shared_mutex > > SharedLockable;
			typedef template_Lockable< boost::unique_lock< boost::shared_mutex > > UniqueLockable;
			typedef template_Lockable< boost::upgrade_lock< boost::shared_mutex > > UpgradableLockable;
			typedef template_LockUpgradable< boost::upgrade_to_unique_lock< boost::shared_mutex >, boost::upgrade_lock< boost::shared_mutex > > UpgradedLockable;

			/// Read-only facet of a meta-fragment
			class ReadOnly_facet
			{
			protected:
				const Core * const _core;

			public:
				ReadOnly_facet(ReadOnly_facet && move);
				ReadOnly_facet(const Core * pCore, const Post * pPost);

				/// @see Core::getNeighborFlags(...)
				inline
					Touch3DFlags getNeighborFlags( const unsigned nLOD ) const { return _core->getNeighborFlags(nLOD); }
			};

			/// Base class for any basic facet to a meta-fragment
			template< typename T_Core, typename T_Post, typename T_IsoSurfaceRenderable >
			class Basic_base
			{
			private:
				const T_Core * const _core;

			public:
				/// Y-level of the meta-fragment
				const YLevel & ylevel;
				/// Meta-fragment's isosurface renderable
				T_IsoSurfaceRenderable * const & surface;
				/// Meta-fragment's attached 3D voxel grid
				Voxel::CubeDataRegion * const & block;

				Basic_base(Basic_base && move)
					: ylevel(move.ylevel), surface(move.surface), block(move.block), _core(move._core) {}

				Basic_base(T_Core * pCore, T_Post * pPost)
					: _core(pCore), surface(pPost->surface), ylevel(pPost->ylevel), block(pPost->block) {}

				template< typename Basic_copy >
				Basic_base(const Basic_copy & copy)
					: surface(copy.surface), ylevel(copy.ylevel), block(copy.block), _core(copy._core) {}

				/// @see Core::isInitialized()
				bool isInitialized () const { return _core->isInitialized(); }

				/// @see Core::neighbor()
				const MetaFragment::Container * neighbor(const Moore3DNeighbor enNeighbor) const { return _core->neighbor(enNeighbor); }

				friend Basic_base< const Core, const Post, const IsoSurfaceRenderable >;
			};

			/// Mutable basic interface
			class _OverhangTerrainPluginExport Basic : public Basic_base< Core, Post, IsoSurfaceRenderable >
			{
			public:
				Basic(Core * pCore, Post * pPost);
			};

			/// Read-only (const) basic interface
			class _OverhangTerrainPluginExport const_Basic : public Basic_base< const Core, const Post, const IsoSurfaceRenderable >
			{
			public:
				const_Basic(const Core * pCore, const Post * pPost);
				const_Basic(const Basic & copy);
			};

			/// Provides the basis for the rest of the facets: unique, shared[upgrade], builder
			template< typename Core, typename Post, typename IsoSurfaceRenderable, typename ISerializeCustomData >
			class template_Base : public ReadOnly_facet, public Basic_base< Core, Post, IsoSurfaceRenderable >
			{
			protected:
				Core * const _core;

			public:
				/// Custom data managed by a custom provider
				ISerializeCustomData & custom;

				template_Base(template_Base && move)
					:	ReadOnly_facet(static_cast< ReadOnly_facet && > (move)),
						Basic_base(static_cast< Basic_base && > (move)),
						_core(move._core), custom(move.custom) {}

				template_Base(Core * pCore, Post * pPost)
					: ReadOnly_facet(pCore, pPost), Basic_base(pCore, pPost), _core(pCore), custom(pPost->custom) {}

				/// @see Core::beginMetas()
				inline
					MetaObjsList::const_iterator beginMetas() const { return _core->beginMetas(); }

				/// @see Core::endMetas()
				inline
					MetaObjsList::const_iterator endMetas () const { return _core->endMetas(); }

				/// @see Core::empty()
				inline
					bool empty() const { return _core->empty(); }

				/// @see Core::operator >> (...)
				inline
					StreamSerialiser & operator >> (StreamSerialiser & output) const
						{ return *_core >> output; }
			};

			typedef template_Base< const Core, const Post, const IsoSurfaceRenderable, const ISerializeCustomData * const > const_Base;
			typedef template_Base< Core, Post, IsoSurfaceRenderable, ISerializeCustomData * > Base;

			/// Facet for shared locks on the meta-fragment exposing properties read-only
			class _OverhangTerrainPluginExport Shared 
				:	public SharedLockable,
					public const_Base
			{
			public:
				Shared (Shared && move);
				Shared (boost::shared_mutex & m, const Core * pCore, const Post * pPost);
			};

			/// The basis for all facets that require an exclusive (potentially mutable access) lock on the meta-fragment
			class Unique_base 
				:	public Base
			{
			public:
				Unique_base (Unique_base && move);
				Unique_base (Core * pCore, Post * pPost);

				/// @see Core::rayQuery(...)
				inline
				std::pair< bool, Real > rayQuery(const Ray & ray, const Real limit) { return _core->rayQuery(ray, limit); }
				
				/// @see Core::updateSurface()
				inline
				void updateSurface() { _core->updateSurface(); }

				/// @see Core::updateGrid()
				inline
				Voxel::DataAccessor::EmptySet updateGrid() { return _core->updateGrid(); }

				/// @see Core::generateConfiguration(...)
				inline
				bool generateConfiguration( const unsigned nLOD, const Touch3DFlags enStitches ) { return _core->generateConfiguration(nLOD, enStitches); }

				/// @see Core::requestConfiguration(...)
				inline
				bool requestConfiguration( const unsigned nLOD, const Touch3DFlags enStitches ) { return _core->requestConfiguration(nLOD, enStitches); }

				/// @see Core::addMetaObject(...)
				inline
				void addMetaObject( MetaObject * const mo ) { _core->addMetaObject(mo); }

				/// @see Core::removeMetaObject(...)
				inline
				bool removeMetaObject( const MetaObject * const mo ) { return _core->removeMetaObject(mo); }

				/// @see Core::clearMetaObjects()
				inline
				void clearMetaObjects() { _core->clearMetaObjects(); }

				/// @see Core::neighbor()
				MetaFragment::Container * neighbor(const Moore3DNeighbor enNeighbor) { return _core->neighbor(enNeighbor); }
			};

			/// The facet for exclusive mutable read/write access to a meta-fragment
			class _OverhangTerrainPluginExport Unique 
				:	public Unique_base,
					public UniqueLockable					
			{
			private:
				Unique (const Unique &);

			public:
				Unique (Unique && move);
				Unique (boost::shared_mutex & m, Core * pCore, Post * pPost);
			};

			/// The facet for a once-shared now exclusive mutable read/write lock on a meta-fragment
			class _OverhangTerrainPluginExport Upgraded
				:	public Unique_base,
					public UpgradedLockable
			{
			public:
				Upgraded (Upgraded && move);
				Upgraded (UpgradableLockable::LockType & lock, Core * pCore, Post * pPost);
			};

			/// The facet for a shared-lock on a meta-fragment that may be possibly upgraded to an exlcusive lock
			class _OverhangTerrainPluginExport Upgradable
				:	public const_Base,
					public UpgradableLockable
			{
			private:
				Core * const _core;
				Post * const _post;

				Upgradable (const Upgradable &);

			public:
				Upgradable (Upgradable && move);
				Upgradable (boost::shared_mutex & m, Core * pCore, Post * pPost);

				Upgraded upgrade();
			};

			/// Facet for fundamental initialization and destruction of a meta-fragment
			class _OverhangTerrainPluginExport Builder 
				:	public template_Base< Core, Post, IsoSurfaceRenderable, ISerializeCustomData * >
			{
			private:
				Builder (const Builder &);

			public:
				Builder (Builder && move);
				Builder (Core * pCore, Post * pPost);

				/// @see Core::initialise(...)
				inline
				void initialise( const Camera * pPrimaryCam, SceneNode * pSceneNode, const String & sSurfName /*= StringUtil::BLANK*/ ) { _core->initialise(pPrimaryCam, pSceneNode, sSurfName); }
				
				/// @see Core::bindCamera(...)
				inline
				void bindCamera( const Camera * pCam ) { _core->bindCamera(pCam); }

				/// @see Core::linkNeighbor(...)
				inline
				void linkNeighbor(const OrthogonalNeighbor on, MetaFragment::Container * pMWF) { _core->linkNeighbor(on, pMWF); }

				/// @see Core::getMaterial(...)
				inline
				const MaterialPtr & getMaterial () const { return _core->getMaterial(); }

				/// @see Core::setMaterial(...)
				inline
				void setMaterial (const MaterialPtr & pMat) { _core->setMaterial(pMat); }

				/// @see core::operator << (...)
				inline
				StreamSerialiser & operator << (StreamSerialiser & input) { return *_core << input; }

				/// @see Core::unlinkNeighbor(...)
				inline
				void unlinkNeighbor(const VonNeumannNeighbor vnnNeighbor) { return _core->unlinkNeighbor(vnnNeighbor); }

				/// @see Core::detachFromScene()
				inline
				void detachFromScene() { _core->detachFromScene(); }
			};
		}

		/** Main entry-point to interacting with a meta-fragment, provides thread-safe access to manipulation or 
			interrogation of a meta-fragment.
		*/
		class _OverhangTerrainPluginExport Container : private Core
		{
		public:
			/// Factory for creating various channel-specific objects
			const Voxel::MetaVoxelFactory * const factory;

			/// The owning terrain tile
			TerrainTile * const tile;

			/// Creates new MetaFragment, as well as IsoSuface and grid as needed.
			Container(RenderManager * pRendMan, const Voxel::MetaVoxelFactory * pFact, TerrainTile * pTile, Voxel::CubeDataRegion * pDG, const YLevel yl = YLevel());
			virtual ~Container();

			template< typename INTERFACE >
			INTERFACE acquire()
			{
				static_assert(false, "Method implementation not found");
			}

			template< typename INTERFACE >
			INTERFACE acquire() const
			{
				static_assert(false, "Method implementation not found");
			}

			template<>
			Interfaces::Basic acquire < Interfaces::Basic > () { return Interfaces::Basic(this, this); }

			/// Acquire basic read-only access to interrogate basic meta-fragment properties
			template<>
			Interfaces::const_Basic acquire < Interfaces::const_Basic > () const { return Interfaces::const_Basic(this, this); }

			/// Acquire construction/destruction access to setup or teardown the meta-fragment
			template<>
			Interfaces::Builder acquire < Interfaces::Builder > () { return Interfaces::Builder(this, this); }

			/// Acquire exclusive write-access to have full control over the meta-fragment after it has been initialized
			template<>
			Interfaces::Unique acquire < Interfaces::Unique > () { return Interfaces::Unique(_mutex, this, this); }

			/// Acquire shared read-only access to interrogate the meta-fragment after it has been initialized
			template<>
			Interfaces::Shared acquire < Interfaces::Shared > () const { return Interfaces::Shared(_mutex, this, this); }

			/// Acquire shared read-only access to interrogate the meta-fragment after it has been initialized that may be later upgraded to exclusive full control access over the meta-fragment
			template<>
			Interfaces::Upgradable acquire < Interfaces::Upgradable > () { return Interfaces::Upgradable(_mutex, this, this); }

			/// Acquire basic read-only access to interrogate basic meta-fragment properties
			inline 
				Interfaces::Basic acquireBasicInterface () { return Interfaces::Basic(this, this); }

			/// Acquire basic read-only access to interrogate basic meta-fragment properties
			inline 
				Interfaces::const_Basic acquireBasicInterface () const { return Interfaces::const_Basic(this, this); }

			/// Acquire construction/destruction access to setup or teardown the meta-fragment
			inline
				Interfaces::Builder acquireBuilderInterface() { return Interfaces::Builder(this, this); }

			/// Acquire exclusive write-access to have full control over the meta-fragment after it has been initialized
			inline
				Interfaces::Unique acquireInterface () { return Interfaces::Unique(_mutex, this, this); }

			/// Acquire exclusive read-only full access to the meta-fragment after it has been initialized
			inline
				Interfaces::Shared acquireInterface () const { return Interfaces::Shared(_mutex, this, this); }

			/// Acquire shared read-only access to interrogate the meta-fragment after it has been initialized that may be later upgraded to exclusive full control access over the meta-fragment
			inline
				Interfaces::Upgradable acquireUpgradableInterface () { return Interfaces::Upgradable(_mutex, this, this); }

		private:
			mutable boost::shared_mutex _mutex;

			friend void Core::linkNeighbor(const OrthogonalNeighbor, Container *);
			friend void Core::unlinkNeighbor(const VonNeumannNeighbor);
			friend void Core::initialise(const Camera *, SceneNode *, const String &);
		};
	}
}/// namespace Ogre
#endif
