/*
-----------------------------------------------------------------------------
This source file is part of the Overhang Terrain Scene Manager library (OhTSM)
for use with OGRE.

Copyright (c) 2013 extollIT Enterprises
http://www.extollit.com

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
-----------------------------------------------------------------------------
*/

#ifndef __OVERHANGTERRAINUTIL_H__
#define __OVERHANGTERRAINUTIL_H__

#include <boost/thread.hpp>

#include <OgreException.h>
#include <OgreStreamSerialiser.h>
#include <OgreLog.h>
#include <OgreVector3.h>

#include "OverhangTerrainPrerequisites.h"
#include "Neighbor.h"

/// Boost scoped try/lock
#define OHT_TRY_LOCK_MUTEX(x) boost::recursive_mutex::scoped_try_lock lock(_mutex); if (lock)

/// Co-routines macros
#define OHT_CR_CONTEXT			int __s;
#define OHT_CR_COPY(from)		__s(from.__s)
#define OHT_CR_INIT()			__s = 0;
#define OHT_CR_START()			switch (__s) { case 0:
#define OHT_CR_RETURN(l,x)		{ __s = l; return x; case l: ; }
#define OHT_CR_RETURN_VOID(l)	{ __s = l; return; case l: ; }
#define OHT_CR_END()			{ break; default: for (;;) ; } } __s = 0;
#define OHT_CR_TERM				(__s == 0)

/// Compiler macro flexibility shortcomings
#define TOKPASTEIMPL(l,r) l##r
#define TOKPASTE(l,r) TOKPASTEIMPL(l,r)
#ifdef _MSC_VER 
#define UNIQUEVAR(x) TOKPASTE(x, __COUNTER__)
#else
#define UNIQUEVAR(x) TOKPASTE(x, __LINE__)
#endif

template< typename T > unsigned long long computeCRC (const T * pData, const size_t nElements)
	{ return computeCRCImpl(reinterpret_cast< const unsigned long long * > (pData), nElements * sizeof(T) / sizeof(unsigned long long)); }

/** Computes a CRC value of the specified block of memory
@param pData Block of memory to compute the CRC of
@param nQWords Size of the memory block in QWords (not bytes) 
@returns a QWord CRC value */
unsigned long long computeCRCImpl (const unsigned long long * pData, const size_t nQWords);

namespace Ogre
{
	/// Provides a way of specifying literal numbers in binary notation
	namespace literal
	{
		template<unsigned long N>
		struct B {
			enum { _ = (N%10)+2*B<N/10>::_ };
		} ;

		template<>
		struct B<0> {
			enum { _ = 0 };
		};
	}

	/// Functions that employ conditionals without branching
	namespace bitmanip
	{
		/// @returns All bits set if the specified ordinal is zero otherwise all bits unset
		template< typename T >
		T testZero(const T t)
		{
			const static auto NBITS = (sizeof(T) << 3) - 1;
			return ((t - 1) & ~t & (1 << NBITS)) >> NBITS;
		}

		/// @returns The minimum of the two
		template< typename T >
		T minimum(const T x, const T y)
		{
			return y ^ ((x ^ y) & -(x < y));
		}
		/// @returns The maximum of the two
		template< typename T >
		T maximum(const T x, const T y)
		{
			return x ^ ((x ^ y) & -(x < y));
		}
		/// @returns Conditionally clamps the specified value according to the specified low and high values
		template< typename T >
		T clamp(const T low, const T high, const T val)
		{
			return minimum(high, maximum(low, val));
		}
	}
	
	/// Denotes whether a coordinate is flush with a minimal-edge or maximal-edge or neither bounded by the dimensions
	enum TouchStatus
	{
		TS_None = 0,
		TS_Low = 1,
		TS_High = 2
	};

	/// Denotes whether a 2D-pair of coordinates are flush with a minimal or maximal edge or corner as bounded by the dimensions
	enum Touch2DSide
	{
		T2DS_None =			literal::B<   0>::_,

		T2DS_Left =			literal::B<   1>::_,
		T2DS_Right =		literal::B<  10>::_,

		T2DS_Top =			literal::B< 100>::_,
		T2DS_TopLeft =		literal::B< 101>::_,
		T2DS_TopRight =		literal::B< 110>::_,

		T2DS_Bottom =		literal::B<1000>::_,
		T2DS_BottomLeft =	literal::B<1001>::_,
		T2DS_BottomRight =	literal::B<1010>::_,

		T2DS_Minimal = T2DS_Left,
		T2DS_Maximal = T2DS_Right,

		Count2DTouchSideElements = 11
	};
	/// Denotes whether a 3D-tuple of coordinates are flush with a minimal or maximal side or edge as bounded by the dimensions
	enum Touch3DSide
	{
		T3DS_None =				T2DS_None,

		T3DS_West =				T2DS_Left,
		T3DS_East =				T2DS_Right,

		T3DS_Nether =			T2DS_Top,
		T3DS_NetherWest =		T2DS_TopLeft,
		T3DS_NetherEast =		T2DS_TopRight,

