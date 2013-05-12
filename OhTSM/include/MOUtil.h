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

#ifndef __OVERHANGTERRAINMETAOBJECTUTILS_H__
#define __OVERHANGTERRAINMETAOBJECTUTILS_H__

#include "OverhangTerrainPrerequisites.h"

#include "MetaObject.h"
#include "Util.h"
#include "IsoSurfaceSharedTypes.h"

namespace Ogre
{
	/// Implemented by metaobjects to specify their discrete sampling function(s)
	template< typename Subclass, typename ObjectType >
	class FieldStrengthFunctorBase
	{
	protected:
		/// The concrete metaobject object
		const ObjectType * const self;
		/// The voxel grid that will be updated
		const Voxel::CubeDataRegion * const dg;

	public:
		/** 
		@param pObject The concrete metaobject object
		@param pDataGrid The voxel grid that will be updated
		*/
		FieldStrengthFunctorBase (const ObjectType * pObject, const Voxel::CubeDataRegion * pDataGrid)
			: self(pObject), dg(pDataGrid) {}
		
		/// @returns Discretely samples a voxel at the specified 3D cartesian voxel grid-point coordinates
		inline
		int operator () (const signed int x, const signed int y, const signed int z) const
		{
			return (int)floor(static_cast< const Subclass * > (this) ->getFieldStrength(x, y, z) * Real(Voxel::FS_Mantissa) + 0.5f);
		}

		/// @returns Discretely samples a voxel at the specified 3D cartesian voxel grid-point coordinates
		/* abstract */ Real getFieldStrength(const signed int x, const signed int y, const signed int z) const;
	};

	/** Updates a region of the voxel grid
	@param pDataGrid The voxel grid to update
	@param data Access to the voxel grid that will be updated
	@param x0 Bounding-box minimum x-coordinate of the update region
	@param y0 Bounding-box minimum y-coordinate of the update region
	@param z0 Bounding-box minimum z-coordinate of the update region
	@param xN Bounding-box maximum x-coordinate of the update region
	@param yN Bounding-box maximum y-coordinate of the update region
	@param zN Bounding-box maximum z-coordinate of the update region
	@param fntorGetFieldStrength A concrete specialization of FieldStrengthFunctorBase
	*/
	template< typename Functor >
	void updateDataGrid(
		const Voxel::CubeDataRegion * pDataGrid,
		Voxel::DataAccessor & data,
		const int x0, const int y0, const int z0, 
		const int xN, const int yN, const int zN, 
		const Functor & fntorGetFieldStrength
	)
	{
#ifdef _DEBUG
		const int nDN = pDataGrid->getDimensions()+1;
		OgreAssert(
			x0 >= -1 && y0 >= -1 && z0 >= -1 && xN <= nDN && yN <= nDN && zN <= nDN,
			"Update region out-of-bounds"
		);
#endif

		for (FieldAccessor::iterator i = data.voxels.iterate(x0, y0, z0, xN, yN, zN); i; ++i)
		{
			const FieldAccessor::Coords & coords = i;
			data.addValueAt(fntorGetFieldStrength(coords.i, coords.j, coords.k), i);
		}
	}
}

#endif