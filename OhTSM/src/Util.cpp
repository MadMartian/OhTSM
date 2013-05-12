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
}
