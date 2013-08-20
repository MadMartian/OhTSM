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
#pragma once

#include "OverhangTerrainPrerequisites.h"
#include "OverhangTerrainSceneManager.h"
#include "OverhangTerrainPageProvider.h"
#include "OverhangTerrainPageInitParams.h"
#include "OverhangTerrainPagedWorldSection.h"

#include "MetaFactory.h"
#include "ChannelIndex.h"

#include <OgreWorkQueue.h>
#include <OgreStreamSerialiser.h>
#include <OgrePagingPrerequisites.h>

#include <map>

namespace Ogre
{
	/* A grouping of overhang terrain pages for handling terrain paging with overhang terrain scene manager, compatible with the Ogre paging system and parallels the classic terrain paging system */
	class _OverhangTerrainPluginExport OverhangTerrainGroup : public WorkQueue::RequestHandler, public WorkQueue::ResponseHandler, public GeneralAllocatedObject, public OverhangTerrainManager
	{
	public:
		// Used to define a loadable and unloadable slot of terrain data, contains multiple terrain tiles and has a one-to-one mapping with overhang terrain pages
		class _OverhangTerrainPluginExport TerrainSlot : public GeneralAllocatedObject
		{
		public:
			/* Used to minimize the use of locking, terrain slot states signify what operations are currently executing on its dependencies.
			Each enum constant indicates the permitted states that the state machine may transition from
			*/
			enum State
			{
				/** Nothing is happening, slot is available to lease for any operation
				Transition to one of: TSS_Mutate, TSS_Saving, TSS_SaveUnload, TSS_NeighborQuery */
				TSS_Neutral = ~0,
				/** The slot is empty, no terrain page is loaded in this slot currently
				Transition to one of: TSS_Loading */
				TSS_Empty = 0,
				/** The slot has at least one adjacent slot performing a neighborhood query that depends on this slot
				Transition to one of: TSS_NeighborQuery, TSS_Neutral */
				TSS_NeighborQuery = 1,
				/** A background thread is currently unloading a terrain page
				Transition to one of: TSS_Empty */
				TSS_Unloading = 2,
				/** A background thread is currently loading a terrain page
				Transition to one of: TSS_Neutral (TODO: TSS_Unloading) */
				TSS_Loading = 3,
				/** A background thread is currently saving a terrain page
				Transition to one of: TSS_Neutral, TSS_SaveUnload */
				TSS_Saving = 4,
				/** A background thread is currently being exposed to gamma radiation :p
				Transition to one of: TSS_Neutral */
				TSS_Mutate = 5,
				/** A background thread is currently querying the slot
				Transition to one of: TSS_Neutral */
				TSS_Query = 6,
				/** The slot is reserved for destruction in preparation for removing all terrain from the group once all background tasks have completed */
				TSS_Destroy = 7
			};

		private:
			/// For the TSS_NeighborQuery state, tracks which neighbors are enforcing the neighbor query state on this slot
			size_t queryNeighbors;
			/// Query stack count
			size_t queryCount;
			/// The current state
			State _enState, 
			/// The previous state
				_enState0;
			
			/// Represents a task that will be run for a terrain slot immediately once it returns to the TSS_Neutral state from a non-neutral background task state
			struct JoinTask
			{
				enum TaskType
				{
					/// The terrain slot and page data will be removed from the scene and destroyed
					JTT_Destroy,
					/// The material for the page and its renderables will be set
					JTT_SetMaterial,
					/// The render queue group for the page and its renderables will be set
					JTT_SetQID
				} 
				/// The type of task to perform
				type;
				
				/// Applies to all channel-related task types
				Channel::Ident channel;

				/// Applies to the JTT_SetMaterial task type
				MaterialPtr material;

				/// Applies to the JTT_SetQID task type
				int qid;
			};
			typedef std::queue< JoinTask > JoinTaskQueue;

			/// A queue of tasks that will be executed after background task on this slot finishes
			JoinTaskQueue _qJoinTasks;

			/// Completes all pending tasks
			void processPendingTasks ();

		public:
			/// A state exception for attempting to transition to invalid states
			class _OverhangTerrainPluginExport StateEx : public std::exception 
			{
			public:
				StateEx( const char * szMsg );
			};

