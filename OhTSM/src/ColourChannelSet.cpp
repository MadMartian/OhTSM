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

#include "ColourChannelSet.h"
#include "CubeDataRegionDescriptor.h"

namespace Ogre
{
	namespace Voxel
	{
		void ColourChannelSet::clear()
		{
			memset(r, 0, _count);
			memset(g, 0, _count);
			memset(b, 0, _count);
			memset(a, 0, _count);
		}

		ColourChannelSet::ColourChannelSet( const CubeDataRegionDescriptor & dgtmpl, unsigned char * r, unsigned char * g, unsigned char * b, unsigned char * a ) 
			: _count(dgtmpl.gpcount), r(r), g(g), b(b), a(a)
		{}
	}
}
