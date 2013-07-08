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

#ifndef META_BALL_H
#define META_BALL_H

#include "MetaObject.h"

// Define math constants
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.707106781186547524401
#endif

#include "OgrePrerequisites.h"

namespace Ogre
{
	/// Class representing a meta ball that makes-up part of a discretely sampled 3D voxel field
	class _OverhangTerrainPluginExport MetaBall : public MetaObject
	{
	public:
		/**
		@param position The position in world coordinates of the meta-ball relative to page
		@param radius The radius in world coordinates of the meta-ball
		@param excavating Whether or not this metaball carves out open-space or fills-in open-space with solid
		*/
		MetaBall(const Vector3& position = Vector3::ZERO, Real radius = 30.0, bool excavating = true);

		/// Applies this metaball to the voxel grid as discrete samples
		virtual void updateDataGrid(const Voxel::CubeDataRegion * pDG, Voxel::DataAccessor * pAccess);
		/// Returns the meta-representation of the ball
		const Sphere & getSphere () const { return _sphere; }
		/// Returns the radius of the meta ball
		Real getRadius() const {return _sphere.getRadius(); }
		/// @returns True if the metaball carves out open-space, false if it fills it in with solid
		bool isExcavating () const { return _fExcavating > 0; }
		/// Sets the radius of the meta ball
		void setRadius(Real radius) { _sphere.setRadius(radius); }
		/// Sets the excavation status, true to carve-out open-space, false to fill it in with solid
		void setExcavating(bool e) { _fExcavating ? 1.0f : -1.0f;}
		/// Retrieve the bounding-box of the ball's sphere
		virtual AxisAlignedBox getAABB() const;

		/// Computes a spherical intersection reflecting this metaball with the specified bounding-box
		virtual void intersection(AxisAlignedBox & bbox) const;

		/// Used for serialization
		virtual MOType getObjectType () const { return MOT_MetaBall; }
		virtual void write(StreamSerialiser & output) const;
		virtual void read(StreamSerialiser & input);

	protected:
		/// The excavation flag already exhaustively explained above
		Real _fExcavating;

		/// Change the position of the metaball
		void setPosition(const Vector3 & pos);

	private:
		/// Container for the position and radius of the metaball
		Sphere _sphere;
	};

} ///namespace Ogre
#endif // META_BALL_H