			/// 2D index of the terrain slot in the group
			int16 x, y;
			/// A pointer to the terrain page, can be NULL
			PageSection * instance;
			/// @see PageSection::getPosition()
			Vector3 position;

			// Structure used for serializing terrain state
			struct LoadData
			{
				PageInitParams params;

				LoadData (const OverhangTerrainOptions & options, const int16 pageX, const int16 pageY) : params(options, pageX, pageY) {}
			} * data;

			/** 
			@param x X-component of the 2D index of this slot in the group
			@param y Y-component of the 2D index of this slot in the group
			*/
			TerrainSlot (const int16 x, const int16 y);

			/// @returns The current state
			State state() const { return _enState; }
			/// @returns True if the slot in its current state may transition to TSS_Mutate
			bool canMutate () const { return _enState == TSS_Neutral; }
			/// @returns True if the slot in its current state may transition to TSS_Destroy
			bool canDestroy () const { return _enState == TSS_Neutral; }
			/// @returns True if the slot in its current state may transition to TSS_Unloading
			bool canUnload () const { return _enState == TSS_Neutral; }
			/// @returns True if the slot in its current state may transition to TSS_Saving or TSS_SaveUnload
			bool canSave () const { return _enState == TSS_Neutral || _enState == TSS_Query; }
			/// @returns True if the slot in its current state allows interrogation and the terrain page isn't undergoing some kind of alteration / mutation in a background thread
			bool canRead () const { return _enState == TSS_Neutral || _enState == TSS_Saving || _enState == TSS_Query;}
			/// @returns True if the specified neighbor slot has this slot under TSS_NeighborQuery state
			bool isNeighborQueried (const VonNeumannNeighbor evnNeighbor) const;
			/// @returns True if this slot in its current state can neighbor query the specified neighbor slot
			bool canNeighborQuery (const VonNeumannNeighbor evnNeighbor) const;
			/// Transitions to the TSS_NeighborQuery state for the specified neighbor
			void setNeighborQuery (const VonNeumannNeighbor evnNeighbor);
			/// Clears the neighbor query state on this slot from the specified neighbor transitioning back to the previous state before all TSS_NeighborQuery states if there are none left on this slot
			void clearNeighborQuery (const VonNeumannNeighbor evnNeighbor);

			/// Transitions to the TSS_Saving state, throws StateEx if the current slot state forbids the transition
			void saving ();
			/// Transitions from the TSS_Saving state back to the previous state before TSS_Saving, throws StateEx if the current slot state forbids the transition
			void doneSaving();
			
			/// Transitions to the TSS_SaveUnload state, throws StateEx if the current slot state forbids the transition
			void saveUnload();

			/// Transitions to the TSS_Mutate state, throws StateEx if the current slot state forbids the transition
			void mutating ();
			/// Transitions from the TSS_Mutate state back to TSS_Neutral, throws StateEx if the current slot state forbids the transition
			void doneMutating();

			/// Transitions to the TSS_Query state, throws StateEx if the current slot state forbids the transition
			void query ();
			/// Transitions from the TSS_Query state back to TSS_Neutral, throws StateEx if the current slot state forbids the transition
			void doneQuery();

			/// Transitions to the TSS_Loading state, throws StateEx if the current slot state forbids the transition
			void loading ();
			/// Transitions from the TSS_Loading state back to TSS_Neutral, throws StateEx if the current slot state forbids the transition
			void doneLoading();

			/// Transitions to the TSS_Unloading state, throws StateEx if the current slot state forbids the transition
			void unloading ();
			/// Transitions from the TSS_Unloading state to TSS_Empty, throws StateEx if the current slot state forbids the transition
			void doneUnloading();

			/// Transitions from the TSS_Neutral state to the TSS_Destroy state, throws StateEx if the current slot state forbids the transition
			void destroy();

			/// Frees-up and nullifies a structure designed to hold data required for loading this slot's terrain page
			void freeLoadData ();

			/// Sets the material of the page-wide channel or queues a request to do so if the slot is busy
			void setMaterial (const Channel::Ident channel, MaterialPtr pMaterial);

