/*
-----------------------------------------------------------------------------
This source file is part of the OverhangTerrainSceneManager
Plugin for OGRE
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2007 Martin Enge. Based on code from DWORD, released into public domain.
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

#ifndef META_OBJECT_H
#define META_OBJECT_H

#include <OgreStreamSerialiser.h>
#include <OgreVector3.h>
#include <OgreAxisAlignedBox.h>

#include "OverhangTerrainPrerequisites.h"
#include "Util.h"
#include "IsoSurfaceSharedTypes.h"

namespace Ogre
{
	/// Abstract class defining the interface for meta objects to be used with MetaObjectDataGrid.
	class _OverhangTerrainPluginExport MetaObject
	{
	public:
		/// Used for serialization
		enum MOType
		{
			MOT_MetaBall = 1, 
			MOT_HeightMap = 2,

			MOT_Invalid = ~0
		};

		MetaObject(const Vector3& position = Vector3::ZERO) : _pos(position) {}
		virtual ~MetaObject();

		/** Tells the meta object subclass to apply itself to the voxel grid	*/
		virtual void updateDataGrid(const Voxel::CubeDataRegion * pDG, Voxel::DataAccessor * pAccess) = 0;
		/// Returns the position of the meta object.
		const Vector3& getPosition() const {return _pos; }
		/// Sets the position of the meta object.
		void setPosition(const Vector3& position) {_pos = position; }
		/// Checks for overlap with an AABB
		virtual AxisAlignedBox getAABB() const = 0;

		/// Used for serialization
		virtual MOType getObjectType () const = 0;
		virtual void write(StreamSerialiser & output) const;
		virtual void read(StreamSerialiser & input);

		inline StreamSerialiser & operator >> (StreamSerialiser & output) const
		{
			write(output);
			return output;
		}
		inline StreamSerialiser & operator << (StreamSerialiser & input)
		{
			read(input);
			return input;
		}


	protected:
		/// Position in world coordinates relative to page of this meta object
		Vector3 _pos;
	};

	typedef std::vector<MetaObject * const> MetaObjsList;
}//namespace Ogre
#endif // META_OBJECT_H
