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

#include "pch.h"

#include "DynamicRenderable.h"
#include "OgreHardwareBufferManager.h"
#include "DebugTools.h"

#define DYNAMIC_RENDERABLE_SHADOW_BUFFERED false

namespace Ogre
{
	DynamicRenderable::DynamicRenderable(
		VertexDeclaration * pVtxDecl, 
		RenderOperation::OperationType enRenderOpType, 
		bool bIndices, 
		const size_t nLODLevels, 
		const Real fPixelError, 
		const Ogre::String & sName /*= ""*/
	)
	:	LODRenderable(nLODLevels, fPixelError, false, 0, sName),
		_pVtxDecl(pVtxDecl), _pVtxBB(HardwareBufferManager::getSingletonPtr() ->createVertexBufferBinding()),
		_pMesh(new Mesh(nLODLevels, pVtxDecl)),
		_txWorld(Matrix4::IDENTITY)
	{
		oht_assert_threadmodel(ThrMdl_Single);
		// Initialize render operation
		_renderOp.operationType = enRenderOpType;
		_renderOp.useIndexes = bIndices;
		_renderOp.vertexData = OGRE_NEW VertexData(pVtxDecl, _pVtxBB);
		_renderOp.indexData = &_indexHWData;
	}

	DynamicRenderable::~DynamicRenderable()
	{
		oht_assert_threadmodel(ThrMdl_Single);
		OGRE_DELETE _renderOp.vertexData;
		delete _pMesh;
		HardwareBufferManager::getSingletonPtr() ->destroyVertexBufferBinding(_pVtxBB);
	}

	bool DynamicRenderable::isConfigurationBuilt( const unsigned nLOD, const size_t nStitchFlags ) const
	{
		return _pMesh->indices.range(nLOD, nStitchFlags) != NULL;
	}

	bool DynamicRenderable::prepareIndexBuffer( const unsigned nLOD, const size_t nStitchFlags, const size_t indexCount )
	{
		oht_assert_threadmodel(ThrMdl_Single);

		if (_renderOp.useIndexes)
			return _pMesh->indices.prepare(nLOD, nStitchFlags, indexCount);

		return true;
	}

	bool DynamicRenderable::prepareVertexBuffer( const size_t nVtxCount, bool bClearIndicesToo )
	{
		oht_assert_threadmodel(ThrMdl_Single);

		bool bPrepareResult = _pMesh->vertices.prepare(nVtxCount);

		if (bClearIndicesToo || !bPrepareResult)
		{
			_pMesh->indices.reset();
		}

		return bPrepareResult && !bClearIndicesToo;
	}

