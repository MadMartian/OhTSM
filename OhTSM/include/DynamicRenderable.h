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
		/// Available current storage capacity in the hardware buffer
		size_t getVertexBufferCapacity() const;

		inline const LightList& /*SimpleRenderable::*/getLights(void) const
			{ return queryLights(); }
		void /*SimpleRenderable::*/setWorldTransform( const Matrix4& xform );
		void /*SimpleRenderable::*/getWorldTransforms( Matrix4* xform ) const;
		void /*SimpleRenderable::*/getRenderOperation(RenderOperation& op);
		void /*SimpleRenderable::*/setRenderOperation( const RenderOperation& rend );
		const String& /*SimpleRenderable::*/getMovableType(void) const;
		void /*SimpleRenderable::*/_updateRenderQueue(RenderQueue* queue);

	protected:
		/// Top-level mesh index list container that manages the hardware buffer and relationship with the individual triangle list configurations
		class SurfaceIndexData
		{
		public:
			/** Defines the ranges within the index hardware buffer that each correspond to a stitch configuration */
			class Resolution
			{
			public:
				/** Simple object defining a range starting from an offset and proceeding for a length */
				class Range 
				{
				public:
					size_t offset, length;

					Range();
					Range(const size_t offset, const size_t length);
				};

			private:
				typedef std::map< size_t, Range > RangeMap;

				/// The ranges within the index hardware buffer that each correspond to a stitch configuration
				RangeMap _indices;

			public:
				Resolution (const Resolution & copy);
				Resolution (Resolution && move);
				Resolution (VertexDeclaration * pVtxDecl);
				~Resolution();

				/// Wipes the IndexData map clean
				void clear();

				/// Returns the range for the specified stitches, returns NULL if there is no such range defined yet
				const Range * operator [] (const size_t stitches) const;

				/// Returns the range for the specified stitches, returns NULL if there is no such range defined yet
				Range * operator [] (const size_t stitches);

				/// Defines a range in the map according to the specified stitches
				void insert(const size_t stitches, const size_t nOffset, const size_t nCount);
			};

		private:
			/// Array of index list information distributed by LOD
			Resolution ** _pvResolutions;
			/// The hardware index buffer
			HardwareIndexBufferSharedPtr _buffer;
			/// Current capacity of, and element count of the hardware index buffer
			size_t _capacity, _count;
			/// Resolution count
			unsigned int _nResCount;
			/// Ensures the hardware index buffer meets the minimum size, if not then the buffer is resized resulting in a subsequent call to reset()
			bool prepare (const size_t nIndexCount);
			/// Tracks whether this object has endured a shallow copy
			mutable bool _bReferenced;

			/**
			 * Shallow copy constructor, not permitted by outside contract subscribers.
			 * @remarks This does not copy the hardware index buffer, only the reference is copied.
			 */
			SurfaceIndexData (const SurfaceIndexData & copy);

		public:
			SurfaceIndexData (const unsigned nResolutionCount, VertexDeclaration * pVtxDecl);
			SurfaceIndexData (SurfaceIndexData && move);
			~SurfaceIndexData();

			/**
			 * Performs a shallow copy of this object and returns a const shallow copy that may not be modified.  The way the 
			 * algorithm works is that the bounded range of the buffer that is referenced by software is defined in software 
			 * and is copied, and only additions appended to the buffer outside the bounded range defined by software are
			 * permitted (alterations to ranges that fall within the software range are not allowed).  There is one exception,
			 * in the case of a reset, the bounded range of the hardware buffer can be reused.  At that point the buffer belonging
			 * to this object will be recreated with the same size as the previous one so that the copy will not be affected. 
			 * This only occurs if this object has endured a shallow copy. */
			const SurfaceIndexData * shallowCopy() const;

			// Acquires the range of the index buffer designated for the specified resolution and configuration, if it does not exist the return value is NULL.
			Resolution::Range * range (const char lod, const size_t stitches);
			// Acquires the range of the index buffer designated for the specified resolution and configuration, if it does not exist the return value is NULL.
			const Resolution::Range * range (const char lod, const size_t stitches) const;

			/// Determines if the hardware index buffer is empty or not
			bool isEmpty () { return _buffer.isNull(); }
			/// Current number of indices stored in the hardware buffer
			size_t getCount() const { return _count; }
			/// Actual capacity of the hardware buffer (element count, not byte size)
			size_t getCapacity() const { return _capacity; }
			/// Retrieve the hardware index buffer
			HardwareIndexBufferSharedPtr getIndexBuffer () { return _buffer; }
			/// Retrieve the hardware index buffer
			const HardwareIndexBufferSharedPtr getIndexBuffer () const { return _buffer; }
			/// Resets the software state for all these resolutions
			virtual void reset ();
			/// Destroys the index hardware buffer
			virtual void clear();
		
			/* Prepare a slot/slice in the hardware buffer for the specified resolution configuration unless it already exists 
			@return True if there was room or the configuration already exists, false if the hardware buffer had to be resized */
			bool prepare (const char lod, const size_t nStitchFlags, const size_t nIndexCount);

			void rebuildHWBuffer();

		};

		/** Defines a hardware vertex buffer used by this renderable */
		class SurfaceVertexData
		{
		private:
			/// The hardware vertex buffer
			HardwareVertexBufferSharedPtr _buffer;
			/// Current capacity of, element count, and element size of the hardware vertex buffer
			size_t _capacity, _count, _elemsize;
			/// Tracks whether this object has endured a shallow copy
			mutable bool _bReferenced;

			/**
			 * Shallow copy constructor, not permitted by outside contract subscribers.
			 * @remarks This does not copy the hardware vertex buffer, only the reference is copied. */
			SurfaceVertexData(const SurfaceVertexData & copy); 

		public:
			SurfaceVertexData(VertexDeclaration * pVtxDecl);
			/**
			 * Pseudo-copy constructor.  
			 * @remarks This does not copy the hardware vertex buffer, only the reference is copied.  However, the way the 
			 * algorithm works is that the bounded range of the buffer that is referenced by software is defined in software 
			 * and is copied, and only additions appended to the buffer outside the bounded range defined by software are
			 * permitted (alterations to ranges that fall within the software range are not allowed).  However it should 
			 * be noted that it isn't thread-safe, but then hardware buffers aren't thread safe anyway.
			 */
			SurfaceVertexData(SurfaceVertexData && move);
			~SurfaceVertexData();

			/* Performs a shallow copy of this object and returns a const shallow copy that may not be modified.  The way the 
			* algorithm works is that the bounded range of the buffer that is referenced by software is defined in software 
			* and is copied, and only additions appended to the buffer outside the bounded range defined by software are
			* permitted (alterations to ranges that fall within the software range are not allowed).  There is one exception,
			* in the case of a reset, the bounded range of the hardware buffer can be reused.  At that point the buffer belonging
			* to this object will be recreated with the same size as the previous one so that the copy will not be affected. 
			* This only occurs if this object has endured a shallow copy. */
			const SurfaceVertexData * shallowCopy() const;

			/// Resets the software state of the hardware buffer
			virtual void reset();
			/// Destroys the hardware vertex buffer
			virtual void clear();
			/// Ensures minimum capacity of the hardware vertex buffer creating a newer bigger one if necessary
			bool prepare(const size_t nVertexCount);

			void rebuildHWBuffer();

			/// Determines if the hardware vertex buffer is empty or not
			bool isEmpty () { return _buffer.isNull(); }
			/// Current number of vertices stored in the hardware buffer
			size_t getCount() const { return _count; }
			/// Actual capacity of the hardware buffer (element count, not byte size)
			size_t getCapacity() const { return _capacity; }
			/// Retrieve the hardware vertex buffer
			HardwareVertexBufferSharedPtr getVertexBuffer () { return _buffer; }
			/// Retrieve the hardware vertex buffer
			const HardwareVertexBufferSharedPtr getVertexBuffer () const { return _buffer; }
		};

		/// A shallow copy of the original mesh
		class ShallowMesh
		{
		public:
			const SurfaceVertexData * vertices;
			const SurfaceIndexData * indices;

			/** Initializes the class members passing pointer ownership to this object.  
			 * @remarks Caller must not delete these pointers. */
			ShallowMesh (const SurfaceVertexData * pVertices, const SurfaceIndexData * pIndices);
			~ShallowMesh();
		};

		/** Aggregates the combined data pertinent to the mesh (indices and vertices) */
		class Mesh
		{
		private:
			/// Deep copy not permitted
			Mesh (const Mesh &);

		public:
			SurfaceVertexData vertices;
			SurfaceIndexData indices;

			Mesh(const unsigned nResolutionCount, VertexDeclaration * pVtxDecl);

			/// Clears both vertices and indices, the entire software state for the mesh
			void clear();

			/// Performs a shallow copy of this object, see the documentation for the corresponding method of each of the members
			const ShallowMesh * shallowCopy () const;
		};

	private:
		/// Initial index data used for rendering before anything else is generated and ready to render
		IndexData _indexHWData;
		/// The render operation used for all render operations on this renderable
		RenderOperation _renderOp;

		/// Stores the world transform for this renderable
		Matrix4 _txWorld;

		/// The vertex and index data that make-up the surface and all of its resolutions and resolution configurations
		Mesh * _pMesh;

		/// The vertex declaration used by all hardware buffers here
		VertexDeclaration * const _pVtxDecl;

		/// The vertex buffer binding used for render operations here
		VertexBufferBinding * const _pVtxBB;

	protected:
		inline VertexDeclaration * vertexDeclaration() const { return _pVtxDecl; }
		inline VertexBufferBinding * vertexBufferBinding() const { return _pVtxBB; }

		/// Ensures minimum capacity (index count) of the hardware index buffer linked by the specified LOD and stitch configuration
		virtual bool prepareIndexBuffer(const unsigned nLOD, const size_t nStitchFlags, const size_t indexCount);
		/** Ensures minimum capacity (vertex element count) of the hardware vertex buffer
		@param nVtxCount The minimum capacity of the hardware vertex buffer even if it has to create a newer bigger buffer
		@param bClearIndicesToo Force erasure of all hardware index buffers used by the specified LOD
		*/
		virtual bool prepareVertexBuffer(const size_t nVtxCount, bool bClearIndicesToo );

		/// Retrieves the mesh data
		virtual Mesh * getMesh ();

		/// Erases all index and vertex buffers in preparation for creating brand new ones
		virtual void wipeBuffers ();
	};

}

#endif // DYNAMIC_RENDERABLE_H
