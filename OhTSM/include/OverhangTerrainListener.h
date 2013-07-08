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
#ifndef __OVERHANGTERRAINLISTENER_H__
#define __OVERHANGTERRAINLISTENER_H__

#include <OgreVector3.h>
#include <OgreColourValue.h>
#include <OgreAxisAlignedBox.h>
#include <OgreStreamSerialiser.h>

#include "OverhangTerrainPrerequisites.h"
#include "Util.h"

namespace Ogre
{
	/// Interface for serializing a custom provider-managed data object
	class _OverhangTerrainPluginExport ISerializeCustomData
	{
	public:
		virtual StreamSerialiser & operator >> (StreamSerialiser & output) const = 0;
		virtual StreamSerialiser & operator << (StreamSerialiser & input) = 0;
	};

	/// Facet for supporting a custom provider-managed data object
	class _OverhangTerrainPluginExport OverhangTerrainSupportsCustomData
	{
	public:
		ISerializeCustomData *& custom;

		OverhangTerrainSupportsCustomData(ISerializeCustomData *& pCustom);
	};

	/// Represents a 3D voxel cube that can be manipulated indirectly by a custom provider
	class _OverhangTerrainPluginExport OverhangTerrainMetaCube : public OverhangTerrainSupportsCustomData
	{
	private:
		/// Read/write access to the 3D voxel grid
		Voxel::DataAccessor * _pDataGridAccess;
		/// The 3D voxel grid itself
		Voxel::CubeDataRegion * _pDataGrid;

		/// Position of the 3D voxel cube region
		Vector3 _pos;

	public:
		// Bounding box of the region relative to the page in vertex space (OCS_Vertex)
		const int 
			vx0, vxN,
			vy0, vyN,
			vz0, vzN;

		/** 
		@param pCubeDataRegion The 3D voxel cube region
		@param pInterfUnique Read/write access to the 3D voxel grid
		@param vx0 Minimal vertex-space x-coordinate of the cube region
		@param vy0 Minimal vertex-space y-coordinate of the cube region
		@param vz0 Minimal vertex-space z-coordinate of the cube region
		@param vxN Maximal vertex-space x-coordinate of the cube region
		@param vyN Maximal vertex-space y-coordinate of the cube region
		@param vzN Maximal vertex-space z-coordinate of the cube region */
		OverhangTerrainMetaCube (
			Voxel::CubeDataRegion * pCubeDataRegion,
			MetaFragment::Interfaces::Unique * pInterfUnique, 

			const int vx0, const int vy0, const int vz0,
			const int vxN, const int vyN, const int vzN
		);
		~OverhangTerrainMetaCube();

		/// Determines if the voxel cube region provides a colour channel
		bool hasColours () const;
		/// Determines if hte voxel cube region provides a gradient field
		bool hasGradient () const;
		
		/// Retrieves the size of the cube along one axis in cells
		size_t getDimensions () const;
		
		/// Retrieve the colour channe for reading or writing
		Voxel::ColourChannelSet & getColours ();
		/// Retrieve the colour channel for reading
		const Voxel::ColourChannelSet & getColours () const;
		
		/// Retrieves the size of the cube in world units
		Vector3 getCubeSize () const;
	};

	/// Interface to an isosurface renderable for a custom provider to manipulate
	class _OverhangTerrainPluginExport OverhangTerrainRenderable : public OverhangTerrainSupportsCustomData
	{
	private:
		/// Builder facet for the meta-fragment owning the isosurface
		MetaFragment::Interfaces::Builder * _pBuilder;

	public:
		/** 
		@param pInterfBuilder Builder facet for the meta-fragment owning the isosurface */
		OverhangTerrainRenderable (MetaFragment::Interfaces::Builder * pInterfBuilder);

		/// Sets the material used by the isosurface renderable
		void setMaterial (MaterialPtr pMat);
	};

	/// The main top-level listener interface that a custom provider can receive various events on, not thread-safe
	class _OverhangTerrainPluginExport IOverhangTerrainListener
	{
	public:
		/** Called before a meta-fragment is loaded
		@remarks Implementors would override this method to allocate/construct the custom data member
		@param pOwner The page to which the meta region is bound
		@param pCube Optional access to the custom data that can be set
		@returns True to stop processing events of this type */
		virtual bool onBeforeLoadMetaRegion (const IOverhangTerrainPage * pOwner, OverhangTerrainSupportsCustomData * pCube) = 0;
		/** Called after a meta-fragment is created but before it is bound to the scene
		@remarks Implementors would override this method to configure the gradient / colours
		@param pOwner The page to which the meta region is bound
		@param pCube Provides access to manipulate the newly created meta region
		@returns True to stop processing events of this type */
		virtual bool onCreateMetaRegion (const IOverhangTerrainPage * pOwner, OverhangTerrainMetaCube * pCube) = 0;
		/** Called after a meta-fragment is created/loaded and after it has been initialized and bound to the scene
		@remarks Implementors would override this method set the renderable material
		@param pOwner The page to which the meta region is bound
		@param pCube Access to the renderable details of the meta-region
		@returns True to stop processing events of this type */
		virtual bool onInitMetaRegion (const IOverhangTerrainPage * pOwner, OverhangTerrainRenderable * pCube) = 0;
		/** Called before a meta-fragment is to be destroyed
		@remarks Implementors would override this method to deallocate/deconstruct the custom data member
		@param pOwner The page to which the meta region is bound
		@param pCube Optional access to the custom data that can be unset
		@returns True to stop processing events of this type */
		virtual bool onDestroyMetaRegion (const IOverhangTerrainPage * pOwner, OverhangTerrainSupportsCustomData * pCustom) = 0;
	};
}

#endif
