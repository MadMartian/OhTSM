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

#include "OgreColourValue.h"
#include "CubeDataRegion.h"
#include "Util.h"
#include "DebugTools.h"
#include "Neighbor.h"

namespace Ogre
{
	namespace Voxel
	{
		CubeDataRegion::CubeDataRegion( const CubeDataRegionDescriptor & dgtmpl, const Vector3 & pos /*= Vector3::ZERO*/)
		  : _meta(dgtmpl), _factory(dgtmpl.factoryDB),
			_pos(pos), _bbox(createBoundingBox(dgtmpl, pos)),
			_compression(new CompressedDataBase(_meta.hasGradient(), _meta.hasColours()))
		{
		}

		CubeDataRegion::~CubeDataRegion()
		{
			OHT_DBGTRACE("Delete " << this);
			delete _compression;
		}

		bool CubeDataRegion::mapRegion( const AxisAlignedBox& aabb, WorldCellCoords & gp0, WorldCellCoords & gpN ) const
		{
			Vector3
				v0 = _bbox.getMinimum() - _meta.scale,
				vN = _bbox.getMaximum() + _meta.scale;

			// x0
			if (aabb.getMinimum().x <= v0.x)
				gp0.i = -1;
			else
			if (aabb.getMinimum().x > vN.x)
				return false;
			else
				gp0.i = signed int(Math::Ceil((aabb.getMinimum().x - _bbox.getMinimum().x) / _meta.scale));

			// gp0.j
			if (aabb.getMinimum().y <= v0.y)
				gp0.j = -1;
			else
			if (aabb.getMinimum().y > vN.y)
				return false;
			else
				gp0.j = signed int(Math::Ceil((aabb.getMinimum().y - _bbox.getMinimum().y) / _meta.scale));

			// gp0.k
			if (aabb.getMinimum().z <= v0.z)
				gp0.k = -1;
			else
			if (aabb.getMinimum().z > vN.z)
				return false;
			else
				gp0.k = signed int(Math::Ceil((aabb.getMinimum().z - _bbox.getMinimum().z) / _meta.scale));

			// gpN.i
			if (aabb.getMaximum().x < v0.x)
				return false;
			else
			if (aabb.getMaximum().x >= vN.x)
				gpN.i = _meta.dimensions+1;
			else
				gpN.i = signed int(Math::Floor((aabb.getMaximum().x - _bbox.getMinimum().x) / _meta.scale));

			// gpN.j
			if (aabb.getMaximum().y < v0.y)
				return false;
			else
			if (aabb.getMaximum().y >= vN.y)
				gpN.j = _meta.dimensions+1;
			else
				gpN.j = signed int(Math::Floor((aabb.getMaximum().y - _bbox.getMinimum().y) / _meta.scale));

			// gpN.k
			if (aabb.getMaximum().z < v0.z)
				return false;
			else
			if (aabb.getMaximum().z >= vN.z)
				gpN.k = _meta.dimensions+1;
			else
				gpN.k = signed int(Math::Floor((aabb.getMaximum().z - _bbox.getMinimum().z) / _meta.scale));

			return true;
		}

		StreamSerialiser & CubeDataRegion::operator >> (StreamSerialiser & output) const
		{
			const_CompressedDataAccessor data = clease();

			output.write(&_pos);
			output.write(&_bbox);
			data >> output;

			return output;
		}
		StreamSerialiser & CubeDataRegion::operator << (StreamSerialiser & input)
		{
			CompressedDataAccessor data = clease();

			input.read(&_pos);
			input.read(&_bbox);
			data << input;

			return input;
		}

		DataAccessor CubeDataRegion::lease()
		{
			DataBase * pDataBucket = _factory.lease();
			populate(pDataBucket);
			return DataAccessor (_mutex, pDataBucket, this, _meta);
		}

		const_DataAccessor CubeDataRegion::lease() const
		{
			DataBase * pDataBucket = _factory.lease();
			populate(pDataBucket);
			return const_DataAccessor (_mutex, pDataBucket, this, _meta);
		}

