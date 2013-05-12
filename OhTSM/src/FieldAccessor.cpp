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

#include "FieldAccessor.h"

namespace Ogre
{
	namespace Voxel
	{
		FieldAccessor::FieldAccessor( const CubeDataRegionDescriptor & dgtmpl, FieldStrength * pValues ) 
			: _cubemeta(dgtmpl), values(pValues), min(-1), max((int)dgtmpl.dimensions +1), _mxy(dgtmpl.dimensions + 1)
		{
			accessors[0] = values;
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
			{
				accessors[s + 1] =
					stripes[s] = new FieldStrength[_cubemeta.sidegpcount];
			}
			accessors[CountOrthogonalNeighbors + 2 - 1] = &dummy;
		}

		FieldAccessor::FieldAccessor( const FieldAccessor & copy )
			: _cubemeta(copy._cubemeta), values(copy.values), _mxy(copy._mxy), min(copy.min), max(copy.max)
		{
			accessors[0] = values;
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
			{
				memcpy(
					accessors[s + 1] = 
					stripes[s] = new FieldStrength[_cubemeta.sidegpcount], 

					copy.stripes[s], 
					_cubemeta.sidegpcount * sizeof(FieldStrength)
					);
			}
			accessors[CountOrthogonalNeighbors + 2 - 1] = &dummy;
		}

		FieldAccessor::FieldAccessor( FieldAccessor && move ) 
			: _cubemeta(move._cubemeta), values(move.values), _mxy(move._mxy), min(move.min), max(move.max)
		{
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
			{
				stripes[s] = move.stripes[s];
				move.stripes[s] = NULL;
			}
			for (unsigned s = 0; s < 1+CountOrthogonalNeighbors+1; ++s)
				accessors[s] = move.accessors[s];
			accessors[CountOrthogonalNeighbors + 2 - 1] = &dummy;
		}

		FieldAccessor::~FieldAccessor()
		{
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
				delete [] stripes[s];
		}

		FieldStrength & FieldAccessor::operator()( const int x, const int y, const int z )
		{
			OgreAssert(x >= min && x <= max && y >= min && y <= max && z >= min && z <= max, "Coordinates out of bounds");

			const Touch3DSide t3ds = 
				getTouch3DSide(
					getTouchStatus(x, min, max),
					getTouchStatus(y, min, max),
					getTouchStatus(z, min, max)
				);

			const Moore3DNeighbor m3n = getMoore3DNeighbor(t3ds);
			const signed int 
				zero = bitmanip::testZero((signed int)t3ds),
				nzero = ~zero;
			const CubeSideCoords csc = CubeSideCoords::from3D(m3n, x, y, z);

			const int nAccessorIdx = nzero & bitmanip::minimum((int)m3n + 1, (int)CountOrthogonalNeighbors + 2 - 1);
			FieldStrength * pField = accessors[nAccessorIdx];
			size_t nIndex = 
				(zero & _cubemeta.getGridPointIndex(x & zero, y & zero, z & zero)) 
				| 
				(nzero & (csc.x*1 + csc.y*_mxy) & ~bitmanip::testZero(nAccessorIdx - int(CountOrthogonalNeighbors + 2 - 1)));

			OgreAssert(nAccessorIdx == 0 || nIndex < _cubemeta.sidegpcount, "Index out of range for slices");

			return pField[nIndex];
		}

		void FieldAccessor::clear()
		{
			memset(values, 0, _cubemeta.gpcount * sizeof(FieldStrength));
			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
				memset(stripes[s], 0, _cubemeta.sidegpcount * sizeof(FieldStrength));
		}

		Ogre::Voxel::FieldAccessor::iterator FieldAccessor::iterate
		(
			const signed short x0, const signed short y0, const signed short z0,
			const signed short xN, const signed short yN, const signed short zN
		)
		{
			return iterator(_cubemeta, Coords(x0, y0, z0), Coords(xN, yN, zN), stripes, values);
		}

		FieldAccessor::iterator::iterator( const CubeDataRegionDescriptor & dgtmpl, const Coords & c0, const Coords & cN, FieldStrength ** vvStripes, FieldStrength * vValues ) 
			:	_dgtmpl(dgtmpl), _c0(c0), _cN(cN), _dim(dgtmpl.dimensions + 1),
				_t3df(
					getTouch3DSide(
						TouchStatus(getTouchStatus(c0.i, -1, dgtmpl.dimensions+1) | getTouchStatus(cN.i, -1, dgtmpl.dimensions+1)),
						TouchStatus(getTouchStatus(c0.j, -1, dgtmpl.dimensions+1) | getTouchStatus(cN.j, -1, dgtmpl.dimensions+1)),
						TouchStatus(getTouchStatus(c0.k, -1, dgtmpl.dimensions+1) | getTouchStatus(cN.k, -1, dgtmpl.dimensions+1))
					)
				), 
				_stripes(vvStripes), _values(vValues), _term(false)
		{
			OHT_CR_INIT();
			process();
		}

