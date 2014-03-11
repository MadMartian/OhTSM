#ifndef __OVERHANGTERRAINVOXELDATABASE_H__
#define __OVERHANGTERRAINVOXELDATABASE_H__

#include "OverhangTerrainPrerequisites.h"

#include "IsoSurfaceSharedTypes.h"

namespace Ogre
{
	namespace Voxel
	{
		/** Data container for a cubical region of voxels including voxel values, gradient, and colours */
		class _OverhangTerrainPluginExport DataBase
		{
		private:
			DataBase (const DataBase &);

		public:
			const size_t count;
			FieldStrength * values;
			signed char * dx, *dy, *dz;
			unsigned char * red, * green, * blue, * alpha;
			unsigned char * tx, *ty;

			DataBase(const size_t nCount, const size_t nVRFlags);
			virtual ~DataBase();
		};

		/** A memory pool pattern for DataBase instances to eliminate allocation/deallocation
			and improve performance */
		class _OverhangTerrainPluginExport DataBasePool
		{
		private:
			struct Leasing
			{
				DataBase * object;
				boost::thread::id thid;

				Leasing(DataBase * pObj, const boost::thread::id & thid );
			};

			typedef std::list< Leasing > DataBaseList;

			mutable boost::mutex _mutex;

			/// The voxel region flags used to create voxel regions in this factory
			const size_t _nVRFlags;
			/// How much to grow the pool each iteration and the voxel count per DataBase instance
			const size_t _nGrowBy, _nBucketElementCount;
			/// Unused and checked-out pools
			DataBaseList _pool, _leased;

			/// Expands the pool by an amount
			void growBy(const size_t nAmt);

			// Copying a factory is nonsensical
			DataBasePool(const DataBasePool &);

		public:
			/** Exception class used to enforce consistency, thrown from destructor when objects are still
				checked-out of the pool and thrown during retire if the specified instance was not previously
				checked-out. */
			class LeaseEx : public std::exception
			{
			public:
				LeaseEx(const char * szMsg);
			};

			/**
			@param nBucketElementCount voxel count per DataBase instance
			@param nVRFlags The voxel region flags used to create voxel regions in this factory
			@param nInitialPoolCount The initial number of database objects to allocate immediately available for checkout
			*/
			DataBasePool(const size_t nBucketElementCount, const size_t nVRFlags, const size_t nInitialPoolCount = 4, const size_t nGrowBy = 1);

			/// Check-out an instance
			DataBase * lease ();
			/// Check-in an instance
			void retire (const DataBase * pDataBase);
			/// Check if an object is already leased
			bool isLeased (const DataBase * pDataBase) const;

			~DataBasePool();
		};
	}
}

#endif