		T3DS_Aether =			T2DS_Bottom,
		T3DS_AetherWest =		T2DS_BottomLeft,
		T3DS_AetherEast =		T2DS_BottomRight,

		T3DS_North =			literal::B< 10000>::_,

		T3DS_NorthWest =		literal::B< 10001>::_,
		T3DS_NorthEast =		literal::B< 10010>::_,

		T3DS_NorthNether =		literal::B< 10100>::_,
		T3DS_NorthWestNether =	literal::B< 10101>::_,
		T3DS_NorthEastNether =	literal::B< 10110>::_,

		T3DS_NorthAether =		literal::B< 11000>::_,
		T3DS_NorthWestAether =	literal::B< 11001>::_,
		T3DS_NorthEastAether =	literal::B< 11010>::_,

		T3DS_South =			literal::B<100000>::_,

		T3DS_SouthWest =		literal::B<100001>::_,
		T3DS_SouthEast =		literal::B<100010>::_,

		T3DS_SouthNether =		literal::B<100100>::_,
		T3DS_SouthWestNether =	literal::B<100101>::_,
		T3DS_SouthEastNether =	literal::B<100110>::_,

		T3DS_SouthAether =		literal::B<101000>::_,
		T3DS_SouthWestAether =	literal::B<101001>::_,
		T3DS_SouthEastAether =	literal::B<101010>::_,

		T3DS_Minimal = T2DS_Left,
		T3DS_Maximal = T2DS_Right,

		CountTouch3DSides = literal::B<111111>::_ + 1
	};
	typedef Touch3DSide Touch3DFlags;

	/// Abbreviated names for the sides described above
	const extern char * Touch3DFlagNames[CountTouch3DSides];

	/// Translates from Touch3DSide type to Moore3DNeighbor type
	const extern signed char Touch3DSide_to_Moore3DNeighbor[CountTouch3DSides];
	/// Translates from OrthogonalNeighbor type to Touch3DSide type
	const extern Touch3DSide OrthogonalNeighbor_to_Touch3DSide[CountOrthogonalNeighbors];

	/// Translates from Touch3DSide type to Moore3DNeighbor type
	inline Moore3DNeighbor getMoore3DNeighbor (const Touch3DSide side)
	{
		return Moore3DNeighbor(Touch3DSide_to_Moore3DNeighbor[side]);
	}
	/** Retrieves the border/clamp flags for the specified number based on the specified minimum and maximum without branching
	@param n The number to test
	@param nMin The minimum bounded number
	@param nMax The maximum bounded number
	@returns Flags indicating whether the number is equal to the minimum value, the maximum value, both, or neither
	*/
	inline TouchStatus getTouchStatus(const int n, const int nMin, const int nMax)
	{
		using Ogre::bitmanip::testZero;
		return TouchStatus((testZero(n - nMin) & 1) | ((testZero(nMax - n) << 1) & 2));
	}
	/// @returns The Touch2DSide based on the specified 2D touch flags
	inline Touch2DSide getTouch2DSide (const TouchStatus tsX, const TouchStatus tsY)
	{
		return Touch2DSide(
			tsX | (tsY << 2)
		);
	}
	/// @returns The Touch3DSide based on the specified 3D touch flags
	inline Touch3DSide getTouch3DSide (const TouchStatus tsX, const TouchStatus tsY, const TouchStatus tsZ)
	{
		return Touch3DSide(
			tsX | (tsY << 2) | (tsZ << 4)
		);
	}

	/** Conditionally clamps the pair of 2-dimensional coordinates based on a touch side
	@remarks Clamps 'p' and/or 'q' to <0,N> according to the touch side
	@param t2ds North, East, South or West clamping instruction
	@param p The horizontal component that will be conditionally clamped
	@param q The vertical component that will be conditionally clamped
	@param N The maximum dimension to clamp either p/q to if the touch is south or east
	*/
	template< typename T >
	void flushSides(const Touch2DSide t2ds, T & p, T & q, const T N)
	{
		register size_t i = t2ds;
		if (i & 0x3)
			p = ((i & 0x3) - 1) * N;
		i >>= 2;
		if (i & 0x3)
			q = ((i & 0x3) - 1) * N;
	}

	/// Coordinate spaces used to express OhTSM positions, rays, and bounding-boxes in different ways
	enum OverhangCoordinateSpace
	{
		OCS_Terrain = 0,
		OCS_Vertex = 1,
		OCS_World = 2,
		OCS_PagingStrategy = 3,
		OCS_DataGrid = 4,

		NumOCS = 5
	};

	/// Clamps the direction component of the specified ray by the specified tolerance amount
	Ray & clamp(Ray & ray, const float fTolerance);

	/// Coordinate type used to represent discrete coordinates such as in a 3D voxel grid or to represent meta-regions in terrain tiles or page sections
	template< typename T, typename Subclass >
	class CellCoords
	{
	public:
		// DEPS: operator [] depends on these fields appearing first in declaration sequence
		T i, j, k;

		CellCoords () : i(0), j(0), k(0) {}
		CellCoords (CellCoords && move)
			: i(move.i), j(move.j), k(move.k) {}
		CellCoords (const T i, const T j, const T k)
			: i(i), j(j), k(k) {}

