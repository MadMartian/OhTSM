/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2006 Torus Knot Software Ltd
Also see acknowledgments in Readme.html

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

Adapted by Jonathan Neufeld (http://www.extollit.com) 2011-2013 for OverhangTerrain Scene Manager
-----------------------------------------------------------------------------
*/
#include "pch.h"

#include "OverhangTerrainPageInitParams.h"

namespace Ogre
{
	PageInitParams::PageInitParams( const OverhangTerrainOptions & options, const int16 pageX, const int16 pageY ) 
		: heightmap(NULL),
		_options(options),
		countTilesPerPageSide(options.getTilesPerPage()), 
		countVerticesPerPageSide(options.pageSize), 
		countVerticesPerTileSide(options.tileSize),
		countVerticesPerPage(options.getTotalPageSize()),
		pageX(pageX), pageY(pageY),
		_channels(options.channels.descriptor, ChannelParamsFactory(options))
	{
		const size_t nTileCount = options.getTilesPerPage() * options.getTilesPerPage();

		heightmap = new Real[countVerticesPerPage];
		memset(heightmap, 0, countVerticesPerPage * sizeof(Real));
	}

	PageInitParams::~PageInitParams()
	{
		delete [] heightmap;
	}

	void PageInitParams::operator << ( StreamSerialiser & ins )
	{
		ins.read(heightmap, countVerticesPerPage);
		Channel::Ident channel;

		while (Channel::Ident_INVALID != ([&ins, &channel] () -> const Channel::Ident { ins >> channel; return channel; } ()))
			ins >> _channels[channel];
	}

	void PageInitParams::operator>>( StreamSerialiser & outs ) const
	{
		outs.write(heightmap, countVerticesPerPage);
		for (ChannelIndex::const_iterator i = _channels.begin(); i != _channels.end(); ++i)
		{
			outs << i->channel;
			outs << *i->value;
		}
		outs << Channel::Ident_INVALID;
	}

	const PageInitParams::TileParams PageInitParams::getTile( const size_t i, const size_t j ) const
	{
		TileParams params;
		populateTileParams(params, i, j);
		return params;
	}

	PageInitParams::TileParams PageInitParams::getTile( const size_t i, const size_t j )
	{
		TileParams params;
		populateTileParams(params, i, j);
		return params;
	}

	void PageInitParams::populateTileParams( TileParams & params, const size_t i, const size_t j ) const
	{
		params.vx0 = i * (countVerticesPerTileSide - 1);
		params.vy0 = j * (countVerticesPerTileSide - 1);
	}

	std::vector< ColourValue * > * PageInitParams::ChannelParams::createColourMap( const size_t nTilesPerPageSide, const size_t nVertsPerTileSide, const OverhangTerrainOptions::ChannelOptions & chanopts )
	{
		const size_t nVertexCount = nVertsPerTileSide * nVertsPerTileSide;
		std::vector< ColourValue * > * pColourMaps = new std::vector< ColourValue * > (nTilesPerPageSide*nTilesPerPageSide);
		for (std::vector< ColourValue * >::iterator i = pColourMaps->begin(); i != pColourMaps->end(); ++i)
		{
			*i = new ColourValue[nVertexCount];
			memset(*i, 0, nVertexCount * sizeof(ColourValue));
		}
		return pColourMaps;
	}

	PageInitParams::ChannelParams::ChannelParams( const size_t nTilesPerPageSide, const size_t nVertsPerTileSide, const OverhangTerrainOptions::ChannelOptions & chanopts )
		:	_nTilesPerPageSide(nTilesPerPageSide), _nVertsPerTileSide(nVertsPerTileSide),
			_pColourMaps( chanopts.voxelRegionFlags & VRF_Colours ? createColourMap(nTilesPerPageSide, nVertsPerTileSide, chanopts) : NULL ),
			_pMaterials(chanopts.materialPerTile ? new std::vector< MaterialPtr > (nTilesPerPageSide*nTilesPerPageSide) : NULL)
	{

	}

	PageInitParams::ChannelParams::~ChannelParams()
	{
		for (std::vector< ColourValue * >::iterator i = _pColourMaps->begin(); i != _pColourMaps->end(); ++i)
			delete [] *i;
		delete _pColourMaps;
		delete _pMaterials;
	}

	StreamSerialiser & operator>>( StreamSerialiser & ins, PageInitParams::ChannelParams & params )
	{
		if (params._pColourMaps != NULL)
		{
			const size_t nVertexCount = params._nVertsPerTileSide * params._nVertsPerTileSide;
			for (std::vector< ColourValue * >::iterator i = params._pColourMaps->begin(); i != params._pColourMaps->end(); ++i)
				ins.read(*i, nVertexCount);
		}
		return ins;
	}

	StreamSerialiser & operator<<( StreamSerialiser & outs, const PageInitParams::ChannelParams & params )
	{
		if (params._pColourMaps != NULL)
		{
			const size_t nVertexCount = params._nVertsPerTileSide * params._nVertsPerTileSide;
			for (std::vector< ColourValue * >::iterator i = params._pColourMaps->begin(); i != params._pColourMaps->end(); ++i)
				outs.write(*i, nVertexCount);
		}
		return outs;
	}

}