		DataAccessor * CubeDataRegion::lease_p()
		{
			DataBase * pDataBucket = _factory.lease();
			populate(pDataBucket);
			return new DataAccessor (_mutex, pDataBucket, this, _meta);
		}

		const_DataAccessor * CubeDataRegion::lease_p() const
		{
			DataBase * pDataBucket = _factory.lease();
			populate(pDataBucket);
			return new const_DataAccessor (_mutex, pDataBucket, this, _meta);
		}

		CompressedDataAccessor CubeDataRegion::clease()
		{
			return CompressedDataAccessor(_mutex, _compression);
		}

		const_CompressedDataAccessor CubeDataRegion::clease() const
		{
			return const_CompressedDataAccessor(_mutex, _compression);
		}

		void CubeDataRegion::released( DataBase * pDataBucket )
		{
			*_compression << *pDataBucket;
			released(const_cast< const DataBase * > (pDataBucket));
		}
		void CubeDataRegion::released( const DataBase * pDataBucket ) const
		{
			_factory.retire(pDataBucket);
		}

		void CubeDataRegion::populate( DataBase * pDataBucket ) const
		{
			*_compression >> *pDataBucket;
		}

		const_DataAccessor::const_DataAccessor( 
			boost::recursive_mutex & mutex, 
			const DataBase * pBucket, 
			const IDataBaseHook * pDataBaseHook,
			const CubeDataRegionDescriptor & dgtmpl 
		)
			: template_DataAccessor(mutex, pBucket, pDataBaseHook, dgtmpl)
		{}

		const_DataAccessor::const_DataAccessor( const const_DataAccessor & copy) 
			: template_DataAccessor(copy) 
		{}

		const_DataAccessor::const_DataAccessor( const_DataAccessor && move ) 
			: template_DataAccessor(static_cast< template_DataAccessor && > (move)) 
		{}

		DataAccessor::EmptySet DataAccessor::getEmptyStatus()
		{
			FieldStrength acc = 0;

			for (size_t i = 1; i < count; ++i)
				acc |= values[i-1] ^ values[i];

			if ((acc & ~Voxel::FS_Mantissa) == 0)
			{
				if (values[0] < 0)
					return Empty_Solid;
				else
					return Empty_Clear;
			} else
				return Empty_None;
		}

		void DataAccessor::updateGradient()
		{
			const int 
				zN = _dgtmpl.dimensions,
				yN = zN,
				xN = yN;

			for (int z = 0; z <= zN; ++z)
				for (int x = 0; x <= xN; ++x)
					for (int y = 0; y <= yN; ++y)
						gradients.dx[_dgtmpl.getGridPointIndex(x, y, z)] = 
							(Voxel::GradientField::PublicPrimitive)voxels(x + 1, y, z) 
							- 
							(Voxel::GradientField::PublicPrimitive)voxels(x - 1, y, z);

			for (int z = 0; z <= zN; ++z)
				for (int x = 0; x <= xN; ++x)
					for (int y = 0; y <= yN; ++y)
						gradients.dy[_dgtmpl.getGridPointIndex(x, y, z)] = 
							(Voxel::GradientField::PublicPrimitive)voxels(x, y + 1, z) 
							- 
							(Voxel::GradientField::PublicPrimitive)voxels(x, y - 1, z);

			for (int z = 0; z <= zN; ++z)
				for (int x = 0; x <= xN; ++x)
					for (int y = 0; y <= yN; ++y)
						gradients.dz[_dgtmpl.getGridPointIndex(x, y, z)] = 
							(Voxel::GradientField::PublicPrimitive)voxels(x, y, z + 1) 
							- 
							(Voxel::GradientField::PublicPrimitive)voxels(x, y, z - 1);
		}
	
		void DataAccessor::reset()
		{
			voxels.clear();
			gradients.clear();
		}

		void DataAccessor::clear()
		{
			reset();
			colours.clear();
		}

		DataAccessor::DataAccessor(
			boost::recursive_mutex & mutex, 
			DataBase * pBucket, 
			IDataBaseHook * pDataBaseHook,
			const CubeDataRegionDescriptor & dgtmpl 
		)
			: template_DataAccessor(mutex, pBucket, pDataBaseHook, dgtmpl)
		{}