		inline Subclass operator - (const Subclass & vc) const
		{
			return Subclass(
				i - vc.i,
				j - vc.j,
				k - vc.k
			);
		}
		inline Subclass operator + (const Subclass & vc) const
		{
			return Subclass(
				i + vc.i,
				j + vc.j,
				k + vc.k
			);
		}
		inline Subclass & operator -= (const Subclass & vc)
		{
			i -= vc.i;
			j -= vc.j;
			k -= vc.k;
			return *static_cast< Subclass * > (this);
		}
		inline Subclass & operator += (const Subclass & vc)
		{
			i += vc.i;
			j += vc.j;
			k += vc.k;
			return *static_cast< Subclass * > (this);
		}

		template< typename J >
		inline Subclass operator - (const J value) const
		{
			return Subclass(
				i - value,
				j - value,
				k - value
			);
		}

		template< typename J >
		inline Subclass operator + (const J value) const
		{
			return Subclass(
				i + value,
				j + value,
				k + value
			);
		}

		template< typename J >
		inline Subclass & operator -= (const J value)
		{
			i -= value;
			j -= value;
			k -= value;
			return *static_cast< Subclass * > (this);
		}

		template< typename J >
		inline Subclass & operator += (const J value)
		{
			i += value;
			j += value;
			k += value;
			return *static_cast< Subclass * > (this);
		}

		template< typename J >
		inline Subclass operator / (const J value) const
		{
			return Subclass
				(
					i / value,
					j / value,
					k / value
				);
		}

		template< typename J >
		inline Subclass operator * (const J value) const
		{
			return Subclass
				(
					i * value,
					j * value,
					k * value
				);
		}

		template< typename J >
		inline Subclass & operator /= (const J value)
		{
			i /= value;
			j /= value;
			k /= value;
			return *static_cast< Subclass * > (this);
		}

		template< typename J >
		inline Subclass & operator *= (const J value)
		{
			i *= value;
			j *= value;
			k *= value;
			return *static_cast< Subclass * > (this);
		}

		template< typename J >
		inline Subclass operator % (const J value) const
		{
			return Subclass
				(
					i % value,
					j % value,
					k % value
				);
		}

		template< typename J >
		inline Subclass & operator %= (const J value)
		{
			i %= value;
			j %= value;
			k %= value;
			return *static_cast< Subclass * > (this);
		}

		inline Subclass & operator >>= (const unsigned s)
		{
			i >>= s;
			j >>= s;
			k >>= s;
			return *static_cast< Subclass * > (this);
		}

		inline Subclass operator - () const
		{
			return Subclass (
				-i,
				-j,
				-k
			);
		}

		inline Subclass operator & (const T mask) const
		{
			return Subclass(
				i & mask,
				j & mask,
				k & mask
			);
		}

		inline Subclass & operator &= (const T mask)
		{
			i &= mask;
			j &= mask;
			k &= mask;
			return *static_cast< Subclass * > (this);
		}

		inline Subclass & operator |= (const Subclass & vc)
		{
			i |= vc.i;
			j |= vc.j;
			k |= vc.k;
			return *static_cast< Subclass * > (this);
		}

		inline T & operator [] (const unsigned nSimplex)
		{
			OgreAssert(nSimplex < 3, "Supports only points, lines, and faces");
			return reinterpret_cast< T * > (this)[nSimplex];
		}
		inline const T & operator [] (const unsigned nSimplex) const
		{
			OgreAssert(nSimplex < 3, "Supports only points, lines, and faces");
			return reinterpret_cast< const T * > (this)[nSimplex];
		}

		inline bool operator == (const Subclass & vc) const
		{ return i == vc.i && j == vc.j && k == vc.k; }
		
		inline bool operator != (const Subclass & vc) const
		{ return i != vc.i || j != vc.j || k != vc.k; }

		inline bool operator < (const Subclass & other) const
		{ return hash() < other.hash(); }

		inline Ogre::Log::Stream & operator << (Ogre::Log::Stream & out) const { return write(out); }
		inline std::ostream & operator << (std::ostream & out) const { return write(out); }

	private:
		template< typename S > inline S & write (S & outs) const 
		{ return outs << "<" << i << "," << j << "," << k << ">"; }

		inline unsigned long long hash() const
		{ return (unsigned long long (k) << 32) | (unsigned long long (j) << 16) | i; }