	void /*SimpleRenderable::*/ DynamicRenderable::getWorldTransforms( Matrix4* xform ) const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		*xform = _txWorld * mParentNode->_getFullTransform();
	}

	void /*SimpleRenderable::*/ DynamicRenderable::setWorldTransform( const Matrix4& xform )
	{
		oht_assert_threadmodel(ThrMdl_Single);
		_txWorld = xform;
	}

	void /*SimpleRenderable::*/ DynamicRenderable::getRenderOperation( RenderOperation& op )
	{
		oht_assert_threadmodel(ThrMdl_Single);
		op = _renderOp;
		OHT_DBGTRACE("Build RenderOp for " << this);
	}

	void /*SimpleRenderable::*/ DynamicRenderable::setRenderOperation( const RenderOperation& rend )
	{
		oht_assert_threadmodel(ThrMdl_Single);
		_renderOp = rend;
	}

	const String& /*SimpleRenderable::*/ DynamicRenderable::getMovableType( void ) const
	{
		oht_assert_threadmodel(ThrMdl_Single);
		static String movType = "DynamicRenderable";
		return movType;
	}

	void /*SimpleRenderable::*/ DynamicRenderable::_updateRenderQueue( RenderQueue* queue )
	{
		oht_assert_threadmodel(ThrMdl_Single);
		queue->addRenderable( this, mRenderQueueID, OGRE_RENDERABLE_DEFAULT_PRIORITY);
	}

	void DynamicRenderable::wipeBuffers()
	{
		oht_assert_threadmodel(ThrMdl_Single);
		// TODO: Is this necessary now that we employ latent render operations?
		_renderOp.vertexData->vertexBufferBinding->unsetAllBindings();
		_pMesh->clear();
	}

	size_t DynamicRenderable::getVertexBufferCapacity() const
	{
		return _pMesh->vertices.getCapacity();
	}

	DynamicRenderable::Mesh * DynamicRenderable::getMesh()
	{
		return _pMesh;
	}

	DynamicRenderable::SurfaceVertexData::SurfaceVertexData(VertexDeclaration * pVtxDecl) 
		: _capacity(0), _count(0), _elemsize(pVtxDecl->getVertexSize(0)), _bReferenced(false) {}

	DynamicRenderable::SurfaceVertexData::SurfaceVertexData( const SurfaceVertexData & copy )
		: _buffer(copy._buffer), _capacity(copy._capacity), _count(copy._count), _elemsize(copy._elemsize), _bReferenced(true) {}

	DynamicRenderable::SurfaceVertexData::SurfaceVertexData( SurfaceVertexData && move )
		: _buffer(move._buffer), _capacity(move._capacity), _count(move._count), _elemsize(move._elemsize), _bReferenced(move._bReferenced) {}

	DynamicRenderable::SurfaceVertexData::~SurfaceVertexData() {}

	bool DynamicRenderable::SurfaceVertexData::prepare( const size_t nVertexCount )
	{
		bool bResized = false;

		if ((nVertexCount > _capacity) || !_capacity)
		{
			if (!_capacity)
				_capacity = 1;

			while (_capacity < nVertexCount)
				_capacity <<= 1;

			rebuildHWBuffer();

			bResized = true;
		} else if (_buffer.isNull())
		{
			rebuildHWBuffer();
		}

		// Update vertex count in the render operation
		_count = nVertexCount;

		return !bResized;
	}

	void DynamicRenderable::SurfaceVertexData::reset()
	{
		_count = 0;

		if (_bReferenced)
		{
			_buffer.setNull();
			_bReferenced = false;
		}
	}

	void DynamicRenderable::SurfaceVertexData::clear()
	{
		reset();
		_capacity = 0;
		_buffer.setNull();
	}

	const DynamicRenderable::SurfaceVertexData * DynamicRenderable::SurfaceVertexData::shallowCopy() const
	{
		_bReferenced = true;
		return new SurfaceVertexData(*this);
	}

	void DynamicRenderable::SurfaceVertexData::rebuildHWBuffer()
	{
		_buffer = 
			HardwareBufferManager::getSingleton().createVertexBuffer(
				_elemsize,
				_capacity,
				HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY,
				DYNAMIC_RENDERABLE_SHADOW_BUFFERED
			);
	}

	DynamicRenderable::SurfaceIndexData::Resolution::Resolution(VertexDeclaration * pVtxDecl) 
	{}

	DynamicRenderable::SurfaceIndexData::Resolution::Resolution( DynamicRenderable::SurfaceIndexData::Resolution && move ) 
		: _indices(static_cast< RangeMap && > (move._indices))
	{}

	DynamicRenderable::SurfaceIndexData::Resolution::Resolution( const Resolution & copy )
		: _indices(copy._indices)
	{

	}

	DynamicRenderable::SurfaceIndexData::Resolution::~Resolution()
	{
		clear();
	}

	void DynamicRenderable::SurfaceIndexData::Resolution::clear()
	{
		_indices.clear();
	}

	DynamicRenderable::SurfaceIndexData::Resolution::Range * DynamicRenderable::SurfaceIndexData::Resolution::operator[]( const size_t stitches )
	{
		RangeMap::iterator i = _indices.find(stitches);
		return i == _indices.end() ? NULL : &i->second;
	}

	const DynamicRenderable::SurfaceIndexData::Resolution::Range * DynamicRenderable::SurfaceIndexData::Resolution::operator[]( const size_t stitches ) const
	{
		RangeMap::const_iterator i = _indices.find(stitches);
		return i == _indices.end() ? NULL : &i->second;
	}

	void DynamicRenderable::SurfaceIndexData::Resolution::insert
	( 
		const size_t stitches, 
		const size_t nOffset, 
		const size_t nCount
	)
	{
		_indices[stitches] = Range(nOffset, nCount);
	}

	bool DynamicRenderable::SurfaceIndexData::prepare( const size_t nIndexCount )
	{
		bool bPrepareResult = true;
		unsigned nNewRequiredSize = _count + nIndexCount;

		if ((nNewRequiredSize > _capacity) || !_capacity)
		{
			if (!_capacity)
				_capacity = 1;

			while (_capacity < nNewRequiredSize)
				_capacity <<= 1;

			rebuildHWBuffer();

			bPrepareResult = false;

			reset();
		} else if (_buffer.isNull())
		{
			rebuildHWBuffer();
		}

		_count = nNewRequiredSize;

		return bPrepareResult;
	}

	bool DynamicRenderable::SurfaceIndexData::prepare( const char lod, const size_t nStitchFlags, const size_t nIndexCount )
	{
		Resolution & res = *_pvResolutions[lod];
		Resolution::Range * pRange = res[nStitchFlags];

		if (pRange != NULL)
			return true;
		else
		{
			bool bResult = prepare(nIndexCount);
			res.insert(nStitchFlags, _count - nIndexCount, nIndexCount);
			return bResult;
		}
	}

	void DynamicRenderable::SurfaceIndexData::reset()
	{
		for (unsigned i = 0; i < _nResCount; ++i)
		{
			_pvResolutions[i] ->clear();
		}
		_count = 0;

		if (_bReferenced)
		{
			_buffer.setNull();
			_bReferenced = false;
		}
	}

	void DynamicRenderable::SurfaceIndexData::clear()
	{
		reset();
		_buffer.setNull();
		_capacity = 0;
	}

	DynamicRenderable::SurfaceIndexData::SurfaceIndexData( const unsigned nResolutionCount, VertexDeclaration * pVtxDecl ) 
		: _nResCount(nResolutionCount), _pvResolutions(new Resolution * [nResolutionCount]), 
		  _capacity(0), _count(0), _bReferenced(false)
	{
		for (unsigned c = 0; c < nResolutionCount; ++c)
			_pvResolutions[c] = new Resolution(pVtxDecl);
	}

	DynamicRenderable::SurfaceIndexData::SurfaceIndexData( const SurfaceIndexData & copy ) 
	:	_pvResolutions( new Resolution * [copy._nResCount]),
		_buffer(copy._buffer),
		_capacity(copy._capacity),
		_count(copy._count),
		_nResCount(copy._nResCount),
		_bReferenced(true)
	{
		for (unsigned i = 0; i < _nResCount; ++i)
			_pvResolutions[i] = new Resolution(*copy._pvResolutions[i]);
	}

	DynamicRenderable::SurfaceIndexData::SurfaceIndexData( SurfaceIndexData && move )
	:	_pvResolutions( move._pvResolutions ),
		_buffer(move._buffer),
		_capacity(move._capacity),
		_count(move._count),
		_nResCount(move._nResCount),
		_bReferenced(move._bReferenced)
	{
		move._pvResolutions = NULL;
	}

	DynamicRenderable::SurfaceIndexData::~SurfaceIndexData()
	{
		if (_pvResolutions != NULL)
		{
			for (unsigned c = 0; c < _nResCount; ++c)
				delete _pvResolutions[c];

			delete [] _pvResolutions;
		}
	}

	DynamicRenderable::SurfaceIndexData::Resolution::Range * DynamicRenderable::SurfaceIndexData::range( const char lod, const size_t stitches )
	{
		Resolution & res = *_pvResolutions[lod];
		return res[stitches];
	}
	
	const DynamicRenderable::SurfaceIndexData::Resolution::Range * DynamicRenderable::SurfaceIndexData::range( const char lod, const size_t stitches ) const
	{
		const Resolution & res = *_pvResolutions[lod];
		return res[stitches];
	}

	const DynamicRenderable::SurfaceIndexData * DynamicRenderable::SurfaceIndexData::shallowCopy() const
	{
		_bReferenced = true;
		return new SurfaceIndexData(*this);
	}

	void DynamicRenderable::SurfaceIndexData::rebuildHWBuffer()
	{
		_buffer =
			HardwareBufferManager::getSingleton().createIndexBuffer(
				HardwareIndexBuffer::IT_16BIT,
				_capacity,
				HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY,
				DYNAMIC_RENDERABLE_SHADOW_BUFFERED
			);
	}


	DynamicRenderable::SurfaceIndexData::Resolution::Range::Range( const size_t offset, const size_t length ) 
		: offset(offset), length(length)
	{

	}

	DynamicRenderable::SurfaceIndexData::Resolution::Range::Range()
	{

	}


	DynamicRenderable::Mesh::Mesh( const unsigned nResolutionCount, VertexDeclaration * pVtxDecl ) 
		: vertices(pVtxDecl), indices(nResolutionCount, pVtxDecl)
	{}

	void DynamicRenderable::Mesh::clear()
	{
		vertices.clear();
		indices.clear();
	}

	const DynamicRenderable::ShallowMesh * DynamicRenderable::Mesh::shallowCopy() const
	{
		return new ShallowMesh(
			vertices.shallowCopy(),
			indices.shallowCopy()
		);
	}

	DynamicRenderable::ShallowMesh::ShallowMesh( const SurfaceVertexData * pVertices, const SurfaceIndexData * pIndices )
		: vertices(pVertices), indices(pIndices)
	{}
	DynamicRenderable::ShallowMesh::~ShallowMesh()
	{
		delete vertices;
		delete indices;
	}

}///namespace Ogre
