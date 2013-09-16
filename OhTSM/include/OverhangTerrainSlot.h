#ifndef __OVERHANGTERRAINSLOT_H__
#define __OVERHANGTERRAINSLOT_H__

#include "OverhangTerrainPrerequisites.h"
#include "OverhangTerrainPageInitParams.h"

#include "ChannelIndex.h"

namespace Ogre
{
	// Used to define a loadable and unloadable slot of terrain data, contains multiple terrain tiles and has a one-to-one mapping with overhang terrain pages
	class _OverhangTerrainPluginExport OverhangTerrainSlot : public GeneralAllocatedObject
	{
	public:
		/// Singleton for managing pages in the overhang terrain world
		OverhangTerrainGroup * const group;

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
		@param pGrp The singleton managing all paged terrain in the overhang terrain world
		@param x X-component of the 2D index of this slot in the group
		@param y Y-component of the 2D index of this slot in the group
		*/
		OverhangTerrainSlot (OverhangTerrainGroup * pGrp, const int16 x, const int16 y);

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

		~OverhangTerrainSlot();
	};

}

#endif