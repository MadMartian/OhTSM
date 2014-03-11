#include "pch.h"

#include "DataBase.h"

#include "OverhangTerrainOptions.h"

namespace Ogre
{
	namespace Voxel
	{
		DataBase::~DataBase()
		{
			delete [] values;
			delete [] dx;
			delete [] dy;
			delete [] dz;
			delete [] red;
			delete [] green;
			delete [] blue;
			delete [] alpha;
			delete [] tx;
			delete [] ty;
		}

		DataBase::DataBase(const size_t nCount, const size_t nVRFlags)
			:	count(nCount),
			values( new FieldStrength[nCount]),
			dx( (nVRFlags & VRF_Gradient) != 0 ? new signed char [nCount] : NULL ),
			dy( (nVRFlags & VRF_Gradient) != 0 ? new signed char [nCount] : NULL ),
			dz( (nVRFlags & VRF_Gradient) != 0 ? new signed char [nCount] : NULL ),
			red( (nVRFlags & VRF_Colours) != 0 ? new unsigned char[nCount] : NULL ),
			green( (nVRFlags & VRF_Colours) != 0 ? new unsigned char[nCount] : NULL ),
			blue( (nVRFlags & VRF_Colours) != 0 ? new unsigned char[nCount] : NULL ),
			alpha( (nVRFlags & VRF_Colours) != 0 ? new unsigned char[nCount] : NULL ),
			tx( (nVRFlags & VRF_TexCoords) != 0 ? new unsigned char[nCount] : NULL ),
			ty( (nVRFlags & VRF_TexCoords) != 0 ? new unsigned char[nCount] : NULL )
		{}

		DataBasePool::LeaseEx::LeaseEx( const char * szMsg )
			: std::exception(szMsg)
		{}

		DataBasePool::DataBasePool( const size_t nBucketElementCount, const size_t nVRFlags, const size_t nInitialPoolCount /*= 4*/, const size_t nGrowBy /*= 1*/ )
			: _nVRFlags(nVRFlags), _nGrowBy(nGrowBy), _nBucketElementCount(nBucketElementCount)
		{
			OgreAssert(nGrowBy > 0, "Grow-by must be at least one");
			growBy(nInitialPoolCount);
		}

		void DataBasePool::growBy( const size_t nAmt )
		{
			for (size_t c = 0; c < nAmt; ++c)
				_pool.push_front(Leasing(new DataBase(_nBucketElementCount, _nVRFlags), boost::this_thread::get_id()));
		}

		DataBase * DataBasePool::lease()
		{
			boost::mutex::scoped_lock lock(_mutex);

			if (_pool.empty())
				growBy(_nGrowBy);

			Leasing leaserec = _pool.front();

			leaserec.thid = boost::this_thread::get_id();
			_leased.push_front(leaserec);
			_pool.pop_front();
			return leaserec.object;
		}

		bool DataBasePool::isLeased( const DataBase * pDataBase ) const
		{
			boost::mutex::scoped_lock lock(_mutex);

			for (DataBaseList::const_iterator i = _leased.begin(); i != _leased.end(); ++i)
				if (i->object == pDataBase)
					return true;

			return false;
		}

		void DataBasePool::retire( const DataBase * pDataBase )
		{
			boost::mutex::scoped_lock lock(_mutex);

			auto thid = boost::this_thread::get_id();

			for (DataBaseList::iterator i = _leased.begin(); i != _leased.end(); ++i)
				if (i->object == pDataBase && i->thid == thid)
				{
					_pool.push_front(*i);
					_leased.erase(i);
					return;
				}

			throw LeaseEx("Cannot retire object, was not previously leased");
		}

		DataBasePool::~DataBasePool()
		{
			boost::mutex::scoped_lock lock(_mutex);

			if (!_leased.empty())
				throw LeaseEx("Cannot deconstruct factory, there are still some objects checked-out of the pool");

			for (DataBaseList::iterator i = _pool.begin(); i != _pool.end(); ++i)
				delete i->object;
		}


		DataBasePool::Leasing::Leasing( DataBase * pObj, const boost::thread::id & thid ) 
			: object(pObj), thid(thid)
		{

		}

	}
}