		FieldAccessor::iterator::iterator( const iterator & copy ) 
			:	OHT_CR_COPY(copy), _dgtmpl(copy._dgtmpl), 
				_c0(copy._c0), _cN(copy._cN), _dim(copy._dim),
				_t3df(copy._t3df),
				_stripes(copy._stripes), _values(copy._values), 
				_curr(copy._curr), _stripe(copy._stripe), _index(copy._index),
				_stripestuff(copy._stripestuff), _blockstuff(copy._blockstuff),
				_coords(copy._coords),
				_term(copy._term)
		{

		}

		void FieldAccessor::iterator::initStripeWalk()
		{
			using bitmanip::clamp;

			const Simplex2xSimplex3 & simp2to3 = Simplex2D3D[_stripe];
			_stripestuff.s0 = simp2to3.simplex[0];
			_stripestuff.s1 = simp2to3.simplex[1];
			_stripestuff.x0 = clamp((short)0, _dim, _c0[_stripestuff.s0]);
			_stripestuff.y0 = clamp((short)0, _dim, _c0[_stripestuff.s1]);
			_stripestuff.xN = clamp((short)-1, (short)_dgtmpl.dimensions, _cN[_stripestuff.s0]);
			_stripestuff.yN = clamp((short)-1, (short)_dgtmpl.dimensions, _cN[_stripestuff.s1]);

			_stripestuff.advanceY = _dim - (_stripestuff.xN - _stripestuff.x0 + 1);

			_index = _stripestuff.y0 * _dim + _stripestuff.x0;

			_coords.i = ((Mat2D3D[_stripe].x.d) & _dim) | ((~Mat2D3D[_stripe].x.d) & _c0.i);
			_coords.j = ((Mat2D3D[_stripe].y.d) & _dim) | ((~Mat2D3D[_stripe].y.d) & _c0.j);
			_coords.k = ((Mat2D3D[_stripe].z.d) & _dim) | ((~Mat2D3D[_stripe].z.d) & _c0.k);
		}

		void FieldAccessor::iterator::initBlockWalk()
		{
			using bitmanip::clamp;

			_blockstuff.x0 = clamp((short)0, _dim, _c0.i);
			_blockstuff.y0 = clamp((short)0, _dim, _c0.j);
			_blockstuff.z0 = clamp((short)0, _dim, _c0.k);

			_blockstuff.xN = clamp((short)-1, (short)_dgtmpl.dimensions, _cN.i);
			_blockstuff.yN = clamp((short)-1, (short)_dgtmpl.dimensions, _cN.j);
			_blockstuff.zN = clamp((short)-1, (short)_dgtmpl.dimensions, _cN.k);

			_index = _blockstuff.z0 * _dim*_dim + _blockstuff.y0 * _dim + _blockstuff.x0;

			const signed short scan = _blockstuff.xN - _blockstuff.x0 + 1;
			_blockstuff.advanceY = _dim - scan;
			_blockstuff.advanceZ = _dim*_dim - _dim*(_blockstuff.yN - _blockstuff.y0 + 1);
		}

		void FieldAccessor::iterator::process()
		{
			OHT_CR_START();

			for (_stripe = 0; _stripe < CountOrthogonalNeighbors; ++_stripe)
			{
				if (OrthogonalNeighbor_to_Touch3DSide[_stripe] & _t3df)
				{
					initStripeWalk();

					for (
						_coords[_stripestuff.s1] = _stripestuff.y0; 
						_coords[_stripestuff.s1] <= _stripestuff.yN; 
							++_coords[_stripestuff.s1], 
							_index += _stripestuff.advanceY
						)
					{
						for (
							_coords[_stripestuff.s0] = _stripestuff.x0; 
							_coords[_stripestuff.s0] <= _stripestuff.xN; 
							++_coords[_stripestuff.s0]
						)
						{
							OgreAssert(_index < _dgtmpl.sidegpcount, "Stripe-Buffer overrun");
							_curr = &_stripes[_stripe][_index++];
							OHT_CR_RETURN_VOID(CRS_Stripe);
						}
					}
				}
			}

			initBlockWalk();

			for (
				_coords[2] = _blockstuff.z0; 
				_coords[2] <= _blockstuff.zN; 
					++_coords[2], 
					_index += _blockstuff.advanceZ
				)
			{
				for (
					_coords[1] = _blockstuff.y0; 
					_coords[1] <= _blockstuff.yN; 
						++_coords[1], 
						_index += _blockstuff.advanceY
					)
				{
					for (
						_coords[0] = _blockstuff.x0; 
						_coords[0] <= _blockstuff.xN; 
						++_coords[0]
					)
					{
						OgreAssert(_index < _dgtmpl.gpcount, "Voxel-Buffer overrun");
						_curr = &_values[_index++];
						OHT_CR_RETURN_VOID(CRS_Values);
					}
				}
			}

			OHT_CR_END();

			_term = true;
		}

	}
}
