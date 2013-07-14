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
#include "DataBase.h"

namespace Ogre
{
	/** Factory pattern for various OhTSM classes */
	class _OverhangTerrainPluginExport MetaBaseFactory
	{
	public:
		/** 
		@param opts The main top-level options
		@param pManRsrcLoader A manual resource loader
		*/
		MetaBaseFactory (
			const OverhangTerrainOptions & opts, 
			ManualResourceLoader * pManRsrcLoader
		);
		~MetaBaseFactory();

		/** Creates a new metaball
		@param position World coordinates relative to page position
		@param radius World radius size of the ball's sphere
		@param excavating Whether or not the ball should carve-out open-space or fill in solid
		@returns A new metaball
		*/
		MetaBall * createMetaBall (const Vector3 & position = Vector3::ZERO, const Real radius = 0.0, const bool excavating = true) const;

		/// Creates a database pool configured according to the specified voxel-region flags (OverhangTerrainVoxelRegionFlags)
		Voxel::DataBasePool * createDataBasePool(const size_t nVRFlags);

		/** Creates a cube data region according to the specified voxel-region flags (OverhangTerrainVoxelRegionFlags)
		@remarks Initializes the cube data region with a factory for creating database objects
		@param nVRFlags A combination of OverhangTerrainVoxelRegionFlags identifying what channels this region supports
		@param pPool The database pool factory for checking-out database objects for manipulating the cube data region with uncompressed data
		@param bbox Bounding-box in world-space coordinates relative to page for the cube data region */
		Voxel::CubeDataRegion * createCubeDataRegion (const size_t nVRFlags, Voxel::DataBasePool * pPool, const AxisAlignedBox & bbox = AxisAlignedBox::BOX_NULL);

		/// Leverages the manual resource loader to load a named material
		MaterialPtr acquireMaterial (const std::string & sName, const std::string & sRsrcGroup) const;

		/// Retrieves the isosurface builder singleton
		inline IsoSurfaceBuilder * getIsoSurfaceBuilder () const { return _pISB; }

		/// Retrieves a voxel factory for the specified channel
		const Voxel::MetaVoxelFactory * getVoxelFactory(const Channel::Ident channel) const;
		/// Retrieves a voxel factory for the specified channel
		Voxel::MetaVoxelFactory * getVoxelFactory(const Channel::Ident channel);

	private:
		boost::mutex _mutex;

		/// The meta-information singleton that describes all voxel cube regions in the scene
		Voxel::CubeDataRegionDescriptor * _pCubeMeta;
		/// The main top-level configuration options
		OverhangTerrainOptions _options;
		/// The isosurface builder singleton
		IsoSurfaceBuilder * _pISB;

		/// Index of channel-specific voxel factories
		Channel::Index< Voxel::MetaVoxelFactory, Channel::FauxFactory< Voxel::MetaVoxelFactory > > * _pVoxelFacts;
		
		/// The manual resource loader
		ManualResourceLoader * _pManRsrcLoader;
	};

	namespace Voxel
	{
		/** Factory for providing channel-specific objects such as meta-fragments and iso-surface renderables */
		class _OverhangTerrainPluginExport MetaVoxelFactory
		{
		public:
			/// The top-level factory
			MetaBaseFactory * const base;
			/// The database pool for cube data regions of this channel
			DataBasePool * const pool;
			/// Identifies the channel that this factory applies to
			const Channel::Ident channel;
			/// Combination of IsoVertexElements::SurfaceFlags used to configure iso-surface renderables of this channel
			const size_t surfaceFlags;

			/** Describes the offsets of various vertex elements in the hardware buffers
				used by renderables of this channel */
			class VertexDeclarationElements
			{
			public:
				const VertexElement
					* const position,
					* const normal,
					* const diffuse,
					* const texcoords;

				VertexDeclarationElements(
					const VertexElement * pPos,
					const VertexElement * pNorm,
					const VertexElement * pDiffuse,
					const VertexElement * pTexC
				)
					: position(pPos), normal(pNorm), diffuse(pDiffuse), texcoords(pTexC)
				{}
			};

			/**
			@param pBase The base factory singleton
			@param channel The channel that this factory applies to
			@param options The main top-level options
			*/
			MetaVoxelFactory (
				MetaBaseFactory * pBase,
				const Channel::Ident channel,
				const OverhangTerrainOptions & options
			);
			~MetaVoxelFactory();

			/// Creates a new 3D voxel grid / cube region at the optionally specified world coordinate bounding box relative to page
			Voxel::CubeDataRegion * createDataGrid (const AxisAlignedBox & bbox) const;

			/** Creates a new meta fragment
			@param bbox World coordinates relative to page position of the bounding region
			@param yl The y-level relative to terrain tile
			@returns A new meta-fragment with a new voxel grid / cube region attached to it
			*/
			MetaFragment::Container * createMetaFragment (const AxisAlignedBox & bbox = AxisAlignedBox::BOX_NULL, const YLevel yl = YLevel()) const;

			/** Creates a new isosurface renderable
			@param pMF The meta-fragment to bind the renderable to
			@param sName The mandatory OGRE name for the renderable
			@returns A new isosurface renderable courtesy of the isosurface builder singleton
			*/
			IsoSurfaceRenderable * createIsoSurfaceRenderable (MetaFragment::Container * const pMF, const String & sName) const;

			/// Returns the size of a single vertex in the hardware vertex declaration for this channel
			size_t getVertexSize() const { return _pVtxDecl->getVertexSize(0); }

			/// Returns the vertex declaration elements describing element offsets into the hardware buffers used by renderables of this channel
			const VertexDeclarationElements * getVertexDeclarationElements() const { return _pVtxDeclElems; }

		private:
			/// Top-level configuration options
			const OverhangTerrainOptions _options;
			/// Channel-specific configuration options
			const OverhangTerrainOptions::ChannelOptions _chanopts;

			/** The vertex declaration used to define the hardware buffers */
			VertexDeclaration * _pVtxDecl;

			/** The vertex declaration elements used to define the hardware buffers, compiled from the vertex declaration */
			VertexDeclarationElements * _pVtxDeclElems;
		};
	}
}

#endif