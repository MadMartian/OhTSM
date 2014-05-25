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

#include "DebugTools.h"

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

	const char * Touch3DFlagNames[CountTouch3DSides] = {
		"",
		"W",
		"E",
		"WE",
		"B",
		"WB",
		"EB",
		"WEB",
		"A",
		"WA",
		"EA",
		"WEA",
		"BA",
		"WBA",
		"EBA",
		"WEBA",
		"N",
		"WN",
		"EN",
		"WEN",
		"BN",
		"WBN",
		"EBN",
		"WEBN",
		"AN",
		"WAN",
		"EAN",
		"WEAN",
		"BAN",
		"WBAN",
		"EBAN",
		"WEBAN",
		"S",
		"WS",
		"ES",
		"WES",
		"BS",
		"WBS",
		"EBS",
		"WEBS",
		"AS",
		"WAS",
		"EAS",
		"WEAS",
		"BAS",
		"WBAS",
		"EBAS",
		"WEBAS",
		"NS",
		"WNS",
		"ENS",
		"WENS",
		"BNS",
		"WBNS",
		"EBNS",
		"WEBNS",
		"ANS",
		"WANS",
		"EANS",
		"WEANS",
		"BANS",
		"WBANS",
		"EBANS",
		"WEBANS"
	};
	const signed char Touch3DSide_to_Moore3DNeighbor[CountTouch3DSides] =
	{
		Moore3NaN,
		Moore3N_WEST,
		Moore3N_EAST,
		Moore3NaN,
		Moore3N_BELOW,
		Moore3N_BELOWWEST,
		Moore3N_BELOWEAST,
		Moore3N_BELOW,
		Moore3N_ABOVE,
		Moore3N_ABOVEWEST,
		Moore3N_ABOVEEAST,
		Moore3N_ABOVE,
		Moore3NaN,
		Moore3N_WEST,
		Moore3N_EAST,
		Moore3NaN,
		Moore3N_NORTH,
		Moore3N_NORTHWEST,
		Moore3N_NORTHEAST,
		Moore3N_NORTH,
		Moore3N_BELOWNORTH,
		Moore3N_BELOWNORTHWEST,
		Moore3N_BELOWNORTHEAST,
		Moore3N_BELOWNORTH,
		Moore3N_ABOVENORTH,
		Moore3N_ABOVENORTHWEST,
		Moore3N_ABOVENORTHEAST,
		Moore3N_ABOVENORTH,
		Moore3N_NORTH,
		Moore3N_NORTHWEST,
		Moore3N_NORTHEAST,
		Moore3N_NORTH,
		Moore3N_SOUTH,
		Moore3N_SOUTHWEST,
		Moore3N_SOUTHEAST,
		Moore3N_SOUTH,
		Moore3N_BELOWSOUTH,
		Moore3N_BELOWSOUTHWEST,
		Moore3N_BELOWSOUTHEAST,
		Moore3N_BELOWSOUTH,
		Moore3N_ABOVESOUTH,
		Moore3N_ABOVESOUTHWEST,
		Moore3N_ABOVESOUTHEAST,
		Moore3N_ABOVESOUTH,
		Moore3N_SOUTH,
		Moore3N_SOUTHWEST,
		Moore3N_SOUTHEAST,
		Moore3N_SOUTH,
		Moore3NaN,
		Moore3N_WEST,
		Moore3N_EAST,
		Moore3NaN,
		Moore3N_BELOW,
		Moore3N_BELOWWEST,
		Moore3N_BELOWEAST,
		Moore3N_BELOW,
		Moore3N_ABOVE,
		Moore3N_ABOVEWEST,
		Moore3N_ABOVEEAST,
		Moore3N_ABOVE,
		Moore3NaN,
		Moore3N_WEST,
		Moore3N_EAST,
		Moore3NaN
	};

	const Touch3DSide OrthogonalNeighbor_to_Touch3DSide[CountOrthogonalNeighbors] =
	{
		T3DS_North /*OrthoN_NORTH*/,
		T3DS_East /*OrthoN_EAST*/,
		T3DS_West /*OrthoN_WEST*/,
		T3DS_South /*OrthoN_SOUTH*/,
		T3DS_Aether /*OrthoN_ABOVE*/,
		T3DS_Nether /*OrthoN_BELOW*/		
	};

	Ray & clamp(Ray & ray, const float fTolerance)
	{
		Vector3 vDir = ray.getDirection();

		if (vDir.x > -fTolerance && vDir.x < +fTolerance)
			vDir.x = vDir.x < 0 ? -fTolerance : +fTolerance;

		if (vDir.y > -fTolerance && vDir.y < +fTolerance)
			vDir.y = vDir.y < 0 ? -fTolerance : +fTolerance;

		if (vDir.z > -fTolerance && vDir.z < +fTolerance)
			vDir.z = vDir.z < 0 ? -fTolerance : +fTolerance;

		ray.setDirection(vDir);
		return ray;
	}

	void DiscreteRayIterator::iterate()
	{
		_dist = (1.0f - _walker) / _incrementor;

		while (
			_walker.x < 1 &&
			_walker.y < 1 &&
			_walker.z < 1
		)
		{
			++_ldist;
			_walker += _incrementor;
		}

		size_t t3dsf = 0;

		if (_walker.x >= 1.0f
			&& (_dist.x <= _dist.y && _dist.x <= _dist.z))
		{
			t3dsf |= (T3DS_East >> (_delta.mx & 1));
			_cell.i += _delta.x;
			_walker.x -= 1.0f;
			_off = _walker.x / _incrementor.x;
		}
		else if (_walker.y >= 1.0f
			&& (_dist.y <= _dist.x && _dist.y <= _dist.z))
		{
			t3dsf |= (T3DS_Aether >> (_delta.my & 1));
			_cell.j += _delta.y;
			_walker.y -= 1.0f;
			_off = _walker.y / _incrementor.y;
		}
		else
		{
			t3dsf |= (T3DS_South >> (_delta.mz & 1));
			_cell.k += _delta.z;
			_walker.z -= 1.0f;
			_off = _walker.z / _incrementor.z;
		}

		_t3ds = (Touch3DSide)t3dsf;
	}

	void DiscreteRayIterator::PositionPropertyAccessor::updatePosition(const Real offset /*= 0.0f*/) const
	{
		_vector = _pRay->getPoint(*_pLDist * *_pfSpan + offset);
	}
	void DiscreteRayIterator::PositionPropertyAccessor::DI (const Ray & ray, const Real & fLDist, const Real & fspan)
	{
		_pRay = &ray;
		_pLDist = &fLDist;
		_pfSpan = &fspan;
	}

	void DiscreteRayIterator::IntersectionPropertyAccessor::DI( const Ray & ray, const Real & fLDist, const Real & off, const Real & fspan )
	{
		PositionPropertyAccessor::DI(ray, fLDist, fspan);
		_pOff = & off;
		_pfSpan = & fspan;
	}

	void DiscreteRayIterator::IntersectionPropertyAccessor::updatePosition(const Real offset /*= 0.0f*/) const
	{
		PositionPropertyAccessor::updatePosition(offset - *_pOff * *_pfSpan);
	}

	void DiscreteRayIterator::LinearDistancePropertyAccessor::DI( const Real & fLDist, const Real & fSpan )
	{
		_pLDist = &fLDist;
		_pfSpan = &fSpan;
	}

	bool DiscreteRayIterator::operator == (const DiscreteRayIterator & other) const
	{
		return 
			ray.getOrigin().positionEquals(other.ray.getOrigin()) &&
			_incrementor.directionEquals(other._incrementor, Radian(std::numeric_limits< Real >::epsilon())) &&
			_cell == other._cell;
	}

	bool DiscreteRayIterator::operator != (const DiscreteRayIterator & other) const
	{
		return 
			!ray.getOrigin().positionEquals(other.ray.getOrigin()) ||
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

	DiscreteRayIterator::DiscreteRayIterator( const Ray & ray, const Real fCellSize, const Vector3 & offset /*= Vector3::ZERO */ )
	:	ray(ray),
		offset(offset),
		_walker((ray.getOrigin() - offset) / fCellSize),
		_incrementor(
			Math::Abs(ray.getDirection().x),
			Math::Abs(ray.getDirection().y),
			Math::Abs(ray.getDirection().z)
		),
		_ldist(0),
		_off(0),
		_t3ds(T3DS_None),
		_fspan(fCellSize),
		_ispan((signed long)fCellSize)
	{
		const Vector3
			direction = ray.getDirection(),
			origin = ray.getOrigin() - offset;

		_delta.x = direction.x < 0.0f ? -1 : 1;
		_delta.y = direction.y < 0.0f ? -1 : 1;
		_delta.z = direction.z < 0.0f ? -1 : 1;

		_delta.mx = (((_delta.x + 1) >> 1) - 1);
		_delta.my = (((_delta.y + 1) >> 1) - 1);
		_delta.mz = (((_delta.z + 1) >> 1) - 1);

		_cell.i = (signed short)floor(origin.x / fCellSize);
		_cell.j = (signed short)floor(origin.y / fCellSize);
		_cell.k = (signed short)floor(origin.z / fCellSize);

		DI();

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

		OHT_DBGTRACE("DiscreteRayIterator: delta=<" << _delta.x << ',' << _delta.y << ',' << _delta.z << ">, wcell=" << _cell << ", walker=" << _walker << ", inc=" << _incrementor);
	}

	DiscreteRayIterator::DiscreteRayIterator( const DiscreteRayIterator & copy )
	: _walker(copy._walker), _incrementor(copy._incrementor), _delta(copy._delta), _dist(copy._dist), _fspan(copy._fspan), _ldist(copy._ldist), _off(copy._off), _ispan(copy._ispan), _cell(copy._cell), _t3ds(copy._t3ds), ray(copy.ray), offset(copy.offset)
	{
		DI();
	}

	DiscreteRayIterator::DiscreteRayIterator( DiscreteRayIterator && move )
	: _walker(move._walker), _incrementor(move._incrementor), _delta(move._delta), _dist(move._dist), _fspan(move._fspan), _ldist(move._ldist), _off(move._off), _ispan(move._ispan), _cell(move._cell), _t3ds(move._t3ds), ray(move.ray), offset(move.offset)
	{
		DI();
	}

	void DiscreteRayIterator::DI()
	{
		position.DI(ray, _ldist, _fspan);
		intersection.DI(ray, _ldist, _off, _fspan);
		distance.DI(_ldist, _fspan);
		neighbor &= _t3ds;
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

	RoleSecureFlag::Flag::operator bool() const
	{
		return _flag;
	}

	bool RoleSecureFlag::Flag::operator--()
	{
		return _flag = false;
	}

	bool RoleSecureFlag::Flag::operator++()
	{
		return _flag = true;
	}

	bool RoleSecureFlag::Flag::operator!() const
	{
		return !_flag;
	}

	RoleSecureFlag::Flag::Flag()
		: _flag (false)
	{

	}

	RoleSecureFlag::Flag::~Flag()
	{

	}

}
