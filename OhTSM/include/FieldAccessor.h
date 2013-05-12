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

#ifndef __OVERHANGTERRAINVOXELGRID_H__
#define __OVERHANGTERRAINVOXELGRID_H__

#include "OverhangTerrainPrerequisites.h"

#include "CubeDataRegionDescriptor.h"
#include "IsoSurfaceSharedTypes.h"
#include "Util.h"

namespace Ogre
{
	namespace Voxel
	{
		/** Linear abstraction for a contiguous block additionally feathered by one element orthogonally on each
			side of the 3D cube it represents.
			@remarks Each field is feathered with 6 2-dimensional decks representing each of the sides of the cube.
			This is used by implentations of MetaObject when generating voxels to generate the gradient as well.
			Since the lifecycle of these feathers is only during voxel manipulation, they can be discarded since
			they take-up lots of additional memory.
		*/
		class _OverhangTerrainPluginExport FieldAccessor
		{
		private:
			const CubeDataRegionDescriptor & _cubemeta;
			const size_t _mxy;

			FieldStrength 
				* stripes[CountOrthogonalNeighbors],
				dummy;
			FieldStrength
				* accessors[1 + CountOrthogonalNeighbors + 1];

		public:
			class Coords : public CellCoords< signed short, Coords >
			{
			public:
				Coords () : CellCoords() {}
				Coords (Coords && move)
					: CellCoords(move) {}
				Coords (const signed short i, const signed short j, const signed short k)
					: CellCoords(i, j, k) {}
			};

			class iterator : public std::iterator< std::input_iterator_tag, FieldStrength >
			{
			private:
				const CubeDataRegionDescriptor & _dgtmpl;
				FieldStrength 
					* const * const _stripes, 
					* const _values,
					* _curr;

				unsigned _stripe, _index;
				signed short _dim;
				
				struct StripeStuff 
				{
					unsigned char s0, s1;
					signed short x0, y0, xN, yN;
					size_t advanceY;
				} _stripestuff;
				struct VoxelStuff
				{
					signed short x0, y0, z0, xN, yN, zN;
					size_t advanceY, advanceZ;
				} _blockstuff;
				Coords _coords;
				const Coords _c0, _cN;
				const Touch3DFlags _t3df;
				bool _term;

				enum CRState
				{
					CRS_Stripe = 1,
					CRS_Values = 2
				};

				OHT_CR_CONTEXT;

				void process ();

				void initBlockWalk();
				void initStripeWalk();

			public:
				iterator (const iterator & copy);
				iterator (const CubeDataRegionDescriptor & dgtmpl, const Coords & c0, const Coords & cN, FieldStrength ** vvStripes, FieldStrength * vValues);

				inline
				operator const Coords & () const { return _coords; }

				inline
				const FieldStrength & operator * () const { return *_curr; }

				inline
				FieldStrength & operator * () { return *_curr; }

				inline
				const FieldStrength & operator -> () const { return *_curr; }

				inline
				FieldStrength & operator -> () { return *_curr; }

				inline
				operator bool () const { return !_term; }

				inline
				iterator & operator ++ ()
				{
					process();
					return *this;
				}

				inline
				iterator operator ++ (int)
				{
					iterator old = *this;
					process();
					return old;
				}
			};

			const int min, max;
			FieldStrength * const values;

			FieldAccessor (const CubeDataRegionDescriptor & dgtmpl, FieldStrength * values);
			FieldAccessor (const FieldAccessor & copy);
			FieldAccessor (FieldAccessor && move);
			~FieldAccessor();

			inline
			iterator iterate ()
			{
				return iterate(-1, -1, -1, _cubemeta.dimensions+1, _cubemeta.dimensions+1, _cubemeta.dimensions+1);
			}
			iterator iterate (
				const signed short x0, const signed short y0, const signed short z0,
				const signed short xN, const signed short yN, const signed short zN
			);

			void clear();

			FieldStrength & operator () (const int x, const int y, const int z);
		};
	}
}

#endif
