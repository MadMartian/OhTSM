/*
-----------------------------------------------------------------------------
This source file is part of the OverhangTerrainSceneManager
Plugin for OGRE
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2007 Martin Enge. Based on code from DWORD, released into public domain.
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

#include "pch.h"
#include "CubeDataRegionDescriptor.h"

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
		}

		DataBase::DataBase(const size_t nCount)
		:	count(nCount),
			values( new FieldStrength[nCount]),
			dx( new signed char [nCount] ),
			dy( new signed char [nCount] ),
			dz( new signed char [nCount] ),
			red( new unsigned char[nCount]),
			green( new unsigned char[nCount]),
			blue( new unsigned char[nCount]),
			alpha( new unsigned char[nCount])
		{}

		DataBaseFactory::LeaseEx::LeaseEx( const char * szMsg ) 
		: std::exception(szMsg)
		{}

		DataBaseFactory::DataBaseFactory( const size_t nBucketElementCount, const size_t nInitialPoolCount /*= 4*/, const size_t nGrowBy /*= 1*/ ) 
		: _nGrowBy(nGrowBy), _nBucketElementCount(nBucketElementCount)
		{
			OgreAssert(nGrowBy > 0, "Grow-by must be at least one");
			growBy(nInitialPoolCount);
		}
		
		void DataBaseFactory::growBy( const size_t nAmt )
		{
			for (size_t c = 0; c < nAmt; ++c)
				_pool.push_front(new DataBase(_nBucketElementCount));
		}

		DataBase * DataBaseFactory::lease()
		{
			boost::mutex::scoped_lock lock(_mutex);

			if (_pool.empty())
				growBy(_nGrowBy);

			DataBase * pDataBase = _pool.front();
			_leased.push_front(pDataBase);
			_pool.pop_front();
			return pDataBase;
		}

		void DataBaseFactory::retire( const DataBase * pDataBase )
		{
			boost::mutex::scoped_lock lock(_mutex);

			for (DataBaseList::iterator i = _leased.begin(); i != _leased.end(); ++i)
				if (*i == pDataBase)
				{
					_pool.push_front(*i);
					_leased.erase(i);
					return;
				}

			throw LeaseEx("Cannot retire object, was not previously leased");
		}

		DataBaseFactory::~DataBaseFactory()
		{
			boost::mutex::scoped_lock lock(_mutex);

			if (!_leased.empty())
				throw LeaseEx("Cannot deconstruct factory, there are still some objects checked-out of the pool");

			for (DataBaseList::iterator i = _pool.begin(); i != _pool.end(); ++i)
				delete *i;
		}

		CubeDataRegionDescriptor::CubeDataRegionDescriptor( const DimensionType nVertexDimensions, const Real nScale, const int nFlags )
			: 
				factoryDB(nVertexDimensions*nVertexDimensions*nVertexDimensions),
				dimensions(nVertexDimensions - 1), _nDimOrder2(unsigned char (logf(nVertexDimensions)/logf(2))), 
				scale(nScale), 
				gpcount(nVertexDimensions*nVertexDimensions*nVertexDimensions), 
				sidegpcount(nVertexDimensions*nVertexDimensions),
				cellcount((nVertexDimensions - 1)*(nVertexDimensions - 1)*(nVertexDimensions - 1)), 
				sidecellcount((nVertexDimensions - 1)*(nVertexDimensions - 1)),
				_flags(nFlags),

				coordsIndexTx(computeCoordsIndexTx(nVertexDimensions)),
				cellIndexTx(computeCellIndexTx(nVertexDimensions))
		{
			OgreAssert(((dimensions - 1) & dimensions) == 0, "Dimensions must be a power of 2");
			OgreAssert(((cellcount - 1) & cellcount) == 0, "Cell count must be a power of 2");
			OgreAssert(dimensions <= 0x20, "Dimensions must be no greater than 32");
			_vertexPositions = new IsoFixVec3[gpcount];

			const IsoFixVec3 vfExtent = IsoFixVec3(signed short(1),signed short(1),signed short(1)) * signed short (dimensions) / signed short (2);

			IsoFixVec3* pVertex = _vertexPositions;
			IsoFixVec3 position = -vfExtent;

			const Vector3 vExtent = Vector3(dimensions, dimensions, dimensions) * scale / 2;
			mBoxSize.setExtents(-vExtent, vExtent);

			// Initialize grid vertices
			for (DimensionType k = 0; k <= dimensions; ++k)
			{
				for (DimensionType j = 0; j <= dimensions; ++j)
				{
					for (DimensionType i = 0; i <= dimensions; ++i)
					{
						*pVertex++ = position;

						position.x++;
					}
					position.x = -vfExtent.x;
					position.y++;
				}
				position.y = -vfExtent.y;
				position.z++;
			}
		}

		CubeDataRegionDescriptor::~CubeDataRegionDescriptor()
		{
			delete [] _vertexPositions;
		}

		Ogre::Voxel::CubeDataRegionDescriptor::IndexTx CubeDataRegionDescriptor::computeCellIndexTx( const size_t nTileSize )
		{
			IndexTx tx;
			tx.mx = 1;
			tx.my = nTileSize-1;
			tx.mz = (nTileSize-1)*(nTileSize-1);
			return tx;
		}

		Ogre::Voxel::CubeDataRegionDescriptor::IndexTx CubeDataRegionDescriptor::computeCoordsIndexTx( const size_t nTileSize )
		{
			IndexTx tx;
			tx.mx = 1;
			tx.my = nTileSize;
			tx.mz = nTileSize*nTileSize;
			return tx;
		}

	}
}