			/// Sets the render queue group of the page-wide channel or queues a request to do so if the slot is busy
			void setRenderQueueGroup (const Channel::Ident channel, const int nQID);
			
			/// Deletes the page and marks the slot as TSS_Destroy or queues a request to do so if the slot is busy
			void destroySlot ();

			~TerrainSlot();
		};

		typedef std::map< uint32, TerrainSlot * > TerrainSlotMap;

		/**
		@param sm Pointer to the scene manager used to obtain the main top-level configuration options (among other things)
		@param pManRsrcLoader Pointer to a manual resource loader used to construct the meta-factory
		@param sResourceGroup Resource group name for read/write terrain page storage
		*/
		OverhangTerrainGroup( 
			OverhangTerrainSceneManager * sm, 
			ManualResourceLoader * pManRsrcLoader = NULL, 
			const String & sResourceGroup = ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME
		);
		~OverhangTerrainGroup();

		/// Writes OhTSM configuration options and basic properties of this group to the stream
		void saveGroupDefinition(StreamSerialiser& stream);
		/// Reads OhTSM configuration options and basic properties of this group from the stream
		void loadGroupDefinition(StreamSerialiser& stream);

		/// Set a custom page provider that will receive events from the library
		inline void setPageProvider (IOverhangTerrainPageProvider * pp)
			{ _pPageProvider = pp; }

		/// Center the terrain group at the specified position
		void setOrigin(const Vector3 & pos);

		/// Origin in the world for this group of terrain pages
		inline const Vector3 & getOrigin() const { return _ptOrigin; }

		/// Sets the material used on all renderables of the specified channel
		void setMaterial (const Channel::Ident channel, const MaterialPtr & m);
		/// Retrieves the material used on all renderables of the specified channel
		inline const MaterialPtr & getMaterial(const Channel::Ident channel) const { return _chanprops[channel].material; }
		/// Sets the render queue group used on all renderables of the specified channel
		void setRenderQueueGroup(const Channel::Ident channel, const int nQID);
		/// Retrieves the render queue group used for all renderables of the specified channel
		inline const int getRenderQueueGroup(const Channel::Ident channel) const { return _chanprops[channel].qid; }

		/** Concurrently add a metaball to the scene.
		@remarks A background request is initiated for adding the metaball to the scene and updating the respective voxel grids.
		@param position The absolute world position of the metaball
		@param radius The radius of the metaball sphere in world units
		@param excavating Whether or not the metaball carves out empty space or fills it in with solid
		@param synchronous Whether or not to execute the operation in this thread and not leverage threading capability */
		void addMetaBall(const Vector3 & position, const Real radius, const bool excavating = true, const bool synchronous = false);

		/** Retrieves the terrain slot at the specified 2D index
		@remarks Retrieves the terrain slot at the coordinates specified optionally creating one if specified
		@param x The x-coordinate of the slot
		@param y The y-cooridnate of the slot
		@param createIfMissing Whether or not to create a terrain slot at the specified coordinates
		@returns The terrain slot at the specified coordinates or NULL if we're not creating one if its empty */
		TerrainSlot* getTerrainSlot(const int16 x, const int16 y, bool createIfMissing);

		/** Retrieves the terrain slot at the specified 2D index
		@param x The x-coordinate of the slot
		@param y The y-cooridnate of the slot
		@returns The terrain slot at the specified coordinates or NULL if there isn't one yet */
		TerrainSlot* getTerrainSlot(const int16 x, const int16 y) const;

		/** Test for intersection of a given ray with any terrain in the group. If the ray hits a terrain, the point of 
			intersection and terrain instance is returned.
		 @param ray The ray to test for intersection
		 @param params Parameters influencing the ray query including the distance from the ray origin at which we will stop looking or zero for no limit
		 @return A result structure which contains whether the ray hit a terrain and if so, where.
		 @remarks This must be called from the main thread */
		RayResult rayIntersects(Ray ray, const OverhangTerrainManager::RayQueryParams & params) const;

