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

#include "GradientField.h"

namespace Ogre
{
	namespace Voxel
	{
		GradientField::GradientField( const CubeDataRegionDescriptor & cubemeta, signed char * dx, signed char * dy, signed char * dz ) 
		: _count(cubemeta.gpcount), _dx(dx), _dy(dy), _dz(dz)
		{
			this->dx.inject(_dx);
			this->dy.inject(_dy);
			this->dz.inject(_dz);
		}

		void GradientField::clear()
		{
			memset(_dx, 0, _count);
			memset(_dy, 0, _count);
			memset(_dz, 0, _count);
		}


		GradientField::ComponentAccessor::ComponentAccessor() : _channel (NULL)
		{

		}

		void GradientField::ComponentAccessor::inject( signed char * pChannel )
		{
			OgreAssert(_channel == NULL, "Already dependency injected");
			const_cast< signed char *& > (_channel) = pChannel;
		}

	}
}
