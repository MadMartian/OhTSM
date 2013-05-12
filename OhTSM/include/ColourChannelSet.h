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

#ifndef __OVERHANGTERRAINCOLOURCHANNELSET_H__
#define __OVERHANGTERRAINCOLOURCHANNELSET_H__

#include "OverhangTerrainPrerequisites.h"

#include <OgreColourValue.h>

#include <memory.h>

namespace Ogre
{
	namespace Voxel
	{
		/** Recomposes access to colour elements whose storage is distributed by colour channel
		@remarks Colours distributed by channel improves performance of RLE compression/decompression
		*/
		class _OverhangTerrainPluginExport ColourChannelSet
		{
		private:
			/// Total number of colours
			const size_t _count;

		public:
			/// Represents the red, green, blue, alpha channels of all colour elements
			unsigned char
				* const r, 
				* const g, 
				* const b, 
				* const a;

			/** 
			@remarks Colour pointer values must be pre-existing, pre-allocated, and pre-populated
					data since this class does not manage its memory and allows for both read and write
					access to these pointers.
			@param dgtmpl The cube region meta information singleton
			@param r Red colour channel
			@param g Green colour channel
			@param b Blue colour channel
			@param a Alpha colour channel
			*/
			ColourChannelSet( 
				const CubeDataRegionDescriptor & dgtmpl, 
				unsigned char * r, 
				unsigned char * g, 
				unsigned char * b, 
				unsigned char * a 
			);

			/// Recompose colour channels into a single colour element at a particular index */
			template< typename UCHAR >
			class template_Reference
			{
			private:

				friend class ColourChannelSet;

			protected:
				/// Pointers to the colour components of a single colour at an implied offset
				UCHAR
					* const r, 
					* const g, 
					* const b, 
					* const a;

				/**
				@remarks Specify pointers to the colour components of a single colour at an offset
				*/
				inline
					template_Reference(UCHAR * r, UCHAR * g, UCHAR * b, UCHAR * a)
					: r(r), g(g), b(b), a(a)
				{}

			public:
				/// Recompose from channel-distribution into a read-only colour element
				inline
					operator const ColourValue () const
				{
					ColourValue cv;
					cv.setAsRGBA((uint32(*r) << 24) | (uint32(*g) << 16) | (uint32(*b) << 8) | *a);
					return cv;
				}
			};

			typedef template_Reference< const unsigned char > const_Reference;

			/// Provides mutable access to colour channels at an offset recomposed into a single colour element
			class Reference : public template_Reference< unsigned char >
			{
			protected:
				/**
				@remarks Specify pointers to the colour components of a single colour at an offset
				*/
				inline
					Reference(unsigned char * r, unsigned char * g, unsigned char * b, unsigned char * a)
					: template_Reference(r,g,b,a) {}

				friend class ColourChannelSet;

			public:
				/// Assigns the specified colour element to the colour components individually
				inline
					Reference & operator = (const ColourValue & c)
				{
					RGBA rgba = c.getAsRGBA();

					*a = rgba & 0xFF;
					rgba >>= 8;
					*b = rgba & 0xFF;
					rgba >>= 8;
					*g = rgba & 0XFF;
					rgba >>= 8;
					*r = rgba & 0xFF;

					return *this;
				}
			};

			/// Retrieve a const reference to the colour at the specified index
			inline
				const_Reference operator [] (const size_t index) const
			{
				return const_Reference(&r[index], &g[index], &b[index], &a[index]);
			}

			/// Retrieve a mutable reference to the colour at the specified index
			inline
				Reference operator [] (const size_t index)
			{
				return Reference(&r[index], &g[index], &b[index], &a[index]);
			}

			void clear();
		};
	}
}
#endif
