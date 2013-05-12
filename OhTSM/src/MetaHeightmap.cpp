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

#include "pch.h"

#include "MetaHeightMap.h"
#include "CubeDataRegion.h"
#include "PageSection.h"
#include "TerrainTile.h"
#include "IsoSurfaceSharedTypes.h"

#include "MOUtil.h"

namespace Ogre
{
	using namespace Voxel;

	Ogre::Vector3 MetaHeightMap::computePosition( const TerrainTile * pTile )
	{
		Vector3 pt = pTile->getPosition(OCS_Vertex);

		pt.z = 0;
		pTile->getManager().transformSpace(OCS_Vertex, OCS_World, pt);

		return pt;
	}

	MetaHeightMap::MetaHeightMap(const TerrainTile * t)
	: _pTerrainTile(t)
	{
	}

	/// Adds this meta heightmap to the access grid.
	void MetaHeightMap::updateDataGrid(const CubeDataRegion * pDG, DataAccessor * pAccess)
	{
		assert(_pTerrainTile);

		class MyFunctor : public FieldStrengthFunctorBase< MyFunctor, MetaHeightMap >
		{
		public:
			MyFunctor (const MetaHeightMap * self, const CubeDataRegion * pDG)
				: FieldStrengthFunctorBase(self, pDG) {}

			inline
			Real getFieldStrength (const signed int x, const signed int y, const signed int z) const
			{
				return (dg->getBoxSize().getMinimum().y + y * dg->getGridScale() + dg->getPosition().y - self->_pTerrainTile->height(x, z)) / dg->getGridScale();
			}
		} fntor(this, pDG);

		const size_t nDN = pDG->getDimensions()+1;
		Ogre::updateDataGrid (pDG, *pAccess, -1, -1, -1, nDN, nDN, nDN, fntor);
	}

	Ogre::AxisAlignedBox MetaHeightMap::getAABB() const
	{
		assert(_pTerrainTile);
		return _pTerrainTile->getHeightMapBBox();
	}

	void MetaHeightMap::write( StreamSerialiser & output ) const
	{
		MetaObject::write(output);
	}

	void MetaHeightMap::read( StreamSerialiser & input )
	{
		MetaObject::read(input);
	}

	void MetaHeightMap::updatePosition()
	{
		setPosition(computePosition(_pTerrainTile));
	}

}/// namespace Ogre
