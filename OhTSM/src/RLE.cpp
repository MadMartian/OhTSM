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
#include "pch.h"

#include "RLE.h"
#include "Util.h"

namespace Ogre
{
	namespace RLE
	{
#ifdef _RLECHECK
	#pragma optimize("", off)
#endif
		void Channel::decompress (const size_t nDecompSize, unsigned char * pDest) const
		{
			register unsigned int i, j, fb, fh;

			if (_buffer == NULL)
				return;

			register unsigned char * pB = _buffer, * pD = pDest;
			const register unsigned char * pDN = &pDest[nDecompSize];

			while (pD < pDN)
			{
				j = *pB++;
				fh = j & Flag_Heterogenous;
				fb = j & Flag_Bigger;
				j ^= fb;
				i = j;

				if (fb != 0)
				{
					j = *pB++;
					fb = j & Flag_Bigger;
					j ^= fb;
					i |= j << PRECISION;

					if (fb != 0)
					{
						j = *pB++;
						fb = j & Flag_Bigger;
						j ^= fb;
						i |= j << PRECISION * 2;

						if (fb != 0)
						{
							j = *pB++;
							i |= j << PRECISION * 3;
						}
					}
				}

				i >>= 1;
				if (fh != 0)
				{
#ifdef _RLECHECK
					if (pD + i > pDN)
						throw BufferOverflowEx("Buffer overflow during decompression memcpy");
#endif
					memcpy(pD, pB, i);
					pB += i;
				} else
				{
#ifdef _RLECHECK
					if (pD + i > pDN)
						throw BufferOverflowEx("Buffer overflow during decompression memset");
#endif
					memset(pD, *pB++, i);
				}
				pD += i;
			}
		}

		void Channel::flushCounter( unsigned int & i, size_t & d, const Flags enfInverse )
		{
			using bitmanip::testZero;

			i <<= 1;
			i |= enfInverse;

			unsigned int j = i >> PRECISION;
			unsigned int nz = testZero(j) - 1;
			appendBuffer(d++, (i & ~Flag_Bigger) | (nz & Flag_Bigger));

			if (nz)
			{
				i = j;

				j >>= PRECISION;
				nz = testZero(j) - 1;
				appendBuffer(d++, (i & ~Flag_Bigger) | (nz & Flag_Bigger));

				if (nz)
				{
					i = j;

					j >>= PRECISION;
					nz = testZero(j) - 1;
					appendBuffer(d++, (i & ~Flag_Bigger) | (nz & Flag_Bigger));

					if (nz)
					{
						i = j;

						j >>= PRECISION;
						nz = testZero(j) - 1;
						appendBuffer(d++, (i & ~Flag_Bigger) | (nz & Flag_Bigger));
					}
				}
			}
			i = 0;
		}

		void Channel::compress (const size_t nDecompSize, const unsigned char * pcSrc)
		{
			unsigned int i;
			Flags enfMode = Flag_Neither;
			size_t d = 0, c = 0;

			i = 0;
			_zsize = nDecompSize / 10;
			_buffer = reinterpret_cast< unsigned char * > (realloc(_buffer, _zsize));
			while (c < nDecompSize)
			{
				const size_t
					c1 = c+1, 
					c2 = c+2;

				if ((c1 >= nDecompSize || pcSrc[c] != pcSrc[c1]) || (c2 >= nDecompSize || pcSrc[c1] != pcSrc[c2]))
				{
					if (enfMode == Flag_Homogenous && c > 0)
					{
						flushCounter(i, d, Flag_Homogenous);
						--c;
						appendBuffer(d++, pcSrc[c]);
						c += 3;
						if (c >= nDecompSize)
							--c;
					}

					enfMode = Flag_Heterogenous;
				} else
				{
					if (enfMode == Flag_Heterogenous && c > 0)
					{
						size_t nCount = i;
						flushCounter(i, d, Flag_Heterogenous);
						appendBuffer(d, &pcSrc[c - nCount], nCount);
						d += nCount;
					}	

					if (enfMode != Flag_Homogenous)
						i += 3 - 1;

					enfMode = Flag_Homogenous;
				}

				++i;
				++c;
			}
			if (c > 0)
			{
				if (enfMode == Flag_Homogenous)
				{
					flushCounter(i, d, Flag_Homogenous);
					appendBuffer(d++, pcSrc[c - 1]);
				} else
				if (enfMode == Flag_Heterogenous)
				{
					size_t nCount = i;
					flushCounter(i, d, Flag_Heterogenous);
					appendBuffer(d, &pcSrc[c - nCount], nCount);
					d += nCount;
				}	
			}

			_zsize = d;
			_buffer = reinterpret_cast< unsigned char * > (realloc(_buffer, _zsize));
		}

		Channel::Channel() 
		: _buffer(NULL), _zsize(0)
			{}

		Channel::~Channel()
		{
			free(_buffer);
		}

		StreamSerialiser & Channel::operator>>( StreamSerialiser & outs ) const
		{
			outs.write(&_zsize);
			outs.write(_buffer, _zsize);

			return outs;
		}

		StreamSerialiser & Channel::operator<<( StreamSerialiser & ins )
		{
			ins.read(&_zsize);
			_buffer = (unsigned char *)realloc(_buffer, _zsize);
			ins.read(_buffer, _zsize);

			return ins;
		}

#pragma optimize("", off)
		BufferOverflowEx::BufferOverflowEx( const char * szMsg ) 
			: std::exception(szMsg)
		{}
	}
}