		/** Queues a background request for unloading terrain at the specified coordinates
		@remarks Attempts to unload the terrain page at the specified slot if it is permitted to.
			Nothing happens if the terrain slot is currently busy.
			The operation is performed in the background or synchronously if requested.
			If the page is dirty and the autosave option is enabled the page is first saved.
		@param x The x-component of the slot index
		@param y The y-component of the slot index
		@param synchronous Whether or not to perform the unload operation in the main thread instead of leveraging a background thread
		@returns True if the terrain was queued for unloading, false otherwise (as in the state of the slot forbade it) */
		bool unloadTerrain(const int16 x, const int16 y, const bool synchronous = false);

		/** Queues a background request for unloading terrain at the specified coordinates
		@remarks Attempts to unload the terrain page at the specified slot if it is permitted to.
			The operation is performed in the background or synchronously if requested.
		@param pSlot The terrain slot to unload the terrain from
		@param synchronous Whether or not to perform the unload operation in the main thread instead of leveraging a background thread */
		void unloadTerrainImpl( TerrainSlot * pSlot, const bool synchronous = false );

		/** Ensures the terrain slot is defined (allocated) and optionally queues a background request for loading terrain at the specified coordinates either 
			from disk or from a custom page provider.
		@remarks Attempts to load the terrain page at the specified slot if it is permitted to and if the load flag is set otherwise it just defines the terrain slot.
			Nothing happens if the terrain slot is currently busy or the neighborhood cannot be locked.
			The operation is performed in the background or synchronously if requested.
			This method is called many times in succession due to the nature of the 2D loading strategy
		@param x The x-component of the slot index
		@param y The y-component of the slot index
		@param bLoad Whether to load the terrain page as well or just define it and leave it at that
		@param synchronous Whether or not to perform the load operation in the main thread instead of leveraging a background thread
		@returns True if the terrain was queued for loading, false otherwise (as in the state of the slot forbade it or the neighborhood could not be locked) */
		bool defineTerrain(const int16 x, const int16 y, const bool bLoad = true, bool synchronous = false);

		/** Queues a request to remove all terrain slots and terrain pages from the group 
		@remarks Creates a background task that marks terrain slots for destruction and polls the work-queue until all terrain slots are successfully marked for destruction,
			then it destroys all terrain slots and pages clearing the scene. */
		void clear();

		virtual bool canHandleRequest( const WorkQueue::Request* req, const WorkQueue* srcQ );
		virtual WorkQueue::Response* handleRequest( const WorkQueue::Request* req, const WorkQueue* srcQ );

		virtual bool canHandleResponse( const WorkQueue::Response* res, const WorkQueue* srcQ );
		virtual void handleResponse( const WorkQueue::Response* res, const WorkQueue* srcQ );

		inline String getResourceGroupName() const { return _sResourceGroup; }

	protected:
		/// Request type indicators for background tasks
		enum RequestType
		{
			/// Load a page slot task
			LoadPage = 1,
			/// Add a metaobject task
			AddMetaObject = 2,
			/// Unload a page slot task
			UnloadPage = 3,
			/// Save a page to disk task
			SavePage = 4,
			/// Destroy and clear the scene of all terrains task
			DestroyAll = 5
		};

		/// The main factory singleton for creating top-level objects
		MetaBaseFactory _factory;

		/// Describes all channels used in the group
		Channel::Descriptor _descchan;

		/// Tracks certain channel-specific properties
		struct ChannelProperties
		{
			MaterialPtr material;
			int qid;
		};
		/// Channel-specific properties
		Channel::Index< ChannelProperties > _chanprops;

		/// Factory method for creating PageSection instances
		PageSection * createPage ();

		/** Fills the array with the neighborhood of the specified slot
		@param vpSlots The array to fill with neighbors of the specified slot, preallocated to support 4 pointers to TerrainSlot
		@param pSlot The slot from which to obtain the neighborhood */
		void fillNeighbors (TerrainSlot ** vpSlots, const TerrainSlot * pSlot);

		/** Converts the specified coordinates to slot coordinates identifying a terrain slot
		@param pt The coordinates to convert
		@param x The x-component of the coordinates of the terrain slot that contains the specified point
		@param y The y-component of the coordinates of the terrain slot that contains the specified point
		@param encsFrom The coordinate system of the specified point */
		void toSlotPosition( const Vector3 & pt, int16 & x, int16 & y, const OverhangCoordinateSpace encsFrom = OCS_World) const;

