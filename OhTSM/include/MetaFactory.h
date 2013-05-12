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
#ifndef __OHTMETAFACTORY_H__
#define __OHTMETAFACTORY_H__

#include <Ogre.h>

#include <boost/thread.hpp>

#include "OverhangTerrainPrerequisites.h"
#include "IsoSurfaceSharedTypes.h"
#include "Types.h"
#include "OverhangTerrainOptions.h"

namespace Ogre
{
	/** Factory pattern for various OhTSM classes */
	class _OverhangTerrainPluginExport MetaFactory
	{
	public:
		/// Some configuration stuff
		class Config
		{
		public:
			/// Type of normals that IsoSurfaceBuilder generates
			ComputeNormalsType normals;
			/// Ratio of space that a transition cell takes-up of a normal regular grid cell
			Real transitionCellWidthRatio;
			/// Whether or not to invert generated normals
			bool flipNormals;
			/// Flags indicating what properties are available from any given voxel (IsoVertexElements::SurfaceFlags)
			int dataGridFlags;

			Config()
				: normals(NORMAL_GRADIENT), transitionCellWidthRatio(0.5f), flipNormals(false), dataGridFlags(0) {}
		};

		/** 
		@param opts The main top-level options
		@param pManRsrcLoader A manual resource loader
		@param config An instance of the config structure previously defined
		*/
		MetaFactory (
			const OverhangTerrainOptions & opts, 
			ManualResourceLoader * pManRsrcLoader, 
			const Config & config
		);
		~MetaFactory();

		/// Creates a new 3D voxel grid / cube region at the optionally specified world coordinates relative to page
		Voxel::CubeDataRegion * createDataGrid (const Vector3 & pos = Vector3::ZERO) const;
		/** Creates a new metaball
		@param position World coordinates relative to page position
		@param radius World radius size of the ball's sphere
		@param excavating Whether or not the ball should carve-out open-space or fill in solid
		@returns A new metaball
		*/
		MetaBall * createMetaBall (const Vector3 & position = Vector3::ZERO, const Real radius = 0.0, const bool excavating = true) const;
		/** Creates a new meta fragment
		@param pt World coordinates relative to page position
		@param yl The y-level relative to terrain tile
		@returns A new meta-fragment with a new voxel grid / cube region attached to it
		*/
		MetaFragment::Container * createMetaFragment (const Vector3 & pt = Vector3::ZERO, const YLevel yl = YLevel()) const;

		/// Leverages the manual resource loader to load a named material
		MaterialPtr acquireMaterial (const std::string & sName, const std::string & sRsrcGroup) const;

		/// Retrieves the isosurface builder singleton
		inline IsoSurfaceBuilder * getIsoSurfaceBuilder () const { return _pISB; }

		/** Creates a new isosurface renderable
		@param pMWF The meta-fragment to bind the renderable to
		@param sName The mandatory OGRE name for the renderable
		@returns A new isosurface renderable courtesy of the isosurface builder singleton
		*/
		IsoSurfaceRenderable * createIsoSurfaceRenderable (MetaFragment::Container * const pMF, const String & sName) const;

	private:
		boost::mutex _mutex;

		/// The meta-information singleton that describes all voxel cube regions in the scene
		Voxel::CubeDataRegionDescriptor * _pCubeMeta;
		/// The main top-level configuration options
		OverhangTerrainOptions _options;
		/// The isosurface builder singleton
		IsoSurfaceBuilder * _pISB;
		
		/// The manual resource loader
		ManualResourceLoader * _pManRsrcLoader;
	};
}

#endif