		friend Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const Subclass & vc);
		friend std::ostream & operator << (std::ostream & outs, const Subclass & vc);
	};

	/// Coordinate type used to represent 3D world coordinates in the same units as grid cell coordinates of a 3D voxel grid / cube region
	class DiscreteCoords : public CellCoords< signed long, DiscreteCoords >
	{
	public:
		DiscreteCoords () : CellCoords() {}
		DiscreteCoords (DiscreteCoords && move)
			: CellCoords(move) {}
		DiscreteCoords (const signed int i, const signed int j, const signed int k)
			: CellCoords(i, j, k) {}
	};

	inline Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const DiscreteCoords & dc)
		{ return dc.write(outs); }
	inline std::ostream & operator << (std::ostream & outs, const DiscreteCoords & dc)
		{ return dc.write(outs); }

	/// Iterator pattern for walking through a discrete 3D-grid of arbitrary size
	class DiscreteRayIterator : public std::iterator <std::input_iterator_tag, DiscreteCoords >
	{
	private:
		/// Runs a single iteration
		void iterate();

		/// Perform dependency-injection on nested class objects
		void DI();

	protected:
		/// Incremented gradually until one of its components crosses 1.0, always positive
		Vector3 _walker;

		/// Increments the walker, this is always positive
		Vector3 _incrementor;

		/// Used to update the cell coordinates, accounts for negative directions unlike the incrementor, mask flags indicating negativity of the corresponding deltas
		struct Delta {
			signed short x, y, z;
			size_t mx, my, mz;
		} _delta;

		/// Linear offset from intersected cell boundary from previous step to the current position
		Real _off;

		/// Used to determine which component of the walker crossed 1.0 first
		Vector3 _dist;

		/// Used to indicate the span of a single cell (float version)
		Real _fspan;

		/// Total linear distance traversed thus far
		Real _ldist;

		/// Used to indicate the span of a single cell (integral version)
		signed long _ispan;

		/// Which cell in discrete space are we currently in?
		DiscreteCoords _cell;

		/// Touch side of previous cell that was crossed-over into the current cell
		Touch3DSide _t3ds;

	public:
		/// Positional offset of discrete cells
		const Vector3 offset;
		/// Ray origin and direction
		const Ray ray;

		/// Property accessor for the current cell position coordinates
		class PositionPropertyAccessor
		{
		public:
			inline const Vector3 * operator -> () const
			{
				updatePosition();
				return &_vector;
			}
			inline operator const Vector3 & () const
			{
				updatePosition();
				return _vector;
			}
			inline const Vector3 & operator () (const Real offset) const
			{
				updatePosition(offset);
				return _vector;
			}

		protected:
			/// Cache variable for the updated position
			mutable Vector3 _vector;

			/// Dependency injection of the root property
			void DI (const Ray & ray, const Real & fLDist, const Real & fspan);

			/// Computes the position offset by the specified real amount
			virtual void updatePosition(const Real offset = 0.0f) const;

		private:
			const Ray * _pRay;
			const Real * _pLDist;
			const Real * _pfSpan;

			friend class DiscreteRayIterator;
			friend Ogre::Log::Stream & operator << (Ogre::Log::Stream &, const PositionPropertyAccessor &);
		} position;

		class IntersectionPropertyAccessor : public PositionPropertyAccessor
		{
		protected:
			virtual void updatePosition(const Real offset = 0.0f) const;

			void DI (const Ray & ray, const Real & fLDist, const Real & off, const Real & fspan);

		private:
			const Real * _pOff;
			const Real * _pfSpan;

			friend class DiscreteRayIterator;
			friend Ogre::Log::Stream & operator << (Ogre::Log::Stream &, const IntersectionPropertyAccessor &);
		} intersection;

		class LinearDistancePropertyAccessor
		{
		public:
			inline operator const Real () const { return *_pLDist * *_pfSpan; }

		protected:
			/// Dependency injection of the root property
			void DI (const Real & fLDist, const Real & fSpan);

		private:
			const Real * _pLDist;
			const Real * _pfSpan;

			friend Ogre::Log::Stream & operator << (Ogre::Log::Stream &, const LinearDistancePropertyAccessor &);
			friend class DiscreteRayIterator;
		} distance;

		/// Property accessor for the neighbor of previous cell that was crossed-over into the current cell
		class NeighborPropertyAccessor
		{
		public:
			inline operator const Moore3DNeighbor () const { return getMoore3DNeighbor(*_pT3DS); }

		protected:
			/// Dependency injection of the root property
			inline void operator &= (Touch3DSide & t3ds) { _pT3DS = &t3ds; }

		private:
			Touch3DSide * _pT3DS;

			friend class DiscreteRayIterator;
		} neighbor;

		/** 
		@param Ray ray
		@param limit Optional search limit length of the ray or zero to specify no limit
		@param offset Positional offset of discrete cells
		*/
		DiscreteRayIterator(const Ray & ray, const Real fCellSize, const Vector3 & offset = Vector3::ZERO);
		DiscreteRayIterator(const DiscreteRayIterator & copy);
		DiscreteRayIterator(DiscreteRayIterator && move);

		static inline DiscreteRayIterator from (const Ray & ray, const Real fCellSize, const Vector3 & offset = Vector3::ZERO) { return DiscreteRayIterator (ray, fCellSize, offset); }

		bool operator == (const DiscreteRayIterator & other) const;
		bool operator != (const DiscreteRayIterator & other) const;

		inline
		bool operator < (const Real fDist) const { return _ldist < fDist; }

		DiscreteCoords & operator * () { return _cell; }
		DiscreteCoords * operator -> () { return &_cell; }

		DiscreteRayIterator & operator ++ ();
		DiscreteRayIterator operator ++ (int);
	};

	inline
	Ogre::Log::Stream & operator << (Ogre::Log::Stream & stream, const DiscreteRayIterator::PositionPropertyAccessor & position)
	{
		return stream << static_cast< const Vector3 & > (position);
	}

	inline
	Ogre::Log::Stream & operator << (Ogre::Log::Stream & stream, const DiscreteRayIterator::IntersectionPropertyAccessor & intersection)
	{
		return stream << static_cast< const Vector3 & > (intersection);
	}

	inline
	Ogre::Log::Stream & operator << (Ogre::Log::Stream & stream, const DiscreteRayIterator::LinearDistancePropertyAccessor & distance)
	{
		return stream << static_cast< const Real & > (distance);
	}

	/** Type to represent a fixed-precision rational value 
	@remarks "B" is the number of bits to occupy for the fractional part, "T" is the integral type to represent the entire number in memory
	*/
	template< int B, typename T >
	class FixedPrecision
	{
	private:
		const static long 
			UNIT = 1 << B,
			MASK = UNIT - 1;

		static_assert(B < ((sizeof(long) << 3) >> 1) - 1, "Precision is too high");

		long _fval;

		FixedPrecision(const long fval)
			: _fval(fval) {}

		inline static long toNative(const T val)
		{
			return (val << B);
		}
		inline static long abs(const long val)
		{
			const long mask = val >> ((sizeof(long) << 3) - 1);
			return (val + mask) ^ mask;
		}

		inline long mul(const long l) const
		{
			return _fval * (l / UNIT) + (_fval * (l % UNIT)) / UNIT;
		}
		inline long div(const long l) const
		{
			return mul((UNIT*UNIT) / l);
		}

		template< int B2, typename T2 >
		inline static long convert(const FixedPrecision< B2, T2 > & other)
		{
			const register long l = other.bits();
			
			return (
				B2 - B < 0 
				? l << (unsigned(B - B2) & ((sizeof(int) << 3) - 1)) 
				: l >> (unsigned(B2 - B) & ((sizeof(int) << 3) - 1))
			);
		}

	public:
		const static int FRACBITS = B;

		FixedPrecision() {}

		FixedPrecision(const T val)
			: _fval (val << B) {}
		FixedPrecision(const Real val)
			: _fval (long(val * Real(UNIT))) {}
		FixedPrecision(FixedPrecision && move)
			: _fval (move._fval) {}
		
		template< int B2, typename T2 >
		FixedPrecision(const FixedPrecision< B2, T2 > & other)
			: _fval (convert(other)) {}

		/// Returns the raw unencoded memory used to store the number
		inline
		long bits () const { return _fval; }

		inline
		FixedPrecision operator + (const T val) const
			{ return _fval + toNative(val); }

		inline
		FixedPrecision operator - (const T val) const
			{ return _fval - toNative(val); }

		inline
		FixedPrecision operator * (const T val) const
			{ return _fval * val; }

		inline
		FixedPrecision operator / (const T val) const
			{ return _fval / val; }

		inline
		FixedPrecision & operator += (const T val)
		{ 
			_fval += toNative(val); 
			return *this;
		}

		inline
		FixedPrecision & operator -= (const T val)
		{ 
			_fval -= toNative(val); 
			return *this;
		}

		inline
		FixedPrecision & operator *= (const T val)
		{ 
			_fval *= val; 
			return *this;
		}

		inline
		FixedPrecision & operator /= (const T val)
		{ 
			_fval /= val; 
			return *this;
		}

		inline
		FixedPrecision operator + (const FixedPrecision & val) const
			{ return _fval + val._fval; }

		inline
		FixedPrecision operator - (const FixedPrecision & val) const
			{ return _fval - val._fval; }

		inline
		FixedPrecision operator * (const FixedPrecision & val) const
		{ 
			return mul(val._fval);
		}

		inline
		FixedPrecision operator / (const FixedPrecision & val) const
		{ 
			return div(val._fval);
		}

		inline
		FixedPrecision & operator += (const FixedPrecision & val)
		{ 
			_fval += val._fval; 
			return *this;
		}

		inline
		FixedPrecision & operator -= (const FixedPrecision & val)
		{ 
			_fval -= val._fval; 
			return *this;
		}

		inline
		FixedPrecision & operator *= (const FixedPrecision & val)
		{ 
			_fval = mul(val._fval);
			return *this;
		}

		inline
		FixedPrecision & operator /= (const FixedPrecision & val)
		{ 
			_fval = div(val._fval);
			return *this;
		}

		inline
		FixedPrecision operator - () const
			{ return -_fval; }

		inline
		FixedPrecision & operator ++ ()
		{
			_fval += UNIT;
			return *this;
		}
		inline
		FixedPrecision operator ++ (int)
		{
			FixedPrecision fp = *this;
			_fval += UNIT;
			return fp;
		}

		inline
		FixedPrecision & operator -- ()
		{
			_fval -= UNIT;
			return *this;
		}
		inline
		FixedPrecision operator -- (int)
		{
			FixedPrecision fp = *this;
			_fval -= UNIT;
			return fp;
		}

		inline
		FixedPrecision operator & (const long val) const
			{ return _fval & val; }

		inline
		FixedPrecision operator | (const long val) const
			{ return _fval | val; }

		inline
		operator Real () const 
		{ 
			return Real(_fval / UNIT) + Real((_fval % UNIT)) / Real(UNIT); 
		}

		inline
		operator T () const
		{
			return T(_fval / UNIT);
		}

		inline
		bool operator == (const T val) const
		{
			return _fval == toNative(val);
		}

		inline
		FixedPrecision sqrt() const 
		{
			return ::sqrt(*this);
		}

		inline
		FixedPrecision & operator = (const T val)
		{
			_fval = toNative(val);
			return *this;
		}
		inline
		FixedPrecision & operator = (const FixedPrecision & val)
		{
			_fval = val._fval;
			return *this;
		}
		inline
		FixedPrecision & operator = (const Real f)
		{
			_fval = long(f * Real(UNIT));
			return *this;
		}

		inline
		FixedPrecision & operator &= (const long val)
		{ 
			_fval &= val; 
			return *this;
		}

		inline
		FixedPrecision & operator |= (const long val)
		{ 
			_fval |= val;
			return *this;
		}

		inline
		bool operator < (const FixedPrecision & fp) const
		{
			return _fval < fp._fval;
		}
		
		inline
		bool operator > (const FixedPrecision & fp) const
		{
			return _fval > fp._fval;
		}

		inline
		bool operator <= (const FixedPrecision & fp) const
		{
			return _fval <= fp._fval;
		}
		
		inline
		bool operator >= (const FixedPrecision & fp) const
		{
			return _fval >= fp._fval;
		}

		inline
		bool operator < (const Real f) const
		{
			return operator Real () < f;
		}
		
		inline
		bool operator > (const Real f) const
		{
			return operator Real () > f;
		}

		inline
		bool operator < (const T val) const
		{
			return _fval < toNative(val);
		}

		inline
		bool operator > (const T val) const
		{
			return _fval > toNative(val);
		}
		
		inline
		bool operator <= (const T val) const
		{
			return _fval <= toNative(val);
		}

		inline
		bool operator >= (const T val) const
		{
			return _fval >= toNative(val);
		}

		/// Modifies the number slightly so that it is very small but not zero
		inline
		void nonZero()
		{
			_fval |= bitmanip::testZero(_fval) & 1;
		}

		template< typename S >
		S & write (S & outs) const
		{
			return outs << static_cast< Real > (*this);
		}
	};

	template< int B, typename T>
	Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const FixedPrecision< B, T > & fval)
		{ return fval.write(outs); }
	
	template< int B, typename T>
	std::ostream & operator << (std::ostream & outs, const FixedPrecision< B, T > & fval)
		{ return fval.write(outs); }

	/// Computes the square root of the number
	template< int B, typename T >
	FixedPrecision< B, T > SQRT(const FixedPrecision< B, T > & fp)
	{
		return fp.sqrt();
	}

	/** Represents a 3-dimensional vector using fixed-precision types
	@see FixedPrecision< B, T >
	*/
	template< int B, typename T>
	class FixVector3
	{
	public:
		typedef FixedPrecision< B, T > PrecisionType;

		PrecisionType x, y, z;

		FixVector3() {}

		FixVector3(const T x, const T y, const T z)
			: x(x), y(y), z(z) {}
		FixVector3(const Real x, const Real y, const Real z)
			: x(x), y(y), z(z) {}
		FixVector3(const PrecisionType & x, const PrecisionType & y, const PrecisionType & z)
			: x(x), y(y), z(z) {}

		FixVector3(PrecisionType && x, PrecisionType && y, PrecisionType && z)
			: x(x), y(y), z(z) {}
		FixVector3(FixVector3 && move)
			: x(move.x), y(move.y), z(move.z) {}
		explicit FixVector3(const Vector3 & copy)
			: x(copy.x), y(copy.y), z(copy.z) {}

		inline
		FixVector3 operator + (const FixVector3 & vec) const
			{ return FixVector3(x + vec.x, y + vec.y, z + vec.z); }

		inline
		FixVector3 operator + (const PrecisionType & val) const
			{ return FixVector3(x + val, y + val, z + val); }

		inline
		FixVector3 operator - (const FixVector3 & vec) const
			{ return FixVector3(x - vec.x, y - vec.y, z - vec.z); }

		inline
		FixVector3 operator - (const PrecisionType & val) const
			{ return FixVector3(x - val, y - val, z - val); }

		inline
		FixVector3 & operator += (const FixVector3 & vec)
		{
			x += vec.x;
			y += vec.y;
			z += vec.z;
			return *this;
		}

		inline
		FixVector3 & operator -= (const FixVector3 & vec)
		{
			x -= vec.x;
			y -= vec.y;
			z -= vec.z;
			return *this;
		}

		template< typename J >
		FixVector3 operator * (const J & val) const
			{ return FixVector3(x * val, y * val, z * val); }

		template< typename J >
		friend FixVector3 operator * (const J & val, const FixVector3 & vec)
			{ return FixVector3(val * vec.x, val * vec.y, val * vec.z); }

		FixVector3 operator * (const FixVector3 & vec) const
			{ return FixVector3(x * vec.x, y * vec.y, z * vec.z); }

		template< typename J >
		FixVector3 operator / (const J & val) const
			{ return FixVector3(x / val, y / val, z / val); }

		template< typename J >
		friend FixVector3 operator / (const J & val, const FixVector3 & vec)
			{ return FixVector3(val / vec.x, val / vec.y, val / vec.z); }
		
		FixVector3 operator / (const FixVector3 & vec) const
			{ return FixVector3(x / vec.x, y / vec.y, z / vec.z); }

		template< typename J >
		FixVector3 & operator *= (const J & val)
		{
			x *= val;
			y *= val;
			z *= val;
			return *this;
		}

		inline
		FixVector3 & operator /= (const FixVector3 & vec)
		{
			x /= vec.x;
			y /= vec.y;
			z /= vec.z;
			return *this;
		}

		template< typename J >
		FixVector3 & operator /= (const J & val)
		{
			x /= val;
			y /= val;
			z /= val;
			return *this;
		}

		/// Computes the cross-product of the two vectors
		inline
		FixVector3 operator % (const FixVector3 & fv) const
		{
			return FixVector3(
				y*fv.z - z*fv.y,
				z*fv.x - x*fv.z,
				x*fv.y - y*fv.x
			);
		}

		/// Computes the dot-product of the two vectors
		inline
		PrecisionType operator ^ (const FixVector3 & fv) const
		{
			return x*fv.x + y*fv.y + z*fv.z;
		}

		/// Performs componentwise and bitwise AND of the two vectors
		template< typename J >
		FixVector3 operator & (const J & m) const
		{
			return FixVector3(
				x & m,
				y & m,
				z & m
			);
		}

		/// Performs componentwise and bitwise OR of the two vectors
		template< typename J >
		FixVector3 operator | (const J & m) const
		{
			return FixVector3(
				x | m,
				y | m,
				z | m
			);
		}

		inline
		FixVector3 operator - () const
			{ return FixVector3(-x, -y, -z); }

		inline
		operator Vector3 () const
			{ return Vector3 (x, y, z); }

		template< typename S >
		S & write (S & outs) const
		{
			return outs << "FixVec3(" << x << ',' << y << ',' << z << ')';
		}
	};

	template< int B, typename T>
	Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const FixVector3< B, T > & fvec)
		{ return fvec.write(outs); }
	
	template< int B, typename T>
	std::ostream & operator << (std::ostream & outs, const FixVector3< B, T > & fvec)
		{ return fvec.write(outs); }
	
	/// Retrieves the magnitude of the vector
	template< int B, typename T>
	typename FixVector3< B, T >::PrecisionType LENGTH(const FixVector3< B, T > & fv)
	{
		return SQRT(LENGTH_SQ(fv));
	}
	
	/// Retrieves the squared magnitude of the vector
	template< int B, typename T>
	typename FixVector3< B, T >::PrecisionType LENGTH_SQ(const FixVector3< B, T > & fv)
	{
		return fv.x*fv.x + fv.y*fv.y + fv.z*fv.z;
	}

	/// Retrieves the normalized version of the vector
	template< int B, typename T>
	FixVector3< B, T > NORMALIZE(const FixVector3< B, T > & fv)
	{
		FixVector3< B, T >::PrecisionType l = LENGTH(fv);

		return FixVector3< B, T >
		(
			fv.x / l,
			fv.y / l,
			fv.z / l
		);
	}

	class BBox2D
	{
	public:
		Vector2 minimum, maximum;

		BBox2D();
		BBox2D(const Vector2 & minimum, const Vector2 & maximum);

		std::pair< bool, Real > intersects(const Ray & ray, const OverhangCoordinateSpace ocs = OCS_Terrain) const;
		bool intersects(const AxisAlignedBox & bbox, const OverhangCoordinateSpace ocs = OCS_Terrain) const;
	};

	inline
	std::pair< bool, Real > operator % (const Ray & ray, const BBox2D & bbox)
	{
		return bbox.intersects(ray);
	}

	namespace Math2
	{
		extern const Real RATIONAL_ERROR;

		/// Truncates the rational fractional part by a predetermined amount
		inline Real Trunc(const Real & r)
		{
			return Math::Floor(r / RATIONAL_ERROR) * RATIONAL_ERROR;
		}
		/// Truncates the vector fractional parts by a predetermined amount
		inline Vector3 Trunc(const Vector3 & v)
		{
			return Vector3(
				Trunc(v.x),
				Trunc(v.y),
				Trunc(v.z)
			);
		}

		/// Slightly modifies the rational if it is zero by a small amount so that its not zero
		inline
		float nonZero(const float f)
		{
			if (f == 0.0f)
				return std::numeric_limits< float >::epsilon();
			else
				return f;
		}
	}

	namespace RoleSecureFlag
	{
		/// Interface for setting the flag (turning it on)
		class ISetFlag
		{
		public:
			virtual bool operator ++ () = 0;
		};

		/// Interface for reading the flag (query)
		class IReadFlag
		{
		public:
			virtual operator bool () const = 0;
			virtual bool operator ! () const = 0;
		};

		/// Interface for clearing the flag (turning it off)
		class IClearFlag : public IReadFlag
		{
		public:
			virtual bool operator -- () = 0;
		};

		class Flag : protected ISetFlag, IClearFlag
		{
		private:
			bool _flag;

		protected:
			operator bool () const;
			bool operator ! () const;
			bool operator -- ();
			bool operator ++ ();

			template< typename INTERFACE > INTERFACE * queryInterface ()
			{
				static_assert(false, "Method implementation not found");
			}
			template<> IClearFlag * queryInterface <IClearFlag> () { return this; }
			template<> IReadFlag * queryInterface <IReadFlag> () { return this; }
			template<> ISetFlag * queryInterface <ISetFlag> () { return this; }

		public:
			Flag();
			virtual ~Flag();
		};
	}

	class BitSet
	{
	private:
		/// The bitset storage
		size_t * _vFlags;
		/// The number of bits to shift 1 in order to achieve the number of bits stored in a single size_t type and its mask
		size_t _nSizeTBitsShift, _nSizeTBitsMask;
		/// The total number of elements / flags in the set
		size_t _count;

		/// Computes the number of bits required to represent the bit-size of a size_t type
		inline static size_t computeSizeTBitsShift()
		{
			return size_t(logl(sizeof(size_t) << 3) / logl(2));
		}
		/** Computes the number of bytes for the given bit count 
		@param nBitCount The number of bits
		@param nSizeTBitsShift The number of bits required to represent the bit-size of a size_t type
		*/
		inline static size_t computeFlagElemCount(const size_t nBitCount, const size_t nSizeTBitsShift)
		{
			return (nBitCount >> nSizeTBitsShift) + 1;
		}
		/** Computes the number of bytes required to represent the specified number of bits */
		inline static size_t computeSizeTBitsMask(const size_t nSizeTBitsShift)
		{
			return ((1 << nSizeTBitsShift) - 1);
		}

	public:
		/// Wrapper class for a single bit in a bitset
		class Reference
		{
		private:
			/// The storage for the bit container
			size_t * p, 
			/// The mask on the bit container identifying the bit
				mask;
			/// The bit number of the bit in the container
			unsigned char n;

		protected:
			/** 
			@param p Pointer to the container containing the bit to reference
			@param n The bit number in the bit container pointed to by 'p'
			*/
			Reference(size_t * p, const unsigned char n)
				: p(p), n(n), mask(1 << n) {}

		public:
			/** Set or clear the bit
			@param v '1' to set the bit, '0' to clear the bit */
			inline
			void operator = (const unsigned char v)
			{
				OgreAssert((v & ~1) == 0, "Value must be either 1 or 0");
				*p ^= ((v << n) ^ *p) & mask;
			}

			/// @returns '1' if the bit is set, '0' if the bit is cleared
			inline
			operator unsigned char () const
			{
				return (*p & mask) >> n;
			}

			friend class BitSet;
		};

		BitSet () 
			: _count(0), _vFlags(NULL), 
			_nSizeTBitsShift(computeSizeTBitsShift()), 
			_nSizeTBitsMask (computeSizeTBitsMask(computeSizeTBitsShift()))
		{}
		/**
		@param nCount The number of bits
		*/
		BitSet (const size_t nCount) 
			: _count(nCount), 
			_vFlags(new size_t [computeFlagElemCount(nCount, computeSizeTBitsShift())]), 
			_nSizeTBitsShift(computeSizeTBitsShift()),
			_nSizeTBitsMask (computeSizeTBitsMask(computeSizeTBitsShift()))
		{}
		~BitSet()
		{
			delete [] _vFlags;
		}

		/// [re]allocates the bitset to accomodate the specified number of bits
		void allocate(const size_t nCount)
		{
			_count = nCount;
			delete [] _vFlags;
			_vFlags = new size_t[computeFlagElemCount(nCount, _nSizeTBitsShift)];
		}

		/// Clears all bits
		inline void clear () 
		{
			OgreAssert(_vFlags != NULL, "Must allocate first");
			memset(_vFlags, 0, ((_count >> _nSizeTBitsShift) + 1) * sizeof(size_t));
		}

		/// @returns A reference to the bit at the specified bit offset
		inline
		Reference operator [] (const size_t i)
		{
			OgreAssert(_vFlags != NULL, "Must allocate first");
			OgreAssert((i >> _nSizeTBitsShift) <= (_count >> _nSizeTBitsShift), "IsoVertex flag index out of bounds");
			return Reference(&_vFlags[i >> _nSizeTBitsShift], i & _nSizeTBitsMask);
		}
	};

	/// Computes the cross product of two vectors
	inline Vector3 CROSS(const Vector3 & a, const Vector3 & b)
	{
		return a.crossProduct(b);
	}
	/// Computes the cross product of two vectors
	inline Real DOT(const Vector3 & a, const Vector3 & b)
	{
		return a.dotProduct(b);
	}
	/// Normalizes the specified vector
	inline Vector3 & NORMAL(Vector3 && v)
	{
		v.normalise();
		return v;
	}
	/// @returns The squared length of the vector
	inline Real LENGTH_SQ(Vector3 && v)
	{
		return v.squaredLength();
	}
}

#endif
