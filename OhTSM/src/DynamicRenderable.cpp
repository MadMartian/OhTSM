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
		_pVertexData(new SurfaceVertexData(pVtxDecl)),
		_txWorld(Matrix4::IDENTITY),
		_pvMeshData(new MeshData * [nLODLevels])
	{
		for (unsigned c = 0; c < nLODLevels; ++c)
			_pvMeshData[c] = NULL;

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
		for (unsigned c = 0; c < _nLODCount; ++c)
			delete _pvMeshData[c];
		delete [] _pvMeshData;
		delete _pVertexData;
		HardwareBufferManager::getSingletonPtr() ->destroyVertexBufferBinding(_pVtxBB);
	}

	bool DynamicRenderable::isConfigurationBuilt( const unsigned nLOD, const size_t nStitchFlags ) const
	{
		return _pvMeshData[nLOD] != NULL && _pvMeshData[nLOD] ->indices.find(nStitchFlags) != _pvMeshData[nLOD] ->indices.end();
	}

	bool DynamicRenderable::prepareIndexBuffer( const unsigned nLOD, const size_t nStitchFlags, const size_t indexCount )
	{
		oht_assert_threadmodel(ThrMdl_Single);

		if (_renderOp.useIndexes)
		{
			if (_pvMeshData[nLOD] == NULL)
				_pvMeshData[nLOD] = new MeshData(_pVtxDecl);

			return _pvMeshData[nLOD] ->prepareIndexBuffer(nStitchFlags, indexCount);
		}
		return true;
	}

	DynamicRenderable::MeshData::Index * DynamicRenderable::getIndexData( const int nLOD, const size_t nStitchFlags )
	{
		OgreAssert(_pvMeshData[nLOD] != NULL, "Resolution doesn't exist");
		return & _pvMeshData[nLOD] ->indices[nStitchFlags];
	}

	DynamicRenderable::SurfaceVertexData * DynamicRenderable::getVertexData( )
	{
		return _pVertexData;
	}

	bool DynamicRenderable::prepareVertexBuffer( const size_t nVtxCount, bool bClearIndicesToo )
	{
		oht_assert_threadmodel(ThrMdl_Single);

		bool bPrepareResult = _pVertexData->prepare(nVtxCount);

		if (bClearIndicesToo || !bPrepareResult)
		{
			for (size_t l = 0; l < getNumLevels(); ++l)
			{
				MeshData * pMeshData = _pvMeshData[l];

				if (pMeshData != NULL)
					pMeshData->clear();
			}
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
		for (size_t l = 0; l < getNumLevels(); ++l)
		{
			MeshData * pMeshData = _pvMeshData[l];

			if (pMeshData != NULL)
				pMeshData->clear();
		}

		_pVertexData->clear();
	}

	size_t DynamicRenderable::getVertexBufferCapacity() const
	{
		return _pVertexData->getCapacity();
	}

	DynamicRenderable::MeshData::MeshData(VertexDeclaration * pVtxDecl) 
	{}

	DynamicRenderable::MeshData::MeshData( DynamicRenderable::MeshData && move ) 
		: indices(move.indices)
	{}

	DynamicRenderable::MeshData::~MeshData()
	{}

	void DynamicRenderable::MeshData::clear()
	{
		indices.clear();
	}

	bool DynamicRenderable::MeshData::prepareIndexBuffer( const size_t nStitchFlags, const size_t nIndexCount )
	{
		OgreAssert(nIndexCount <= std::numeric_limits<unsigned short>::max(), "indexCount exceeds 16 bit");

		return indices[nStitchFlags].prepare(nIndexCount);
	}

	DynamicRenderable::MeshData::Index::Index()
		: _capacity(0)
	{}

	DynamicRenderable::MeshData::Index::Index( Index && move )
		: _capacity(move._capacity)
	{
		_data.indexBuffer = move._data.indexBuffer;
		_data.indexStart = move._data.indexStart;
		_data.indexCount = move._data.indexCount;
	}

	DynamicRenderable::MeshData::Index::Index( const Index & copy )
		: _capacity(copy._capacity)
	{
		_data.indexBuffer = copy._data.indexBuffer;
		_data.indexStart = copy._data.indexStart;
		_data.indexCount = copy._data.indexCount;
	}

	DynamicRenderable::MeshData::Index & DynamicRenderable::MeshData::Index::operator = ( const Index & copy )
	{
		_data.indexBuffer = copy._data.indexBuffer;
		_data.indexCount = copy._data.indexCount;
		_data.indexStart = copy._data.indexStart;
		_capacity = copy._capacity;
		return *this;
	}

	bool DynamicRenderable::MeshData::Index::prepare( const size_t nIndexCount )
	{
		bool bPrepareResult = true;

		if ((nIndexCount > _capacity) || !_capacity)
		{
			if (!_capacity)
				_capacity = 1;

			while (_capacity < nIndexCount)
				_capacity <<= 1;

			_data.indexBuffer =
				HardwareBufferManager::getSingleton().createIndexBuffer(
					HardwareIndexBuffer::IT_16BIT,
					_capacity,
					HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY,
					DYNAMIC_RENDERABLE_SHADOW_BUFFERED
				);

			bPrepareResult = false;
		}

		_data.indexCount = nIndexCount;
		return bPrepareResult;
	}

	void DynamicRenderable::MeshData::Index::clear()
	{
		_capacity =
		_data.indexStart = 
		_data.indexCount = 0;
		_data.indexBuffer.setNull();
	}

	DynamicRenderable::MeshData::Index::~Index()
	{
	}

	DynamicRenderable::SurfaceVertexData::SurfaceVertexData(VertexDeclaration * pVtxDecl) 
		: _capacity(0), _count(0), _elemsize(pVtxDecl->getVertexSize(0)) {}

	DynamicRenderable::SurfaceVertexData::SurfaceVertexData( const SurfaceVertexData & copy )
		: _buffer(copy._buffer), _capacity(copy._capacity), _count(copy._count), _elemsize(copy._elemsize) {}

	DynamicRenderable::SurfaceVertexData::SurfaceVertexData( SurfaceVertexData && move )
		: _buffer(move._buffer), _capacity(move._capacity), _count(move._count), _elemsize(move._elemsize) {}

	DynamicRenderable::SurfaceVertexData & DynamicRenderable::SurfaceVertexData::operator = ( const SurfaceVertexData & copy )
	{
		_buffer = copy._buffer;
		_capacity = copy._capacity;
		_count = copy._count;
		_elemsize = copy._elemsize;
		return *this;
	}

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

			_buffer = 
				HardwareBufferManager::getSingleton().createVertexBuffer(
					_elemsize,
					_capacity,
					HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY,
					DYNAMIC_RENDERABLE_SHADOW_BUFFERED
				);

			bResized = true;
		}

		// Update vertex count in the render operation
		_count = nVertexCount;

		return !bResized;
	}

	void DynamicRenderable::SurfaceVertexData::clear()
	{
		_capacity = 
		_count = 0;
		_buffer.setNull();
	}

}///namespace Ogre