		DataAccessor::DataAccessor( const DataAccessor & copy ) 
		: template_DataAccessor(copy) 
		{}

		DataAccessor::DataAccessor( DataAccessor && move) 
		: template_DataAccessor(static_cast< template_DataAccessor && > (move)) 
		{}

		CompressedDataBase::CompressedDataBase( const bool bGradField /*= true*/, const bool bColorSet /*= true*/ ) 
		: gradfield(bGradField ? new GradientChannels : NULL), colors(bColorSet ? new ColorChannels : NULL)
		{

		}

		CompressedDataBase::~CompressedDataBase()
		{
			delete gradfield;
			delete colors;
		}

		void CompressedDataBase::operator<<( const DataBase & database )
		{
			values.compress(database.count, reinterpret_cast< unsigned char * > (database.values));

			if (gradfield != NULL)
			{
				gradfield->dx.compress(database.count, reinterpret_cast< const unsigned char * > (database.dx));
				gradfield->dy.compress(database.count, reinterpret_cast< const unsigned char * > (database.dy));
				gradfield->dz.compress(database.count, reinterpret_cast< const unsigned char * > (database.dz));
			}

			if (colors != NULL)
			{
				colors->r.compress(database.count, reinterpret_cast< unsigned char * > (database.red));
				colors->g.compress(database.count, reinterpret_cast< unsigned char * > (database.green));
				colors->b.compress(database.count, reinterpret_cast< unsigned char * > (database.blue));
				colors->a.compress(database.count, reinterpret_cast< unsigned char * > (database.alpha));
			}
		}

		void CompressedDataBase::operator>>( DataBase & database ) const
		{
			values.decompress(database.count, reinterpret_cast< unsigned char * > (database.values));

			if (gradfield != NULL)
			{
				gradfield->dx.decompress(database.count, reinterpret_cast< unsigned char * > (database.dx));
				gradfield->dy.decompress(database.count, reinterpret_cast< unsigned char * > (database.dy));
				gradfield->dz.decompress(database.count, reinterpret_cast< unsigned char * > (database.dz));
			}

			if (colors != NULL)
			{
				colors->r.decompress(database.count, reinterpret_cast< unsigned char * > (database.red));
				colors->g.decompress(database.count, reinterpret_cast< unsigned char * > (database.green));
				colors->b.decompress(database.count, reinterpret_cast< unsigned char * > (database.blue));
				colors->a.decompress(database.count, reinterpret_cast< unsigned char * > (database.alpha));
			}
		}


		const_CompressedDataAccessor::const_CompressedDataAccessor( boost::recursive_mutex & m, const CompressedDataBase * compression ) 
		: template_CompressedDataAccessor(m, compression)
		{

		}

		StreamSerialiser & const_CompressedDataAccessor::operator >> ( StreamSerialiser & outs ) const
		{
			_compression->values >> outs;

			if (_compression->gradfield != NULL)
			{
				_compression->gradfield->dx >> outs;
				_compression->gradfield->dy >> outs;
				_compression->gradfield->dz >> outs;
			}

			if (_compression->colors != NULL)
			{
				_compression->colors->r >> outs;
				_compression->colors->g >> outs;
				_compression->colors->b >> outs;
				_compression->colors->a >> outs;
			}

			return outs;
		}


		CompressedDataAccessor::CompressedDataAccessor( boost::recursive_mutex & m, CompressedDataBase * compression ) 
		: template_CompressedDataAccessor(m, compression)
		{

		}

		StreamSerialiser & CompressedDataAccessor::operator<<( StreamSerialiser & ins )
		{
			_compression->values << ins;

			if (_compression->gradfield != NULL)
			{
				_compression->gradfield->dx << ins;
				_compression->gradfield->dy << ins;
				_compression->gradfield->dz << ins;
			}

			if (_compression->colors != NULL)
			{
				_compression->colors->r << ins;
				_compression->colors->g << ins;
				_compression->colors->b << ins;
				_compression->colors->a << ins;
			}

			return ins;
		}
	}
}
