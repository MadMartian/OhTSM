/*
-----------------------------------------------------------------------------
This source file is part of the Overhang Terrain Scene Manager library (OhTSM)
for use with OGRE.

Copyright (c) 2013 extollIT Enterprises
http://www.extollit.com

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

You may alternatively use this source under the terms of a specific version of
the OGRE Unrestricted License provided you have obtained such a license from
Torus Knot Software Ltd.
-----------------------------------------------------------------------------
*/

#include "pch.h"

#include "Util.h"

#include <boost/thread.hpp>
#include <map>
#include <set>
#include <string>
#include <limits>

#include "OverhangTerrainManager.h"

unsigned long long computeCRCImpl( const unsigned long long * pData, const size_t nQWords )
{
	unsigned long long v = 0;

	assert(nQWords > 0);
	for (size_t i = 0; i < nQWords; ++i)
		v += pData[i] + i;

	return v;
}

namespace Ogre
{
	namespace Math2
	{
		static const Real RATIONAL_ERROR = powf(10.0f, -static_cast< float > (std::numeric_limits< Real >::digits10) + 1.0f);
	}

	void DiscreteRayIterator::iterate()
	{
		_dist = (_fspan - _walker) / _incrementor;

		while (
			_walker.x < _fspan &&
			_walker.y < _fspan &&
			_walker.z < _fspan
		)
			_walker += _incrementor * _fspan;

		if (_walker.x >= _fspan
			&& (_dist.x <= _dist.y && _dist.x <= _dist.z))
		{
			_cell.i += _delta.x * _ispan;
			_walker.x -= _fspan;
		}
		else if (_walker.y >= _fspan
			&& (_dist.y <= _dist.x && _dist.y <= _dist.z))
		{
			_cell.j += _delta.y * _ispan;
			_walker.y -= _fspan;
		}
		else
		{
			_cell.k += _delta.z * _ispan;
			_walker.z -= _fspan;
		}
	}

	Ogre::Vector3 DiscreteRayIterator::getPosition() const
	{
		Vector3 v;

		v.x = (Real)_cell.i + _walker.x * (Real)_delta.x;
		v.y = (Real)_cell.j + _walker.y * (Real)_delta.y;
		v.z = (Real)_cell.k + _walker.z * (Real)_delta.z;

		return v;
	}

	bool DiscreteRayIterator::operator == (const DiscreteRayIterator & other) const
	{
		return 
			_origin.positionEquals(other._origin) &&
			_incrementor.directionEquals(other._incrementor, Radian(0.0000001f)) &&
			_cell == other._cell;
	}

	bool DiscreteRayIterator::operator != (const DiscreteRayIterator & other) const
	{
		return 
			!_origin.positionEquals(other._origin) ||
			!_incrementor.directionEquals(other._incrementor, Radian(std::numeric_limits< Real >::epsilon())) ||
			_cell != other._cell;
	}

	DiscreteRayIterator & DiscreteRayIterator::operator ++ ()
	{
		iterate();
		return *this;
	}

	DiscreteRayIterator DiscreteRayIterator::operator ++ (int)
	{
		DiscreteRayIterator old = *this;
		iterate();
		return old;
	}

	DiscreteRayIterator::DiscreteRayIterator( const Vector3 & ptOrigin, const Vector3 & direction, const Real limit /*=0*/) 
	:	_origin(ptOrigin),
		_walker(ptOrigin),
		_limit_sq(limit*limit),
		_incrementor(
			Math::Abs(direction.x),
			Math::Abs(direction.y),
			Math::Abs(direction.z)
		),
		_intersection(false, 0.0f)
	{
		_delta.x = direction.x < 0.0f ? -1 : 1;
		_delta.y = direction.y < 0.0f ? -1 : 1;
		_delta.z = direction.z < 0.0f ? -1 : 1;

		_cell.i = (signed short)floor(ptOrigin.x);
		_cell.j = (signed short)floor(ptOrigin.y);
		_cell.k = (signed short)floor(ptOrigin.z);

		cell &= _cell;

		_walker.x -= _cell.i;
		_walker.y -= _cell.j;
		_walker.z -= _cell.k;

		if (_delta.x < 0)
			_walker.x = -_walker.x + (short)1;
		if (_delta.y < 0)
			_walker.y = -_walker.y + (short)1;
		if (_delta.z < 0)
			_walker.z = -_walker.z + (short)1;

		OgreAssert(_walker.x >= (short)0 && _walker.y >= (short)0 && _walker.z >= (short)0, "Walker must be positive");

		OHT_DBGTRACE("RayCellWalk: delta=<" << _delta.x << ',' << _delta.y << ',' << _delta.z << ">, wcell=" << _cell << ", walker=" << _walker << ", inc=" << _incrementor);
	}

	std::pair< bool, Real > BBox2D::intersects( const Ray & ray, const OverhangCoordinateSpace ocs /*= OCS_Terrain*/ ) const
	{
		Vector3
			p = ray.getOrigin(),
			d = ray.getDirection();

		OverhangTerrainManager::transformSpace(ocs, ALIGN_X_Z, OCS_Terrain, p);
		OverhangTerrainManager::transformSpace(ocs, ALIGN_X_Z, OCS_Terrain, d);

		if (
			(p.x > minimum.x && p.x < maximum.x) 
			&& 
			(p.y > minimum.y && p.y < maximum.y)
		)
			return std::pair< bool, Real > (true, 0.0f);

		const Real
			mx0 = (minimum.x - p.x) / d.x,
			mxN = (maximum.x - p.x) / d.x,
			my0 = (minimum.y - p.y) / d.y,
			myN = (maximum.y - p.y) / d.y;

		const Real
			x0_y = d.y * mx0,
			xN_y = d.y * mxN,
			y0_x = d.x * my0,
			yN_x = d.x * myN;

		return
			std::pair< bool, Real > (
				(x0_y >= minimum.y && x0_y <= maximum.y && mx0 >= 0) ||
				(xN_y >= minimum.y && xN_y <= maximum.y && mxN >= 0) ||
				(y0_x >= minimum.x && y0_x <= maximum.x && my0 >= 0) ||
				(yN_x >= minimum.x && yN_x <= maximum.x && myN >= 0),

				std::min(mx0, std::min(mxN, std::min(my0, myN)))	// TODO: Doesn't appear to be correct
			);
	}

	bool BBox2D::intersects( const AxisAlignedBox & bbox, const OverhangCoordinateSpace ocs /*= OCS_Terrain*/ ) const
	{
		Vector3
			p0 = bbox.getMinimum(),
			pN = bbox.getMaximum();

		OverhangTerrainManager::transformSpace(ocs, ALIGN_X_Z, OCS_Terrain, p0);
		OverhangTerrainManager::transformSpace(ocs, ALIGN_X_Z, OCS_Terrain, pN);

		return
			(maximum.x >= p0.x && maximum.y >= p0.y) &&
			(minimum.x <= pN.x && minimum.y <= pN.y);
	}

	BBox2D::BBox2D()
	{

	}

	BBox2D::BBox2D( const Vector2 & minimum, const Vector2 & maximum )
		: minimum(minimum), maximum(maximum)
	{

	}

}
