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

#ifndef __OVERHANGTERRAINCUBEDATAREGION_H__
#define __OVERHANGTERRAINCUBEDATAREGION_H__

#include <OgreAxisAlignedBox.h>
#include <OgreSharedPtr.h>
#include <OgreStreamSerialiser.h>

#include "Neighbor.h"
#include "Util.h"

#include "OverhangTerrainPrerequisites.h"
#include "OverhangTerrainOptions.h"

#include "CubeDataRegionDescriptor.h"
#include "FieldAccessor.h"
#include "IsoSurfaceSharedTypes.h"
#include "ColourChannelSet.h"
#include "GradientField.h"
#include "RLE.h"
#include "DataBase.h"

namespace Ogre
{
	namespace Voxel 
	{
		class IDataBaseHook
		{
		public:
			virtual void released(DataBase * pDataBucket) = 0;
			virtual void released(const DataBase * pDataBucket) const = 0;
		};

		class CompressedDataBase
		{
		public:
			struct GradientChannels
			{
				RLE::Channel dx, dy, dz;
			} * const gradfield;

			struct ColorChannels
			{
				RLE::Channel r, g, b, a;
			} * const colors;

			struct TexCoordChannels
			{
				RLE::Channel u, v;
			} * const texcoords;

			RLE::Channel values;

			CompressedDataBase(const size_t nVRFlags);
			~CompressedDataBase();

			void operator << (const DataBase & database);
			void operator >> (DataBase & database) const;
		};

		template< typename HOOK, typename BUCKET, typename FIELDSTRENGTH, typename VOXELGRID, typename COLOURSET, typename GRADIENTFIELD >
		class template_DataAccessor
		{
		private:
			class AtomicResource
			{
			private:
				boost::recursive_mutex::scoped_lock	_lock;
				HOOK * const _pHook;
				BUCKET * _pBucket;

			public:
				AtomicResource(boost::recursive_mutex & mutex, BUCKET * pBucket, HOOK * pHook)
					: _lock(mutex), _pBucket(pBucket), _pHook(pHook) {}
				~AtomicResource()
				{
					_pHook->released(_pBucket);
				}
			};

			SharedPtr< AtomicResource > _pResource;

			friend class CubeDataRegion;

		protected:
			const CubeDataRegionDescriptor & _dgtmpl;

		public:
			const size_t count;

			typedef FIELDSTRENGTH FieldStrength;
			typedef VOXELGRID VoxelGrid;
			typedef COLOURSET ColourSet;
			typedef GRADIENTFIELD GradientField;

			FIELDSTRENGTH * const values;
			VOXELGRID voxels;
			COLOURSET colours;
			GRADIENTFIELD gradients;

			template_DataAccessor(
				boost::recursive_mutex & mutex,
				BUCKET * pBucket,
				HOOK * pDataBaseHook,
				const CubeDataRegionDescriptor & dgtmpl
			)
				:	_pResource(new AtomicResource(mutex, pBucket, pDataBaseHook)),
					_dgtmpl(dgtmpl),
					gradients(dgtmpl, pBucket->dx, pBucket->dy, pBucket->dz), 
					colours(dgtmpl, pBucket->red, pBucket->green, pBucket->blue, pBucket->alpha), 
					values(pBucket->values),
					voxels(dgtmpl, pBucket->values),
					count(dgtmpl.gpcount)
			{
			}

			template_DataAccessor(template_DataAccessor && move)
				:	_pResource(move._pResource),
					_dgtmpl(move._dgtmpl), 
					gradients(const_cast< Ogre::Voxel::GradientField && > (move.gradients)), 
					colours(const_cast< Ogre::Voxel::ColourChannelSet && > (move.colours)),
					values(move.values), 
					voxels(const_cast< Ogre::Voxel::FieldAccessor && > (move.voxels)), 
					count(move.count) 
			{
			}

			template_DataAccessor(const template_DataAccessor & copy)
				:	_pResource(copy._pResource),
					_dgtmpl(copy._dgtmpl), 
					gradients(copy.gradients), colours(copy.colours),
					values(copy.values), voxels(copy.voxels), count(copy.count) 
			{
			}
		};

		class _OverhangTerrainPluginExport const_DataAccessor : public template_DataAccessor< const IDataBaseHook, const DataBase, const FieldStrength, FieldAccessor, const ColourChannelSet, const GradientField >
		{
		public:
			const_DataAccessor(
				boost::recursive_mutex & mutex,
				const DataBase * pBucket,
				const IDataBaseHook * pDataBaseHook,
				const CubeDataRegionDescriptor & dgtmpl
			);
			const_DataAccessor( const const_DataAccessor & copy);
			const_DataAccessor( const_DataAccessor && move);
		};

		class _OverhangTerrainPluginExport DataAccessor : public template_DataAccessor< IDataBaseHook, DataBase, FieldStrength, FieldAccessor, ColourChannelSet, GradientField >
		{
		private:
			inline void addValueTo (const int delta, FieldStrength & vout)
			{
				using namespace Voxel;
				using bitmanip::clamp;

				const int val = delta + vout;

				static_assert(FS_MaxClosed < FS_MaxOpen, "Expected FS_MaxClosed to be less than FS_MaxOpen");
				vout = FieldStrength(clamp(int(FS_MaxClosed), int(FS_MaxOpen), val));
			}

		public:
			DataAccessor(
				boost::recursive_mutex & mutex,
				DataBase * pBucket,
				IDataBaseHook * pDataBaseHook,
				const CubeDataRegionDescriptor & dgtmpl
			);
			DataAccessor(const DataAccessor & copy);
			DataAccessor(DataAccessor && move);

