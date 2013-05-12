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
#ifndef __OVERHANGTERRAINRUNLENGTHENCODINGUTILITIES_H__
#define __OVERHANGTERRAINRUNLENGTHENCODINGUTILITIES_H__

#include <vector>

#include <OgreStreamSerialiser.h>

namespace Ogre
{
	namespace RLE
	{
		/** Provides and stores run-length encoded compressed data
		@remarks The class is called "Channel" due to practical use.  In the case of some
		aggregate data-types posessing multiple components (such as an RGBA colour) it makes
		sense to compress buffers per component to maximize the benefit of RLE compression.
		For example in the case of a field of RGBA colours, one channel would be 
		assigned for all of the reds, another for all of the greens, another for blues, 
		and finally alphas.  Each channel is compressed independently and can also benefit
		from parallelization (future).
		*/
		class Channel
		{
		private:
			size_t _zsize;
			unsigned char * _buffer;

			const static int PRECISION = 7;

			enum Flags
			{
				Flag_Bigger = 0x80,
				Flag_Heterogenous = 0x01,
				Flag_Homogenous = 0x00,
				Flag_Neither = 0x02
			};

			inline
			void appendBuffer(const size_t d, const unsigned char * pcSrc, const size_t nCount)
			{
				if (d + nCount > _zsize)
				{
					_zsize += nCount;
					_zsize *= 2;
					_buffer = reinterpret_cast< unsigned char * > (realloc(_buffer, _zsize));
				}
				memcpy(&_buffer[d], pcSrc, nCount);
			}

			inline
			void appendBuffer(const size_t d, const unsigned char c)
			{
				if (d >= _zsize)
				{
					_zsize *= 2;
					_buffer = reinterpret_cast< unsigned char * > (realloc(_buffer, _zsize));
				}
				_buffer[d] = c;
			}

			void flushCounter( unsigned int & i, size_t & d, const Flags enfInverse );

		public:
			Channel();

			/** Compress a block of data
			@remarks Stores a compressed representation of the specified block in this object that can be accessed later
			@param nDecompSize The byte size of the block of data to compress
			@param pcSrc The block of data to compress
			*/
			void compress (const size_t nDecompSize, const unsigned char * pcSrc);
			
			/** Decompress the previously compressed data from this object
			@remarks Loads the compressed data from this object into the specified memory block
			@param nDecompSize byte size of the data when uncompressed
			@param pDest The destination buffer for decompressed data
			*/
			void decompress (const size_t nDecompSize, unsigned char * pDest) const;

			/// Writes the compressed channel to the stream
			StreamSerialiser & operator >> (StreamSerialiser & outs) const;
			/// Reads the compressed channel from the stream
			StreamSerialiser & operator << (StreamSerialiser & ins);

			/// Retrieves the total byte size of the compressed data
			inline
			size_t getCompressedSize() const { return _zsize; }

			~Channel();
		};
	}
}

#endif
