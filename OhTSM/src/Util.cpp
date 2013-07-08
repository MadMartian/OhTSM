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

	std::pair< bool, Real > BBox2D::intersects( const Ray & ray, const OverhangCoordinateSpace ocs /*= OCS_Terrain*/ ) const
	{
		Vector3 
			p = ray.getOrigin(),
			d = ray.getDirection();

		OverhangTerrainManager::transformSpace(ocs, ALIGN_X_Z, OCS_Terrain, p);
		OverhangTerrainManager::transformSpace(ocs, ALIGN_X_Z, OCS_Terrain, d);
		
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

				std::min(mx0, std::min(mxN, std::min(my0, myN)))
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