		/** Computes the appropriate world coordinates for the specified 2D slot index
		@param x The x-component of the coordinates of the terrain slot
		@param y The y-component of the coordinates of the terrain slot
		@param fOffset A percentage of the page size to shift the coordinates by
		@returns World-system coordinates useful for a page located at the 2D Slot index */
		Vector3 computeTerrainSlotPosition (const int16 x, const int16 y) const;

		/** Converts the 2D terrain slot index into a scalar unique identifier
		@returns A page ID uniquely based on the slot at the specified 2D coordinates */
		virtual PageID calculatePageID(const int16 x, const int16 y) const;

		/// Base struct for all request data structures
		struct WorkRequestBase
		{
			/// Specifies the origin of the request (this)
			OverhangTerrainGroup * origin;

			_OverhangTerrainPluginExport friend std::ostream & operator << (std::ostream & o, const WorkRequestBase & r)
			{ return o; }
		};

		/// Most work requests involve a specific terrain slot, this class is that purpsoe
		struct WorkRequest : public WorkRequestBase
		{
			/// Object of the request
			TerrainSlot * slot;

			_OverhangTerrainPluginExport friend std::ostream & operator << (std::ostream & o, const WorkRequest & r)
			{ return o; }
		};

		/// Special request type for adding metaballs to the scene
		struct MetaBallWorkRequest : public WorkRequestBase
		{
			/// The position of the metaball in world coordinates
			struct  
			{
				Real x, y, z;
			} position;
			/// The radius of the metaball's sphere in world units
			Real radius;
			/// The excavation flag for the metaball
			bool excavating;

			typedef std::vector< TerrainSlot * > SlotList;
			/// The terrain slots that are affected and must be updated by this metaball representation
			SlotList slots;

			_OverhangTerrainPluginExport friend std::ostream & operator << (std::ostream & o, const MetaBallWorkRequest & r)
			{ return o; }
		};

		/// Request type data for unloading terrain pages
		struct UnloadPageResponseData
		{
			/// Specifies the list of renderable isosurfaces from the page that is to be disposed of and that must be disposed of at the end of the request
			std::vector< Renderable * > * dispose;

			_OverhangTerrainPluginExport friend std::ostream & operator << (std::ostream & o, const UnloadPageResponseData & r)
			{ return o; }
		};

		/** Queues a request for saving the terrain slot
		@remarks Attempts to queue a background request for saving the specified terrain slot.
			The method will do nothing if the slot is busy or otherwise forbidden from saving.
		@param pSlot The terrain slot to save
		@param synchronous Whether to execute the task synchronously in the main thread or concurrently in a background thread
		@returns True if the terrain was queued for saving, false otherwise (as in the state of the slot forbade it) */		
		bool saveTerrain (TerrainSlot * pSlot, const bool synchronous = false);

		/** Queues a save request regardless of the terrain slot state
		@param pSlot The terrain slot to save
		@param synchronous Whether to execute the task synchronously in the main thread or concurrently in a background thread */
		void saveTerrainImpl( TerrainSlot * pSlot, const bool synchronous = false );

		/// The worker method for saving a particular terrain slot
		void saveTerrain_worker(TerrainSlot * pSlot );
		/// The final response steps of saving a particular terrain slot
		void saveTerrain_response( TerrainSlot * slot );

		/** Initialize a page and bind it to the scene for rendering
		@param page The page that should be initialized and bound to the scene
		@param data Data necessary for completing page initialization */
		void preparePage(PageSection * const page, const TerrainSlot::LoadData * data);

		/** Links-up neighbors for the specified page with terrain pages from the Von Neumann neighborhood terrain slots
		@param x The x-component of the slot index
		@param y The y-component of the slot index
		@param pPage The terrain page to link-up to neighbors */
		void linkPageNeighbors (const int16 x, const int16 y, PageSection * const pPage);

