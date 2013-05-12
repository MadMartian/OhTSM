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
		: heightmap(NULL), _pColourMaps(NULL), _pMaterials(NULL),
		countTilesPerPageSide(options.getTilesPerPage()), 
		countVerticesPerPageSide(options.pageSize), 
		countVerticesPerTileSide(options.tileSize),
		countVerticesPerPage(options.getTotalPageSize()),
		pageX(pageX), pageY(pageY)
	{
		const size_t nTileCount = options.getTilesPerPage() * options.getTilesPerPage();

		heightmap = new Real[countVerticesPerPage];
		memset(heightmap, 0, countVerticesPerPage * sizeof(Real));
		if (options.coloured)
		{
			const size_t nVertexCount = countVerticesPerTileSide * countVerticesPerTileSide;
			_pColourMaps = new std::vector< ColourValue * > (nTileCount);
			for (std::vector< ColourValue * >::iterator i = _pColourMaps->begin(); i != _pColourMaps->end(); ++i)
			{
				*i = new ColourValue[nVertexCount];
				memset(*i, 0, nVertexCount * sizeof(ColourValue));
			}
		}
		if (options.materialPerTile)
		{
			// TODO: Threading - Take care and below
			_pMaterials = new std::vector< MaterialPtr > (nTileCount);
		}
	}

	PageInitParams::~PageInitParams()
	{
		delete [] heightmap;
		for (std::vector< ColourValue * >::iterator i = _pColourMaps->begin(); i != _pColourMaps->end(); ++i)
			delete [] *i;
		delete _pColourMaps;
		delete _pMaterials;
	}

	void PageInitParams::operator << ( StreamSerialiser & ins )
	{
		ins.read(heightmap, countVerticesPerPage);
		if (_pColourMaps != NULL)
		{
			const size_t nVertexCount = countVerticesPerTileSide * countVerticesPerTileSide;
			for (std::vector< ColourValue * >::iterator i = _pColourMaps->begin(); i != _pColourMaps->end(); ++i)
				ins.read(*i, nVertexCount);
		}
	}

	void PageInitParams::operator>>( StreamSerialiser & outs ) const
	{
		outs.write(heightmap, countVerticesPerPage);
		if (_pColourMaps != NULL)
		{
			const size_t nVertexCount = countVerticesPerTileSide * countVerticesPerTileSide;
			for (std::vector< ColourValue * >::iterator i = _pColourMaps->begin(); i != _pColourMaps->end(); ++i)
				outs.write(*i, nVertexCount);
		}
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
		params.heightmap = heightmap;
		params.vx0 = i * (countVerticesPerTileSide - 1);
		params.vy0 = j * (countVerticesPerTileSide - 1);
		params.colourmap = isColourMap() ? const_cast< ColourValue * > (colourmap(i, j)) : NULL;
		params.material = isMaterials() ? const_cast< MaterialPtr & > (material(i, j)) : MaterialPtr();
	}

}
