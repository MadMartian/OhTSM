/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org

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

Modified by Martin Enge (martin.enge@gmail.com) 2007 to fit into the OverhangTerrain Scene Manager
Modified by Jonathan Neufeld (http://www.extollit.com) 2011-2013 to implement Transvoxel
Transvoxel conceived by Eric Lengyel (http://www.terathon.com/voxels/)

-----------------------------------------------------------------------------
*/

#ifndef __OVERHANGTERRAINGENERALTYPES_H__
#define __OVERHANGTERRAINGENERALTYPES_H__

#include <limits>
#include <ostream>

#include <OgreLogManager.h>

#include "OverhangTerrainPrerequisites.h"

namespace Ogre
{
	/** Type used to specify vertical position of a meta-fragment in a terrain-tile
	@remarks A separate non-primitive type is used for this innately ordinal type to leverage the benefits of compile-time error checking and prevent
		accidental assignment to / reference from incompatible types that make no sense (e.g. assigning a world coordinate to a Y-Level). 
		The vertical world coordinate component of a meta-fragment is a multiple of its Y-level
	*/
	class YLevel
	{
	private:
		/// The ordinal value
		signed short _position;

		explicit YLevel (const signed short nNumericalRepresentation);

	public:
		static YLevel MIN, MAX;

		YLevel ();
		YLevel (YLevel && move);

		/** Constructs a Y-level that reflects the specified vertical component and cube dimension
		@param y The vertical world coordinate component
		@param fCubeDimension Dimensions of a 3D voxel cube region */
		inline static
		YLevel fromYCoord (const Real y, const Real fCubeDimension)
		{
			return YLevel((signed short)floor (y / fCubeDimension));
		}

		/// Constructs a Y-level from the specified ordinal quantity
		inline static
		YLevel fromNumber (const signed short n) { return YLevel(n); }

		/** Constructs a Y-level from the specified voxel grid cell vertical coordinate
		@remarks Determines the Y-level by rounding the voxel grid cell vertical coordinate to a multiple of Y-level, then performs division.
		@param c Voxel grid cell coordinate vertical component identifying a cell of a voxel cube whose Y-level we want to calculate
		@param nCellsPerCubeDimension Defines how many cells span a 3D voxel cube region vertically */
		inline static
		YLevel fromCell (const signed int c, const unsigned int nCellsPerCubeDimension)
		{
			signed int low = std::numeric_limits< signed int >::lowest();
			low -= low % nCellsPerCubeDimension;

			return YLevel(
				signed short(
					((unsigned int)c - low) / nCellsPerCubeDimension
					+ 
					low / nCellsPerCubeDimension
				)
			);
		}

		/** Determines the vertical world coordinate component
		@remarks Given the voxel cube dimension returns the world coordinates vertical component as a multiple of this Y-level
		@param fCubeDimension Specifies the vertical span of a 3D voxel cube region 
		@returns The vertical coordinate component as a multiple of Y-level per 3D voxel cube region */
		inline
		Real toYCoord (const Real fCubeDimension) const { return (Real)_position * fCubeDimension; }

		/// Converts this Y-level to an ordinal
		inline
		signed short toNumber () const { return _position; }

		inline YLevel & operator ++ ()
		{
			++_position;
			return *this;
		}
		inline YLevel operator ++ (int)
		{
			return YLevel(_position++);
		}

		inline YLevel operator + (const signed int n) const { return YLevel(_position + n); }
		inline YLevel operator + (const YLevel yl) const { return YLevel(_position + yl._position); }

		inline bool operator < (const YLevel yl) const { return _position < yl._position; }
		inline bool operator > (const YLevel yl) const { return _position > yl._position; }
		inline bool operator <= (const YLevel yl) const { return _position <= yl._position; }
		inline bool operator >= (const YLevel yl) const { return _position >= yl._position; }

		inline bool operator == (const YLevel yl) const { return _position == yl._position; }

		inline Ogre::Log::Stream & operator << (Ogre::Log::Stream & out) const { return write(out); }
		inline std::ostream & operator << (std::ostream & out) const { return write(out); }

	private:
		template< typename S > inline S & write (S & outs) const 
		{ return outs << "YLevel(" << _position << ')'; }

		inline unsigned long long hash() const { return _position; }

		friend signed int DIFF(const YLevel, const YLevel);
		friend Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const YLevel & yl);
		friend std::ostream & operator << (std::ostream & outs, const YLevel & yl);
	};
	inline Ogre::Log::Stream & operator << (Ogre::Log::Stream & outs, const YLevel & yl)
		{ return yl.write(outs); }
	inline std::ostream & operator << (std::ostream & outs, const YLevel & yl)
		{ return yl.write(outs); }

	/// Returns the ordinal difference between two Y-levels
	inline signed int DIFF(const YLevel a, const YLevel b)
	{
		return a._position - b._position;
	}
	inline Vector3 operator + (const Vector3 & v3, const YLevel & yl)
	{
		return v3 + Vector3(0, yl.toNumber(), 0);
	}

	typedef std::map<YLevel, MetaFragment::Container *> MetaFragMap;
	typedef std::deque< MetaFragment::Container * > MetaFragList;
	typedef std::set< MetaFragment::Container * > MetaFragSet;
}

#endif
