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

#include "MetaBall.h"
#include "CubeDataRegion.h"
#include "MetaWorldFragment.h"
#include "IsoSurfaceSharedTypes.h"

#include "MOUtil.h"

namespace Ogre
{
	using namespace Voxel;

	MetaBall::MetaBall(const Vector3& position, Real radius, bool excavating)
	  : MetaObject(position), _sphere(position, radius), _fExcavating(excavating ? 1.0f : -1.0f)
	{
	}

	void MetaBall::updateDataGrid(const CubeDataRegion * pDG, DataAccessor * pAccess)
	{
		AxisAlignedBox aabb(
			_pos - _sphere.getRadius()*Vector3::UNIT_SCALE,
			_pos + _sphere.getRadius()*Vector3::UNIT_SCALE);

		WorldCellCoords
			wccur0, wccurN;

		// Find the grid points this meta ball can possibly affect
		if (!pDG->mapRegion(aabb, wccur0, wccurN))
			return;

		class MyFunctor : public FieldStrengthFunctorBase< MyFunctor, MetaBall >
		{
		public:
			MyFunctor (const MetaBall * self, const CubeDataRegion * pDG)
				: FieldStrengthFunctorBase(self, pDG) {}

			inline
			Real getFieldStrength (const signed int x, const signed int y, const signed int z) const
			{
				Vector3 v = dg->getBoxSize().getMinimum();
				
				v += Vector3((Real)x, (Real)y, (Real)z) * dg->getGridScale();
				v -= self->_pos;
				v += dg->getPosition();

				Real r2 = Real(v.squaredLength() / (2.0 * self->_sphere.getRadius()*self->_sphere.getRadius()));

				const Real i = r2 - 0.5f;
				
				// Cubic-spline has better fall-off than parabolic polynomial
				// Builds on Metaballs from http://www.geisswerks.com/ryan/BLOBS/blobs.html
				const Real k = -0.4f*i*i*i + 0.8f*i*i - 0.5f*i;	
				return k * self->_fExcavating;
			}
		} fntor(this, pDG);

		Ogre::updateDataGrid (pDG, *pAccess, wccur0.i, wccur0.j, wccur0.k, wccurN.i, wccurN.j, wccurN.k, fntor);
	}

	AxisAlignedBox MetaBall::getAABB() const
	{
		return AxisAlignedBox(
			  _sphere.getCenter() - _sphere.getRadius()*Vector3::UNIT_SCALE,
			  _sphere.getCenter() + _sphere.getRadius()*Vector3::UNIT_SCALE
		);
	}

	void MetaBall::setPosition( const Vector3 & pos )
	{
		_pos = pos;
		_sphere.setCenter(pos);
	}

	void MetaBall::write( StreamSerialiser & output ) const
	{
		const Real nRadius = _sphere.getRadius();
		MetaObject::write(output);
		output.write(&_fExcavating);
		output.write(&nRadius);
	}

	void MetaBall::read( StreamSerialiser & input )
	{
		Real nRadius;

		MetaObject::read(input);
		input.read(&_fExcavating);
		input.read(&nRadius);

		_sphere.setRadius(nRadius);
		_sphere.setCenter(_pos);
	}

}
