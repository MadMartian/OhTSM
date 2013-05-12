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

#include "MetaFactory.h"
#include "MetaHeightmap.h"
#include "MetaBall.h"
#include "MetaWorldFragment.h"
#include "DebugTools.h"
#include "IsoSurfaceBuilder.h"
#include "CubeDataRegionDescriptor.h"

namespace Ogre
{
	using namespace Voxel;

	MetaFactory::MetaFactory( 
		const OverhangTerrainOptions & opts, 
		ManualResourceLoader * pManRsrcLoader, 
		const Config & config
	) 
		:	_pCubeMeta(new CubeDataRegionDescriptor(opts.tileSize, opts.cellScale, config.dataGridFlags | (opts.coloured ? CubeDataRegionDescriptor::HAS_COLOURS : 0))), 
			_options(opts), 
			_pManRsrcLoader(pManRsrcLoader)
	{
		_pISB = new IsoSurfaceBuilder(
			*_pCubeMeta, 
			opts.maxGeoMipMapLevel, 
			opts.maxPixelError, 
			config.transitionCellWidthRatio, 
			config.flipNormals,
			config.normals
		);
	}

	MetaFactory::~MetaFactory()
	{
		delete _pISB;
		delete _pCubeMeta;
	}

	MetaBall * MetaFactory::createMetaBall( const Vector3 & position /*= Vector3::ZERO*/, const Real radius /*= 0.0*/, const bool excavating /*= true*/ ) const
	{
		//OHT_DBGTRACE("pos=" << position << ", radius=" << radius << ", excavating=" << excavating);
		return new MetaBall(position, radius, excavating);
	}

	MetaFragment::Container * MetaFactory::createMetaFragment( const Vector3 & pt /*= Vector3::ZERO*/, const YLevel yl /*= YLevel()*/ ) const
	{
		//OHT_DBGTRACE("pos=" << pt << ", y-level=" << yl);
		// TODO: Inconsistent positioning, data-grid is centered whilst other crap elsewhere is not
		return new MetaFragment::Container(this, createDataGrid(pt - _pCubeMeta->getBoxSize().getMinimum()), yl);
	}

	CubeDataRegion * MetaFactory::createDataGrid( const Vector3 & pos /*= Vector3::ZERO*/ ) const
	{
		//OHT_DBGTRACE("pos=" << pos);
		return new CubeDataRegion(*_pCubeMeta, pos);
	}

	Ogre::MaterialPtr MetaFactory::acquireMaterial( const std::string & sName, const std::string & sRsrcGroup ) const
	{
		std::pair< MaterialPtr, bool > result = MaterialManager::getSingleton().createOrRetrieve(sName, sRsrcGroup);

		if (result.second)
			_pManRsrcLoader->loadResource(result.first.getPointer());

		return result.first;
	}

	IsoSurfaceRenderable * MetaFactory::createIsoSurfaceRenderable( MetaFragment::Container * const pMWF, const String & sName ) const
	{
		return _pISB->createIsoSurface(pMWF, sName);
	}

}
