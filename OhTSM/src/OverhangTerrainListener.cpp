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

#include "OverhangTerrainListener.h"

#include "CubeDataRegion.h"
#include "MetaWorldFragment.h"

#include "DebugTools.h"

namespace Ogre
{
	OverhangTerrainMetaCube::OverhangTerrainMetaCube( 
		Voxel::CubeDataRegion * pCubeDataRegion,
		MetaFragment::Interfaces::Unique * pInterfUnique, 

		const int vx0, const int vy0, const int vz0,
		const int vxN, const int vyN, const int vzN
	) 
		:	OverhangTerrainSupportsCustomData(pInterfUnique->custom), 
			_pDataGrid(pCubeDataRegion), 
			_pDataGridAccess(pCubeDataRegion->lease_p()), 
			
			vx0(vx0), vy0(vy0), vz0(vz0), 
			vxN(vxN), vyN(vyN), vzN(vzN)
	{}
	
	OverhangTerrainMetaCube::~OverhangTerrainMetaCube()
	{
		delete _pDataGridAccess;
	}

	bool OverhangTerrainMetaCube::hasColours() const { return _pDataGrid->hasColours();	}
	bool OverhangTerrainMetaCube::hasGradient() const { return _pDataGrid->hasGradient(); }
	size_t OverhangTerrainMetaCube::getDimensions() const { return _pDataGrid->getDimensions(); }

	Voxel::ColourChannelSet & OverhangTerrainMetaCube::getColours() { return _pDataGridAccess->colours; }
	const Voxel::ColourChannelSet & OverhangTerrainMetaCube::getColours() const { return _pDataGridAccess->colours; }
	Vector3 OverhangTerrainMetaCube::getCubeSize() const { return _pDataGrid->getBoxSize().getSize(); }

	OverhangTerrainRenderable::OverhangTerrainRenderable( MetaFragment::Interfaces::Builder * pInterfBuilder ) 
		: OverhangTerrainSupportsCustomData(pInterfBuilder->custom), _pBuilder(pInterfBuilder)
	{}

	void OverhangTerrainRenderable::setMaterial( MaterialPtr pMat ) 
	{ 
		oht_assert_threadmodel(ThrMdl_Main);
		_pBuilder->setMaterial(pMat); 
	}


	OverhangTerrainSupportsCustomData::OverhangTerrainSupportsCustomData(ISerializeCustomData *& pCustom) 
	: custom(pCustom)
	{}

}
