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

#include "MetaHeightMap.h"
#include "CubeDataRegion.h"
#include "IsoSurfaceSharedTypes.h"

#include "MOUtil.h"

namespace Ogre
{
	using namespace Voxel;

	MetaHeightMap::MetaHeightMap()
	: _vHeightmap(NULL), _hscale(0), _vscale(0), _width(0), _depth(0)
	{
	}

	MetaHeightMap::~MetaHeightMap()
	{
		delete [] _vHeightmap;
	}

	class MyUpdateFunctor : public FieldStrengthFunctorBase< MyUpdateFunctor, MetaHeightMap >
	{
	private:
		size_t _x0, _y0;

	public:
		MyUpdateFunctor (const MetaHeightMap * self, const CubeDataRegion * pDG)
			: FieldStrengthFunctorBase(self, pDG) 
		{
			Vector3 o = pDG->getBoundingBox().getMinimum();
			o -= self->getAABB().getMinimum();
			o /= pDG->getGridScale();

			_x0 = size_t(o.x);
			_y0 = size_t(o.z);
		}

		inline
			Real getFieldStrength (const signed int x, const signed int y, const signed int z) const
		{
			return (dg->getBoundingBox().getMinimum().y + (Real)y * dg->getGridScale() - self->height(x + _x0, z + _y0)) / dg->getGridScale();
		}
	};

	/// Adds this meta heightmap to the access grid.
	void MetaHeightMap::updateDataGrid(const CubeDataRegion * pDG, DataAccessor * pAccess)
	{
		const size_t nDN = pDG->getDimensions()+1;
		Ogre::updateDataGrid (pDG, *pAccess, -1, -1, -1, nDN, nDN, nDN, MyUpdateFunctor(this, pDG));
	}

	Ogre::AxisAlignedBox MetaHeightMap::getAABB() const
	{
		return _bbox;
	}

	void MetaHeightMap::write( StreamSerialiser & output ) const
	{
		MetaObject::write(output);
	}

	void MetaHeightMap::read( StreamSerialiser & input )
	{
		MetaObject::read(input);
	}

	void MetaHeightMap::unyion(const size_t x0, const size_t z0, const size_t xN, const size_t zN, Real & min, Real & max) const
	{
		for ( size_t j = z0; j < zN; j++ )
		{
			for ( size_t i = x0; i < xN; i++ )
			{
				const Real height = _vHeightmap[j * _width + i] * _vscale;

				if ( height < min )
					min = height;

				if ( height > max )
					max = height;
			}
		}
	}

	void MetaHeightMap::intersection( AxisAlignedBox & bbox ) const
	{
		//calculate min and max heights;
		Real 
			min = bbox.getMinimum().y,
			max = bbox.getMaximum().y;

		const Real
			b0 = min,
			bN = max;

		const size_t 
			x0 = size_t(bbox.getMinimum().x / _hscale), 
			z0 = size_t(bbox.getMinimum().z / _hscale), 
			xN = size_t(bbox.getMaximum().x / _hscale), 
			zN = size_t(bbox.getMaximum().z / _hscale);

		for ( size_t j = z0; j < zN; j++ )
		{
			for ( size_t i = x0; i < xN; i++ )
			{
				const Real height = _vHeightmap[j * _width + i] * _vscale;

				if ( height > b0 )
					min = height;

				if ( height < bN )
					max = height;
			}
		}

		if (min > max)
			bbox.setInfinite();
		else
		{
			bbox.setMinimumY(min);
			bbox.setMaximumY(max);
		}
	}

	void MetaHeightMap::load( Real * pHM, const size_t width, const size_t depth, const Real hscale, const Real vscale )
	{
		delete [] _vHeightmap;
		const size_t nCount = width*depth;
		_width = width;
		_depth = depth;
		_hscale = hscale;
		_vscale = vscale;
		_vHeightmap = new Real[nCount];
		memcpy(_vHeightmap, pHM, nCount * sizeof(*_vHeightmap));

		_w1 = _width - 1,
		_d1 = _depth - 1;

		Real min, max;
		span(0, 0, _w1, _d1, min, max);

		_bbox.setExtents(
			_pos.x - (Real)_w1 * _hscale / 2.0f,
			min,
			_pos.x - (Real)_d1 * _hscale / 2.0f,

			_pos.x + (Real)_w1 * _hscale / 2.0f,
			max,
			_pos.x + (Real)_d1 * _hscale / 2.0f
		);
	}

	void MetaHeightMap::span( const size_t x0, const size_t z0, const size_t xN, const size_t zN, Real & min, Real & max ) const
	{
		min = std::numeric_limits< Real >::max();
		max = -min;
		unyion(x0, z0, xN, zN, min, max);
	}

}/// namespace Ogre
