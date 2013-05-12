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

#include "OverhangTerrainPrerequisites.h"

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