		/** The background work algorithm for adding a metaball to the scene, does all the heavy-lifting
		@param iBegin Iterator pattern for beginning of the list of terrain slots affected by the metaball
		@param iEnd Iterator pattern for the end of the list of terrain slots affected by the metaball
		@param postion Absolute position in world coordinates of the metaball
		@param radius Radius of the metaball sphere in world units
		@param excavating The excavation flag of the metaball */
		void addMetaBall_worker (MetaBallWorkRequest::SlotList::iterator iBegin, MetaBallWorkRequest::SlotList::iterator iEnd, const Vector3 & position, const Real radius, const bool excavating = true);
		/** The final steps of adding a metaball to the scene.
		@param iBegin Iterator pattern for beginning of the list of terrain slots affected by the metaball
		@param iEnd Iterator pattern for the end of the list of terrain slots affected by the metaball */
		void addMetaBall_response(MetaBallWorkRequest::SlotList::iterator iBegin, MetaBallWorkRequest::SlotList::iterator iEnd);

		/** Attempts to set the Von Neumann neighborhood of terrain slots to the TSS_NeighborQuery state for this slot
		@remarks Sets the Von Neumann neighborhood of terrain slots to the TSS_NeighborQuery state for this slot unless at least one of them forbids the
			state change, in which case no change is made to any slots.
		@param slot The terrain slot whose neighborhood will undergo possible state change
		@returns True if the neighborhood of terrain-tiles was transitioned to TSS_NeighborQuery successfully, false if nothing was changed */
		bool tryLockNeighborhood( const TerrainSlot * slot );
		/** Reverses the state change to TSS_NeighborQuery previously accomplished by a call to lock the neighborhood
		@param slot The terrain slot whose neighborhood shall undergo state reversion */
		void unlockNeighborhood( const TerrainSlot* slot );

		/// Worker tasks for defining / loading a terrain slot
		bool defineTerrain_worker( TerrainSlot * slot );
		/// Final steps in defining / loading a terrain slot
		void defineTerrain_response( TerrainSlot* slot );

		/// Worker tasks for unloading a terrain slot
		void unloadTerrain_worker( TerrainSlot * slot, UnloadPageResponseData & respdata );
		/// Final steps in unloading a terrain slot
		void unloadTerrain_response( TerrainSlot * pSlot, UnloadPageResponseData &data );

		/// Final steps in clearing the scene of all terrain tiles, this does the heavy lifting of deleting all terrain objects if the scene is ready to do so, otherwise it polls by making a call to 'clear()'
		void clear_response();

		/** Creates or opens a file (of predetermined filename) from the configured resource group for the terrain slot at the specified location
		@remarks This method is typically used for loading/saving terrain pages individually
		@param x The x-component of the slot index
		@param y The y-component of the slot index
		@param bReadOnly Whether to open the file in read-only mode 
		@returns A stream object capable of reading/writing objects to/from it from/to disk */
		DataStreamPtr acquirePageStream( const int16 x, const int16 z, bool bReadOnly = true );

	private:
		/// The Overhang Terrain Scene Manager
		OverhangTerrainSceneManager * _pScMgr;
		/// A unique ID for the work queue channel all requests initiated by this class are inserted into
		uint16 _nWorkQChannel;
		/// Center position of the terrain group and its terrains relative to
		Vector3 _ptOrigin;
		/// Name of the resource group where terrain pages are stored
		String _sResourceGroup;
		/// Pointer to an OGRE object used for paging
		const OverhangTerrainPagedWorldSection * _pPagedWorld;
		/// A custom terrain page provider for loading custom terrain pages
		IOverhangTerrainPageProvider * _pPageProvider;
		/// Small biases for querying pages
		int16 _pxi, _pyi;

		/// The terrain slots container
		TerrainSlotMap _slots;

		/// Used for serialization
		static const uint32 CHUNK_ID;
		static const uint16 CHUNK_VERSION;
		static const uint32 CHUNK_PAGE_ID;
		static const uint16 CHUNK_PAGE_VERSION;

		/// Sets the terrain paged world section object pointer
		inline void setOverhangTerrainPagedWorldSection (const OverhangTerrainPagedWorldSection * pws)
		{
			_pPagedWorld = pws;
		}
		friend class OverhangTerrainPagedWorldSection;
	};

}