			inline void addValueAt (const int delta, FieldAccessor::iterator i)
			{
				addValueTo(delta, *i);
			}

			inline void addValueAt (const int delta, const int x, const int y, const int z)
			{
				addValueTo(delta, voxels(x, y, z));
			}

			void updateGradient();

			enum EmptySet
			{
				Empty_None,
				Empty_Solid,
				Empty_Clear
			} getEmptyStatus();
			void reset();
			void clear();
		};

		template< typename CompressedDataBase >
		class template_CompressedDataAccessor
		{
		private:
			boost::recursive_mutex::scoped_lock	_lock;

			template_CompressedDataAccessor(const template_CompressedDataAccessor &);

		protected:
			CompressedDataBase * const _compression;

		public:
			template_CompressedDataAccessor(boost::recursive_mutex & m, CompressedDataBase * compression)
				: _lock(m), _compression(compression) {}
			template_CompressedDataAccessor(template_CompressedDataAccessor && move)
				: _lock(static_cast< boost::recursive_mutex::scoped_lock && > (move._lock)), _compression(move._compression) {}
		};

		class _OverhangTerrainPluginExport const_CompressedDataAccessor : public template_CompressedDataAccessor< const CompressedDataBase >
		{
		private:
			const_CompressedDataAccessor(const const_CompressedDataAccessor &);

		public:
			const_CompressedDataAccessor(boost::recursive_mutex & m, const CompressedDataBase * compression);
			const_CompressedDataAccessor(const_CompressedDataAccessor && move)
				: template_CompressedDataAccessor(static_cast< template_CompressedDataAccessor && > (move)) {}

			StreamSerialiser & operator >> (StreamSerialiser & outs) const;
		};

		class _OverhangTerrainPluginExport CompressedDataAccessor : public template_CompressedDataAccessor< CompressedDataBase >
		{
		private:
			CompressedDataAccessor(const CompressedDataAccessor &);

		public:
			CompressedDataAccessor(boost::recursive_mutex & m, CompressedDataBase * compression);
			CompressedDataAccessor(CompressedDataAccessor && move)
				: template_CompressedDataAccessor(static_cast< template_CompressedDataAccessor && > (move)) {}

			StreamSerialiser & operator << (StreamSerialiser & ins);
		};

		class _OverhangTerrainPluginExport CubeDataRegion : private IDataBaseHook
		{
		public:
			const CubeDataRegionDescriptor & meta;

			CubeDataRegion(const size_t nVRFlags, DataBasePool * pPool, const CubeDataRegionDescriptor & dgtmpl, const AxisAlignedBox & bbox = AxisAlignedBox::BOX_NULL );
			virtual ~CubeDataRegion();

			inline 
				DimensionType getDimensions() const {return meta.dimensions; }
			inline 
				Real getGridScale() const {return meta.scale; }

			inline 
				const IsoFixVec3* getVertices() const {return meta.getVertices(); }

			inline
				VoxelIndex getGridPointIndex(const size_t x, const size_t y, const size_t z) const { return meta.getGridPointIndex(x, y, z); }

			const AxisAlignedBox& getBoundingBox() const {return _bbox; }
			inline 
				const AxisAlignedBox & getBoxSize() const { return meta.getBoxSize(); }
			/** Maps an axis aligned box to the grid points inside it.
			@remarks
			This function modifies the values of x0, y0, z0, x1, y1, and z1.
			(x0, y0, z0) will be the 'minimum' grid point <i>inside</i> the axis aligned box, and
			(x1, y1, z1) will be the 'maximum' grid point <i>inside</i> the axis aligned box.
			I.e. the points (x, y, z) for which (x0, y0, z0) <= (x, y, z) <= (x1, y1, z1) lie inside the axis aligned box.
			@returns
			false if the box is completely outside the grid, otherwise it returns true.
			@note
			If the function returns true, it is safe to use the values of (x0, y0, z0) and (x1, y1, z1) in a call to getGridPointIndex().
			@warning
			If the function returns false, the contents of x0, y0, z0, x1, y1, and z1 are undefined. */
			bool mapRegion(const AxisAlignedBox& aabb, WorldCellCoords & gp0, WorldCellCoords & gpN) const;

			bool hasGradient() const {return (_nVRFlags & VRF_Gradient) != 0; }
			bool hasColours() const {return (_nVRFlags & VRF_Colours) != 0; }
			bool hasTexCoords() const { return (_nVRFlags & VRF_TexCoords) != 0; }

			virtual StreamSerialiser & operator >> (StreamSerialiser & output) const;
			virtual StreamSerialiser & operator << (StreamSerialiser & input);

		private:
			mutable boost::recursive_mutex _mutex;

			size_t _nVRFlags;

			mutable DataBasePool * _pPool;

			CompressedDataBase * _compression;

			/// Bounding box of the grid.
			AxisAlignedBox _bbox;

			static AxisAlignedBox createBoundingBox (const CubeDataRegionDescriptor & dgtmpl, const Vector3 & pos)
			{
				const Real offs = dgtmpl.dimensions * dgtmpl.scale / 2;
				return AxisAlignedBox(pos - offs, pos + offs);
			}

			virtual void released(DataBase * pDataBucket);
			virtual void released(const DataBase * pDataBucket) const;

			void populate (DataBase * pDataBucket) const;

		public:
			DataAccessor lease ();
			const_DataAccessor lease () const;
			DataAccessor * lease_p ();
			const_DataAccessor * lease_p () const;

			CompressedDataAccessor clease();
			const_CompressedDataAccessor clease () const;
		};
	}
}// namespace Ogre
#endif // DATA_GRID_H
