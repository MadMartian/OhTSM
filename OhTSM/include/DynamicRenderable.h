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

#ifndef DYNAMIC_RENDERABLE_H
#define DYNAMIC_RENDERABLE_H

//#include <OgreSimpleRenderable.h>
#include <OgreRenderOperation.h>
#include <OgreMatrix4.h>
#include <OgreCamera.h>

#include <map>

#include "OverhangTerrainPrerequisites.h"
#include "LODRenderable.h"
#include "Neighbor.h"

namespace Ogre
{
	/** Abstract base class providing mechanisms for dynamically growing hardware buffers. */
	class _OverhangTerrainPluginExport DynamicRenderable : public LODRenderable
	{
	public:
		/**
		@param pVtxDecl Vertex declaration to define the hardware buffers used by this renderable
		@param enRenderOpType The render operation type used by the triangle data generated for this renderable
		@param bIndices Whether indices are used by triangle data generated for this renderable
		@param nLODLevels The number of levels of detail used by this renderable for multi-resolution rendering
		@param fPixelError The maximum number of pixels allowed on the screen in error before resolution switching occurs
		@param sName Optional name for this renderable
		*/
		DynamicRenderable(
			VertexDeclaration * pVtxDecl, 
			RenderOperation::OperationType enRenderOpType, 
			bool bIndices, 
			const size_t nLODLevels, 
			const Real fPixelError, 
			const Ogre::String & sName = ""
		);
		virtual ~DynamicRenderable();

		// Determines if there is index data for the specified level of detail and stitch configuration
		bool isConfigurationBuilt (const unsigned nLOD, const size_t nStitchFlags) const;
		/// Available current storage capacity in the hardware buffer for the specified LOD
		size_t getVertexBufferCapacity(const unsigned nLOD) const;

		inline const LightList& /*SimpleRenderable::*/getLights(void) const
			{ return queryLights(); }
		void /*SimpleRenderable::*/setWorldTransform( const Matrix4& xform );
		void /*SimpleRenderable::*/getWorldTransforms( Matrix4* xform ) const;
		void /*SimpleRenderable::*/getRenderOperation(RenderOperation& op);
		void /*SimpleRenderable::*/setRenderOperation( const RenderOperation& rend );
		const String& /*SimpleRenderable::*/getMovableType(void) const;
		void /*SimpleRenderable::*/_updateRenderQueue(RenderQueue* queue);

	protected:
		/** Defines hardware buffers and properties thereof distributed by LOD and stitch configuration */
		class MeshData
		{
		private:
			MeshData (const MeshData & ); // Copy not allowed because VertexData copy is not allowed

		public:
			/** Defines triangle index data according to a stitch configuration */
			class Index
			{
			private:
				/// The IndexData object
				IndexData _data;
				/// Current capacity of the hardware index buffer
				size_t _capacity;

			public:
				Index();
				Index (const Index & copy);	// Copy does not copy the indexBuffer contained in the _data member
				Index (Index && move);
				~Index();

				/// Destroys the hardware index buffer
				virtual void clear();
				/// Ensures the specified minimum capacity (index count, not byte size) of the hardware buffer, creating a newer bigger one if necessary
				bool prepare(const size_t nIndexCount);

				/// Determines if the hardware index buffer is empty or not
				bool isEmpty () const { return _data.indexBuffer.isNull(); }
				/// Returns the IndexData object
				IndexData * getIndexData () { return &_data; }
			};
			typedef std::map< size_t, Index > IndexDataMap;

			/// Maps stitch configuration to Index objects (see above)
			IndexDataMap indices;

			/** Defines a hardware vertex buffer used by this renderable */
			class VertexData
			{
			private:
				/// The hardware vertex buffer
				HardwareVertexBufferSharedPtr _buffer;
				/// Current capacity of, element count, and element size of the hardware vertex buffer
				size_t _capacity, _count, _elemsize;

				VertexData(const VertexData &);	// Copy not allowed

			public:
				VertexData(VertexDeclaration * pVtxDecl);
				VertexData(VertexData && move);
				~VertexData();

				/// Destroys the hardware vertex buffer
				virtual void clear();
				/// Ensures minimum capacity of the hardware vertex buffer creating a newer bigger one if necessary
				bool prepare(const size_t nVertexCount);

				/// Determines if the hardware vertex buffer is empty or not
				bool isEmpty () { return _buffer.isNull(); }
				/// Current number of vertices stored in the hardware buffer
				size_t getCount() const { return _count; }
				/// Actual capacity of the hardware buffer (element count, not byte size)
				size_t getCapacity() const { return _capacity; }
				HardwareVertexBufferSharedPtr getVertexBuffer () { return _buffer; }

			} vertices;

			MeshData (MeshData && move);
			MeshData (VertexDeclaration * pVtxDecl);
			~MeshData();

			/// Ensures minimum capacity of the specified index count for the index buffer identified by the specified stitch flags
			bool prepareIndexBuffer(const size_t nStitchFlags, const size_t nIndexCount);
			
			/// Destroys all hardware buffers
			virtual void clear();
		};

	private:
		/// Initial index data used for rendering before anything else is generated and ready to render
		IndexData _indexHWData;
		/// The render operation used for all render operations on this renderable
		RenderOperation _renderOp;

		/// Stores the world transform for this renderable
		Matrix4 _txWorld;

		/// An array of MeshData object pointers indexed by LOD
		MeshData ** _pvMeshData;

		/// The vertex declaration used by all hardware buffers here
		VertexDeclaration * _pVtxDecl;

		/// The vertex buffer binding used for render operations here
		VertexBufferBinding * _pVtxBB;

	protected:
		/// Ensures minimum capacity (index count) of the hardware index buffer linked by the specified LOD and stitch configuration
		bool prepareIndexBuffer(const unsigned nLOD, const size_t nStitchFlags, const size_t indexCount);
		/** Ensures minimum capacity (vertex element count) of the hardware vertex buffer linked by the specified LOD
		@param nLOD The LOD used to retrieve the associated hardware vertex buffer
		@param nVtxCount The minimum capacity of the hardware vertex buffer even if it has to create a newer bigger buffer
		@param bClearIndicesToo Force erasure of all hardware index buffers used by the specified LOD
		*/
		bool prepareVertexBuffer(const unsigned nLOD, const size_t nVtxCount, bool bClearIndicesToo );

		/// Retrieves the index and vertex data for the specified level of detail with specified stitch configuration
		virtual std::pair< MeshData::VertexData *, MeshData::Index * > getMeshData( const int nLOD, const size_t nStitchFlags );

		/// Retrieves the hardware buffer vertex data for the specified LOD
		MeshData::VertexData * getVertexData (const unsigned nLOD);

		/// Erases all index and vertex buffers in preparation for creating brand new ones
		virtual void wipeBuffers ();
	};

}

#endif // DYNAMIC_RENDERABLE_H
