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
#include "IsoSurfaceRenderable.h"
#include "CubeDataRegionDescriptor.h"

namespace Ogre
{
	namespace Voxel
	{
		MetaVoxelFactory::MetaVoxelFactory(
			MetaBaseFactory * pBase,
			const Channel::Ident channel,
			const OverhangTerrainOptions & options
		)
		:	base(pBase),
			channel(channel),
			_options(options),
			_chanopts(options.channels[channel]),
			pool(pBase->createDataBasePool(options.channels[channel].voxelRegionFlags)),
			surfaceFlags(IsoSurfaceBuilder::genSurfaceFlags(options.channels[channel]))
		{
			HardwareBufferManager * pBuffMan = HardwareBufferManager::getSingletonPtr();
			_pVtxDecl = pBuffMan->createVertexDeclaration();

			size_t offset = 0;

			const VertexElement * pPos, * pNorm, * pDiffuse, * pTexC;
			pPos = pNorm = pDiffuse = pTexC = NULL;

			pPos = &_pVtxDecl->addElement(0, offset, VET_FLOAT3, VES_POSITION);
			offset += VertexElement::getTypeSize(VET_FLOAT3);

			if (surfaceFlags & IsoVertexElements::GEN_NORMALS)
			{
				pNorm = &_pVtxDecl->addElement(0, offset, VET_FLOAT3, VES_NORMAL);
				offset += VertexElement::getTypeSize(VET_FLOAT3);
			}
			if (surfaceFlags & IsoVertexElements::GEN_VERTEX_COLOURS)
			{
				pDiffuse = &_pVtxDecl->addElement(0, offset, VET_COLOUR, VES_DIFFUSE);
				offset += VertexElement::getTypeSize(VET_COLOUR);
			}
			if (surfaceFlags & IsoVertexElements::GEN_TEX_COORDS)
			{
				pTexC = &_pVtxDecl->addElement(0, offset, VET_FLOAT2, VES_TEXTURE_COORDINATES);
				offset += VertexElement::getTypeSize(VET_FLOAT2);
			}

			_pVtxDeclElems = new VertexDeclarationElements(pPos, pNorm, pDiffuse, pTexC);

		}

		MetaVoxelFactory::~MetaVoxelFactory()
		{
			delete _pVtxDeclElems;

			HardwareBufferManager * pBuffMan = HardwareBufferManager::getSingletonPtr();
			pBuffMan->destroyVertexDeclaration(_pVtxDecl);
		}

		CubeDataRegion * MetaVoxelFactory::createDataGrid( const AxisAlignedBox & bbox ) const
		{
			//OHT_DBGTRACE("pos=" << pos);
			return base->createCubeDataRegion(_chanopts.voxelRegionFlags, pool, bbox);
		}

		MetaFragment::Container * MetaVoxelFactory::createMetaFragment( const AxisAlignedBox & bbox /* = AxisAlignedBox::BOX_NULL*/, const YLevel yl /*= YLevel()*/ ) const
		{
			//OHT_DBGTRACE("pos=" << pt << ", y-level=" << yl);
			return new MetaFragment::Container(this, createDataGrid(bbox), yl);
		}

		IsoSurfaceRenderable * MetaVoxelFactory::createIsoSurfaceRenderable( MetaFragment::Container * const pMWF, const String & sName ) const
		{
			oht_assert_threadmodel(ThrMdl_Single);
			IsoSurfaceRenderable * pISR = new IsoSurfaceRenderable(_pVtxDecl, pMWF, _chanopts.maxGeoMipMapLevel, _chanopts.maxPixelError, sName);
			pISR->setCastShadows(true);
			return pISR;
		}

	}
	const Voxel::MetaVoxelFactory * MetaBaseFactory::getVoxelFactory(const Channel::Ident channel) const
	{
		return &(*_pVoxelFacts)[channel];
	}
	Voxel::MetaVoxelFactory * MetaBaseFactory::getVoxelFactory(const Channel::Ident channel)
	{
		return &(*_pVoxelFacts)[channel];
	}

	MetaBaseFactory::MetaBaseFactory(
		const OverhangTerrainOptions & opts, 
		ManualResourceLoader * pManRsrcLoader
	) 
		:	_pCubeMeta(new Voxel::CubeDataRegionDescriptor(opts.tileSize, opts.cellScale)),
			_options(opts), 
			_pManRsrcLoader(pManRsrcLoader),
			_pVoxelFacts(NULL)
	{
		_pISB = new IsoSurfaceBuilder(
			*_pCubeMeta, 
			Channel::Index< IsoSurfaceBuilder::ChannelParameters >(
				opts.channels.descriptor,
				[opts] (const Channel::Ident channel) -> IsoSurfaceBuilder::ChannelParameters *
				{
					const OverhangTerrainOptions::ChannelOptions & chanopts = opts.channels[channel];
					return new IsoSurfaceBuilder::ChannelParameters(
						chanopts.transitionCellWidthRatio,
						IsoSurfaceBuilder::genSurfaceFlags(chanopts),
						chanopts.maxGeoMipMapLevel,
						chanopts.maxPixelError,
						chanopts.flipNormals,
						chanopts.normals
					);
				}
			)
		);

		MetaBaseFactory * self = this;

		_pVoxelFacts = new Channel::Index< Voxel::MetaVoxelFactory, Channel::FauxFactory< Voxel::MetaVoxelFactory > > (
			opts.channels.descriptor,
			[self, opts] (const Channel::Ident channel) -> Voxel::MetaVoxelFactory *
			{
				return new Voxel::MetaVoxelFactory(self, channel, opts);
			}
		);
	}

	MetaBaseFactory::~MetaBaseFactory()
	{
		delete _pISB;
		delete _pCubeMeta;
		delete _pVoxelFacts;
	}

	MetaBall * MetaBaseFactory::createMetaBall( const Vector3 & position /*= Vector3::ZERO*/, const Real radius /*= 0.0*/, const bool excavating /*= true*/ ) const
	{
		//OHT_DBGTRACE("pos=" << position << ", radius=" << radius << ", excavating=" << excavating);
		return new MetaBall(position, radius, excavating);
	}

	Ogre::MaterialPtr MetaBaseFactory::acquireMaterial( const std::string & sName, const std::string & sRsrcGroup ) const
	{
		std::pair< MaterialPtr, bool > result = MaterialManager::getSingleton().createOrRetrieve(sName, sRsrcGroup);

		if (result.second)
			_pManRsrcLoader->loadResource(result.first.getPointer());

		return result.first;
	}

	Voxel::DataBasePool * MetaBaseFactory::createDataBasePool( const size_t nVRFlags )
	{
		return new Voxel::DataBasePool(_pCubeMeta->gpcount, nVRFlags);
	}

	Voxel::CubeDataRegion * MetaBaseFactory::createCubeDataRegion (const size_t nVRFlags, Voxel::DataBasePool * pPool, const AxisAlignedBox & bbox /*= AxisAlignedBox::BOX_NULL */)
	{
		return new Voxel::CubeDataRegion(nVRFlags, pPool, *_pCubeMeta, bbox);
	}

}
