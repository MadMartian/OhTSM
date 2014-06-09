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

#ifndef _ISO_SURFACE_RENDERABLE_H_
#define _ISO_SURFACE_RENDERABLE_H_

#include "OverhangTerrainPrerequisites.h"

#include "DynamicRenderable.h"
#include "Neighbor.h"
#include "IsoVertexElements.h"
#include "IsoSurfaceSharedTypes.h"
#include "HardwareIsoVertexShadow.h"

namespace Ogre
{
	/** The OGRE renderable used for the 2-dimensional surface extracted from a voxel cube region */
	class _OverhangTerrainPluginExport IsoSurfaceRenderable : public DynamicRenderable
	{
	public:
		/** 
		@param pRendMan The render manager for synchronizing all iso-surface renderings in the viewport
		@param pVtxDecl The vertex declaration used for the mesh vertices
		@param pMWF The meta-fragment that houses this renderable (one-to-one relationship)
		@param nLODLevels The maximum quantity of detail levels supported for multi-resolution rendering
		@param fPixelError Number of pixels in error permitted before resolution switches between levels
		@param sName Optional OGRE name for the renderable
		*/
		IsoSurfaceRenderable(
			RenderManager * pRendMan,
			VertexDeclaration * pVtxDecl, 
			MetaFragment::Container * pMWF, 
			const size_t nLODLevels, 
			const Real fPixelError, 
			const Ogre::String & sName = ""
		);
		virtual ~IsoSurfaceRenderable();

		virtual const MaterialPtr& getMaterial( void ) const { return _pMaterial; }
		virtual void setMaterial(const MaterialPtr& m ) { _pMaterial = m; }
		/// @returns "IsoSurface"
		virtual const String& getMovableType(void) const { return TYPE; }

		/// Retrieves the meta-fragment "parent"
		inline
		MetaFragment::Container * getMetaWorldFragment() { return _pMWF; }
		/// Retrieves the meta-fragment "parent"
		inline
		const MetaFragment::Container * getMetaWorldFragment() const { return _pMWF; }

		/// Retrieves the precomputed meta-information and cache analogous to a system-memory-copy of the mesh data
		inline
		SharedPtr< HardwareShadow::HardwareIsoVertexShadow > getShadow() { return _pShadow; }
		/// Retrieves the precomputed meta-information and cache analogous to a system-memory-copy of the mesh data
		inline
		const SharedPtr< HardwareShadow::HardwareIsoVertexShadow > & getShadow() const { return _pShadow; }

		virtual void detachFromParent(void);

		virtual bool getNormaliseNormals(void) const { return true; }
		virtual const AxisAlignedBox &getBoundingBox(void) const { return _bbox; }
		void getRenderOperation( RenderOperation& op );
		virtual bool determineRenderState();

		/// Implementation of Ogre::SimpleRenderable
		virtual Real getBoundingRadius(void) const { return _bbox.getHalfSize().length(); }
		/// Implementation of Ogre::Renderable
		virtual Real getSquaredViewDepth(const Camera* cam) const { return mParentNode->getSquaredViewDepth(cam); }

		/// Populate the hardware buffers concurrently from a recently completed IsoSurfaceBuilder execution
		void populateBuffers( HardwareShadow::HardwareIsoVertexShadow::ConsumerLock::QueueAccess & queue );
		/** Populate the hardware buffers synchronously from a recently completed IsoSurfaceBuilder execution
		@param pVtxElems The buffer containing vertex information supplied by the IsoSurfaceBuilder
		@param direct Direct access to the shadow data
		@param bResetVertexBuffer Whether the hardware vertex buffer should be reset prior to populating (index buffer will reset as well)
		@param bResetIndexBuffer Whether the hardware index buffer should be reset prior to populating
		@param nNewVertexCount Number of new vertices to be appended to the vertex buffer
		@param nIndexCount Total number of triangle vertex indices to be flushed */
		void populateBuffers( 
			IsoVertexElements *											pVtxElems, 
			HardwareShadow::HardwareIsoVertexShadow::DirectAccess &		direct, 
			const bool													bResetVertexBuffer, 
			const bool													bResetIndexBuffer, 
			const size_t												nNewVertexCount, 
			const size_t												nIndexCount 
		);
		/// Deletes all hardware buffers and the shadow object
		void deleteGeometry();

	protected:
		virtual bool prepareVertexBuffer( const size_t vertexCount, bool bClearIndicesToo );
		virtual bool prepareIndexBuffer(const unsigned nLOD, const size_t nStitchFlags, const size_t indexCount);
		virtual void computeMinimumLevels2Distances (const Real & fErrorFactorSqr, Real * pfMinLev2DistSqr, const size_t nCount);
		virtual void setDeltaBinding (const int nLevel)
		{
			// LOD morphing not supported
		}
		/// Deletes all hardware buffers
		virtual void wipeBuffers ();

	private:
		/// The name of this thing
		static String TYPE;
		/// The render manager for synchronizing iso-surface rendering
		RenderManager * const _pRendMan;
		/// The shadow buffer and cached iso-vertex meta-data
		SharedPtr< HardwareShadow::HardwareIsoVertexShadow > _pShadow;
		/// The parent/owner MetaWorldFragment
		MetaFragment::Container * _pMWF;
		/// Current material used by this isosurface
		MaterialPtr _pMaterial;    
		/// Keeps track of the most recent render operation used and the current state
		Touch3DFlags _t3df0, _t3df;
		/// Keeps track of the most recent render operation used and the current state
		int _lod0, _lod;
		/// Keeps track of the most recent render operation used
		const ShallowMesh * _pMesh0;
		/// Index data used for each render operation
		const SurfaceIndexData::Resolution::Range * _pRange0;
		/// Last used index data buffer range
		IndexData _idxdata;

#ifdef _DISPDBG
		bool _bBoxDisplay;
#endif

		/// Bounding box of this renderable (always square)
		AxisAlignedBox _bbox;
	};
}/// namespace Ogre
#endif ///_ISO_SURFACE_RENDERABLE_H_
