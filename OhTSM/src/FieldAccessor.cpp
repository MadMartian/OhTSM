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

		Ogre::Voxel::FieldAccessor::gradient_iterator FieldAccessor::iterate_gradient(
			const unsigned component, 
			const signed short x0, const signed short y0, const signed short z0, 
			const signed short xN, const signed short yN, const signed short zN 
		)
		{
			return gradient_iterator(component, _cubemeta, Coords(x0, y0, z0), Coords(xN, yN, zN), stripes, values);
		}

		FieldAccessor::iterator::iterator( const CubeDataRegionDescriptor & dgtmpl, const Coords & c0, const Coords & cN, FieldStrength ** vvStripes, FieldStrength * vValues ) 
			:	_dgtmpl(dgtmpl), _c0(c0), _cN(cN), _dim1(dgtmpl.dimensions + 1),
				_t3df(
					getTouch3DSide(
						TouchStatus(getTouchStatus(c0.i, -1, dgtmpl.dimensions+1) | getTouchStatus(cN.i, -1, dgtmpl.dimensions+1)),
						TouchStatus(getTouchStatus(c0.j, -1, dgtmpl.dimensions+1) | getTouchStatus(cN.j, -1, dgtmpl.dimensions+1)),
						TouchStatus(getTouchStatus(c0.k, -1, dgtmpl.dimensions+1) | getTouchStatus(cN.k, -1, dgtmpl.dimensions+1))
					)
				), 
				_stripestuff(dgtmpl), _blockstuff(dgtmpl),
				_stripes(vvStripes), _values(vValues), _term(false)
		{
			OHT_CR_INIT();
			process();
		}

		FieldAccessor::iterator::iterator( const iterator & copy ) 
			:	OHT_CR_COPY(copy), _dgtmpl(copy._dgtmpl), 
				_c0(copy._c0), _cN(copy._cN), _dim1(copy._dim1),
				_t3df(copy._t3df),
				_stripes(copy._stripes), _values(copy._values), 
				_curr(copy._curr), _stripe(copy._stripe), _index(copy._index),
				_stripestuff(copy._stripestuff), _blockstuff(copy._blockstuff),
				_coords(copy._coords),
				_term(copy._term)
		{

		}

		void FieldAccessor::iterator::process()
		{
			OHT_CR_START();

			for (_stripe = 0; _stripe < CountOrthogonalNeighbors; ++_stripe)
			{
				if (OrthogonalNeighbor_to_Touch3DSide[_stripe] & _t3df)
				{
					_stripestuff.init(_stripe, _c0, _cN, _coords);

					for (
						_index = _stripestuff.index,
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

			_blockstuff.init(_c0, _cN);

			for (
				_index = _blockstuff.index,
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


		FieldAccessor::StripeLogic::StripeLogic( const CubeDataRegionDescriptor & dgtmpl ) 
			: _dim(dgtmpl.dimensions), _dim1(dgtmpl.dimensions + 1)
		{}


		void FieldAccessor::StripeLogic::init(const unsigned stripe, const Coords & c0, const Coords & cN, Coords & start)
		{
			using bitmanip::clamp;

			const Simplex2xSimplex3 & simp2to3 = Simplex2D3D[stripe];
			s0 = simp2to3.simplex[0];
			s1 = simp2to3.simplex[1];
			x0 = clamp((short)0, _dim1, c0[s0]);
			y0 = clamp((short)0, _dim1, c0[s1]);
			xN = clamp((short)-1, (short)_dim, cN[s0]);
			yN = clamp((short)-1, (short)_dim, cN[s1]);

			advanceY = _dim1 - (xN - x0 + 1);

			index = y0 * _dim1 + x0;

			start.i = ((Mat2D3D[stripe].x.d) & _dim1) | ((~Mat2D3D[stripe].x.d) & c0.i);
			start.j = ((Mat2D3D[stripe].y.d) & _dim1) | ((~Mat2D3D[stripe].y.d) & c0.j);
			start.k = ((Mat2D3D[stripe].z.d) & _dim1) | ((~Mat2D3D[stripe].z.d) & c0.k);
		}

		FieldAccessor::BlockLogic::BlockLogic( const CubeDataRegionDescriptor & dgtmpl ) 
			: _dim(dgtmpl.dimensions), _dim1(dgtmpl.dimensions + 1)
		{}

		void FieldAccessor::BlockLogic::init( const Coords & c0, const Coords & cN )
		{
			clamp(c0, cN);

			index = z0 * _dim1*_dim1 + y0 * _dim1 + x0;

			const signed short scan = xN - x0 + 1;
			advanceY = _dim1 - scan;
			advanceZ = _dim1*_dim1 - _dim1*(yN - y0 + 1);
		}

		void FieldAccessor::BlockLogic::clamp( const Coords &c0, const Coords &cN )
		{
			using bitmanip::clamp;

			x0 = clamp((short)0, _dim1, c0.i);
			y0 = clamp((short)0, _dim1, c0.j);
			z0 = clamp((short)0, _dim1, c0.k);

			xN = clamp((short)-1, (short)_dim, cN.i);
			yN = clamp((short)-1, (short)_dim, cN.j);
			zN = clamp((short)-1, (short)_dim, cN.k);
		}

		FieldAccessor::BlockLogicFeathered::BlockLogicFeathered( const CubeDataRegionDescriptor & dgtmpl, const short feather, const unsigned component ) 
			: BlockLogic(dgtmpl)
		{
			const short dimmf = _dim - feather;
			
			signed flags[3];
			flags[0] = bitmanip::testZero(signed(component - 0));
			flags[1] = bitmanip::testZero(signed(component - 1));
			flags[2] = bitmanip::testZero(signed(component - 2));

			_feathers[0].min = short((feather & flags[0]) | (0 & ~flags[0]));
			_feathers[1].min = short((feather & flags[1]) | (0 & ~flags[1]));
			_feathers[2].min = short((feather & flags[2]) | (0 & ~flags[2]));

			_feathers[0].max = short((dimmf & flags[0]) | (_dim & ~flags[0]));
			_feathers[1].max = short((dimmf & flags[1]) | (_dim & ~flags[1]));
			_feathers[2].max = short((dimmf & flags[2]) | (_dim & ~flags[2]));
		}

		void FieldAccessor::BlockLogicFeathered::clamp( const Coords &c0, const Coords &cN )
		{
			using bitmanip::clamp;

			x0 = clamp(_feathers[0].min, _dim1, c0.i);
			y0 = clamp(_feathers[1].min, _dim1, c0.j);
			z0 = clamp(_feathers[2].min, _dim1, c0.k);

			xN = clamp((short)-1, _feathers[0].max, cN.i);
			yN = clamp((short)-1, _feathers[1].max, cN.j);
			zN = clamp((short)-1, _feathers[2].max, cN.k);
		}
		void FieldAccessor::gradient_iterator::process()
		{
			OHT_CR_START();

			_dimc = _stripestuff.dimx(component);

			for (_leftright = 0; _leftright < 2; ++_leftright)
			{
				_stripe = ComponentIndex_to_OrthogonalNeighbor[component][_leftright];

				if (OrthogonalNeighbor_to_Touch3DSide[_stripe] & _t3df)
				{
					_stripestuff.init(_stripe, _c0, _cN, _coords);
					if (Mat2D3D[_stripe][component].d)
					{
						_pBlockVal = & _curr.left;
						_pStripeVal = & _curr.right;
					} else
					{
						_pBlockVal = & _curr.right;
						_pStripeVal = & _curr.left;
					}

					for (
						_sidx = _stripestuff.index,
						_index = _stripestuff.block.index,
						_vidx = _stripestuff.block.index + 
						signed short(
							(+1*_dimc) & ~Mat2D3D[_stripe][component].d | 
							(-1*_dimc) &  Mat2D3D[_stripe][component].d
						),
						_coords[_stripestuff.s1] = _stripestuff.y0; 

						_coords[_stripestuff.s1] <= _stripestuff.yN; 

						++_coords[_stripestuff.s1], 
						_sidx += _stripestuff.advanceY,
						_index += _stripestuff.block.advance4V,
						_vidx += _stripestuff.block.advance4V
					)
					{
						for (
							_coords[_stripestuff.s0] = _stripestuff.x0; 
							_coords[_stripestuff.s0] <= _stripestuff.xN; 
							++_coords[_stripestuff.s0], 
								_vidx += _stripestuff.block.advance4U,
								_index += _stripestuff.block.advance4U,
								++_sidx
						)
						{
							OgreAssert(_sidx < _dgtmpl.sidegpcount, "Stripe-Buffer overrun");

							*_pBlockVal = _values[_vidx];
							*_pStripeVal = _stripes[_stripe][_sidx];
							OgreAssert(_index < _dgtmpl.gpcount, "Index was out of bounds");
							OHT_CR_RETURN_VOID(CRS_Stripe);
						}
					}
				}
			}

			_blockstuff.init(_c0, _cN);

			for (
				_lidx = _blockstuff.index - _dimc,
				_index = _blockstuff.index,
				_ridx = _blockstuff.index + _dimc,
				_coords[2] = _blockstuff.z0; 

				_coords[2] <= _blockstuff.zN; 

				++_coords[2], 
				_lidx += _blockstuff.advanceZ,
				_index += _blockstuff.advanceZ,
				_ridx += _blockstuff.advanceZ
			)
			{
				for (
					_coords[1] = _blockstuff.y0; 
				
					_coords[1] <= _blockstuff.yN; 
					
					++_coords[1], 
					_lidx += _blockstuff.advanceY,
					_index += _blockstuff.advanceY,
					_ridx += _blockstuff.advanceY
				)
				{
					for (
						_coords[0] = _blockstuff.x0; 
						
						_coords[0] <= _blockstuff.xN; 
						
						++_coords[0],
						++_index
					)
					{
						OgreAssert(_index < _dgtmpl.gpcount, "Voxel-Buffer overrun");
						_curr.left = _values[_lidx++];
						_curr.right = _values[_ridx++];
						OHT_CR_RETURN_VOID(CRS_Values);
					}
				}
			}

			OHT_CR_END();

			_term = true;
		}

		FieldAccessor::gradient_iterator::gradient_iterator
		( 
			const unsigned component, 
			const CubeDataRegionDescriptor & dgtmpl, 
			const Coords & c0, const Coords & cN, 
			FieldStrength ** vvStripes, 
			FieldStrength * vValues 
		) 
			:	component(component),
			_dgtmpl(dgtmpl), _c0(c0), _cN(cN), _dim1(dgtmpl.dimensions + 1),
			_t3df(
				getTouch3DSide(
					TouchStatus(getTouchStatus(c0.i, 0, dgtmpl.dimensions) | getTouchStatus(cN.i, 0, dgtmpl.dimensions)),
					TouchStatus(getTouchStatus(c0.j, 0, dgtmpl.dimensions) | getTouchStatus(cN.j, 0, dgtmpl.dimensions)),
					TouchStatus(getTouchStatus(c0.k, 0, dgtmpl.dimensions) | getTouchStatus(cN.k, 0, dgtmpl.dimensions))
				)
			), 
			_blockstuff(dgtmpl, 1, component), _stripestuff(dgtmpl),
			_stripes(vvStripes), _values(vValues), _term(false)
		{
			OHT_CR_INIT();

			using bitmanip::testZero;
			_cf[0] = testZero(signed int(component - 0));
			_cf[1] = testZero(signed int(component - 1));
			_cf[2] = testZero(signed int(component - 2));

			process();
		}

		FieldAccessor::gradient_iterator::gradient_iterator( const gradient_iterator & copy ) 
			:	OHT_CR_COPY(copy), component(copy.component),
			_dgtmpl(copy._dgtmpl), 
			_c0(copy._c0), _cN(copy._cN), _dim1(copy._dim1), _dimc(copy._dimc),
			_t3df(copy._t3df), _pBlockVal(copy._pBlockVal), _pStripeVal(copy._pStripeVal),
			_stripes(copy._stripes), _values(copy._values), 
			_curr(copy._curr), _stripe(copy._stripe), _lidx(copy._lidx), _ridx(copy._ridx),
			_sidx(copy._sidx), _vidx(copy._vidx),
			_leftright(copy._leftright),
			_stripestuff(copy._stripestuff), _blockstuff(copy._blockstuff),
			_coords(copy._coords), _index(copy._index),
			_term(copy._term)
		{
			_cf[0] = copy._cf[0];
			_cf[1] = copy._cf[1];
			_cf[2] = copy._cf[2];
		}

		FieldAccessor::StripeLogicExt::StripeLogicExt( const CubeDataRegionDescriptor & dgtmpl ) 
			: StripeLogic(dgtmpl)
		{
			_dimx[0] = 1;
			_dimx[1] = _dim1;
			_dimx[2] = _dim1*_dim1;
		}

		void FieldAccessor::StripeLogicExt::init( const unsigned stripe, const Coords & c0, const Coords & cN, Coords & start )
		{
			StripeLogic::init(stripe, c0, cN, start);
			
			block.advance4U = _dimx[s0];
			block.advance4V = _dimx[s1] - (xN - x0 + 1)*_dimx[s0];
			//block.index = start.k * _dim1*_dim1 + start.j * _dim1 + start.i;
			s2 = (s0 | s1) ^ 3;
			start[s2] = Mat2D3D[stripe][s2].d & _dim;
			block.index = x0*_dimx[s0] + y0*_dimx[s1] + start[s2]*_dimx[s2];
		}


	}
}
