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

#include <strstream>
#include <stack>
#include <hash_set>

#include "IsoSurfaceBuilder.h"
#include "IsoSurfaceRenderable.h"
#include "MetaWorldFragment.h"

#include "DebugTools.h"

namespace Ogre
{
	using namespace Voxel;
	using namespace HardwareShadow;
	
#if defined(_DEBUG) || defined(_OHT_LOG_TRACE)

	Ogre::Vector3 IsoSurfaceBuilder::getWorldPos( const IsoVertexIndex nIsoVertex ) const
	{
		return getWorldPos(_pMainVtxElems->positions[nIsoVertex]);
	}
	Ogre::Vector3 IsoSurfaceBuilder::getWorldPos( const IsoFixVec3 & fv ) const
	{
		return Vector3(fv) * _cubemeta.scale + _debugs.center;
	}
	
	String IsoSurfaceBuilder::drawGridCell( const size_t nCornerFlags, const int nIndent /*= 1 */ ) const
	{
		std::strstream ss;
		std::string sIndent (nIndent, '\t');
		char cC[8];

		for (int c = 0; c < 8; ++c)
			cC[c] = (~nCornerFlags & (1 << c) ? '.' : 'o');

/*		Draws a box like this:
			  o---o
			 /|  /|
			o---. |
			| .-|-o
			|/  |/
			.---.
		with corners shaded according to the corner flags
*/ 
		ss 
			<< std::endl << sIndent << "Flags=" << nCornerFlags
			<< std::endl << sIndent << "  " << cC[6] << "---" << cC[7]
			<< std::endl << sIndent << " /|  /|"
			<< std::endl << sIndent << "" << cC[4] << "---" << cC[5] << " |"
			<< std::endl << sIndent << "| " << cC[2] << "-|-" << cC[3]
			<< std::endl << sIndent << "|/  |/"
			<< std::endl << sIndent << "" << cC[0] << "---" << cC[1]
			<< std::ends
		;

		return ss.str();
	}

	String IsoSurfaceBuilder::drawTransitionCell( const size_t nCornerFlags, const int nIndent /*= 1 */ ) const
	{
		std::strstream ss;
		std::string sIndent (nIndent, '\t');
		char cC[9];

		for (int c = 0; c < 9; ++c)
			cC[c] = (~nCornerFlags & (1 << c) ? '.' : 'o');

/*		Draws a box like this:
			.---o---.
			|   |   |
			o---.---.
			|   |   |
			o---o---o
		with corners shaded according to the corner flags
*/ 
		ss 
			<< std::endl << sIndent << "Flags=" << std::hex << nCornerFlags
			<< std::endl << sIndent << cC[6] << "---" << cC[5] << "---" << cC[4]
			<< std::endl << sIndent << "|   |   |"
			<< std::endl << sIndent << cC[7] << "---" << cC[8] << "---" << cC[3]
			<< std::endl << sIndent << "|   |   |"
			<< std::endl << sIndent << cC[0] << "---" << cC[1] << "---" << cC[2]
			<< std::ends
		;

		return ss.str();
	}

#endif

	const size_t
		IsoSurfaceBuilder::_triwindflags = literal::B< 101100 >::_;

	const IsoSurfaceBuilder::TransitionCell::XY 
		IsoSurfaceBuilder::TransitionCell::_matci2tcc[13] = 
	{
		{ 0, 0 },
		{ 1, 0 },
		{ 2, 0 },
		{ 0, 1 },
		{ 1, 1 },
		{ 2, 1 },
		{ 0, 2 },
		{ 1, 2 },
		{ 2, 2 },
		{ 0, 0 },
		{ 2, 0 },
		{ 0, 2 },
		{ 2, 2 }
	},
		IsoSurfaceBuilder::TransitionCell::_matcfi2tcc[9] =
	{
		{ 0, 0 },
		{ 1, 0 },
		{ 2, 0 },
		{ 2, 1 },
		{ 2, 2 },
		{ 1, 2 },
		{ 0, 2 },
		{ 0, 1 },
		{ 1, 1 }
	};

	IsoSurfaceBuilder::TransitionCell::TransitionCell( const TransitionCell & copy ) : _cubemeta(copy._cubemeta), halfLOD(copy.halfLOD), fullLOD(copy.fullLOD), x(copy.x), y(copy.y), side(copy.side), _lastcell(copy._lastcell)
	{
		corners.setParent(this);
		flags.setParent(this);
	}

	IsoSurfaceBuilder::TransitionCell::TransitionCell( const Voxel::CubeDataRegionDescriptor & cubemeta, const unsigned short nLOD, OrthogonalNeighbor on ) : _cubemeta(cubemeta), halfLOD(nLOD), fullLOD(nLOD - 1), x(0), y(0), side(on), _lastcell(cubemeta.dimensions - 1)
	{
		corners.setParent(this);
		flags.setParent(this);
	}

	IsoSurfaceBuilder::TransitionCell::TransitionCell( const Voxel::CubeDataRegionDescriptor & cubemeta, const unsigned short nLOD, const DimensionType x, const DimensionType y, const OrthogonalNeighbor on ) : _cubemeta(cubemeta), halfLOD(nLOD), fullLOD(nLOD - 1), x(x), y(y), side(on), _lastcell(cubemeta.dimensions - 1)
	{
		corners.setParent(this);
		flags.setParent(this);
	}

	IsoSurfaceBuilder::IsoSurfaceBuilder(
		const CubeDataRegionDescriptor & cubemeta,
		const Channel::Index< ChannelParameters > & chanparams
	)
	: 	_cubemeta(cubemeta),
		_chanparams(chanparams),
		_vRegularCases(new unsigned char [cubemeta.cellcount]),
		_pCurrentChannelParams(NULL)
	{
		oht_assert_threadmodel(ThrMdl_Main);

		_pMainVtxElems = new MainVertexElements(cubemeta);

		for (unsigned c = 0; c < CountOrthogonalNeighbors; ++c)
			_vvTrCase[c] = new unsigned short[cubemeta.sidecellcount];

		WorkQueue * pWorkQ = Root::getSingleton().getWorkQueue();
		_nWorkQChannID = pWorkQ->getChannel("OhTSM/IsoSurfaceBuilder");
		pWorkQ->addRequestHandler(_nWorkQChannID, this);
		pWorkQ->addResponseHandler(_nWorkQChannID, this);
	}

	IsoSurfaceBuilder::ChannelParameters::TransitionCellTranslators * IsoSurfaceBuilder::ChannelParameters::createTransitionCellTranslators( const unsigned short nLODCount, const Real fTCWidthRatio )
	{
		TransitionCellTranslators * txTCHalf2Full = new TransitionCellTranslators[nLODCount];
		for (unsigned l = 0; l < nLODCount; ++l)
		{
			for (unsigned s = 0; s < CountTouch3DSides; ++s)
			{
				txTCHalf2Full[l].side[s] = IsoFixVec3(
					((s & T3DS_West) ? +fTCWidthRatio : 0) + 
					((s & T3DS_East) ? -fTCWidthRatio : 0),

					((s & T3DS_Nether) ? +fTCWidthRatio : 0) + 
					((s & T3DS_Aether) ? -fTCWidthRatio : 0),

					((s & T3DS_North) ? +fTCWidthRatio : 0) + 
					((s & T3DS_South) ? -fTCWidthRatio : 0)
				) * short(1 << l);
			}
		}
		return txTCHalf2Full;
	}

	IsoSurfaceBuilder::ChannelParameters::ChannelParameters(
		const Real fTCWidthRatio,
		const size_t nSurfaceFlags,
		const unsigned short nLODCount,
		const Real fMaxPixelError,
		const bool bFlipNormals,
		const NormalsType enNormalType
	)
		:	_txTCHalf2Full(createTransitionCellTranslators(nLODCount, fTCWidthRatio)),
			surfaceFlags(nSurfaceFlags),
			clod(nLODCount),
			maxPixelError(fMaxPixelError),
			flipNormals(bFlipNormals),
			normalsType(enNormalType)
	{
		OgreAssert(fTCWidthRatio <= 1.0f && fTCWidthRatio >= 0.0f, "Width ratio was out of bounds");
	}

	IsoSurfaceBuilder::ChannelParameters::~ChannelParameters()
	{
		delete [] _txTCHalf2Full;
	}

	IsoSurfaceBuilder::~IsoSurfaceBuilder()
	{
		{ OGRE_LOCK_MUTEX(mMutex);
			oht_assert_threadmodel(ThrMdl_Main);

			delete [] _vRegularCases;
			for (unsigned c = 0; c < CountOrthogonalNeighbors; ++c)
				delete [] _vvTrCase[c];
			delete _pMainVtxElems;

		}
	}

	size_t IsoSurfaceBuilder::genSurfaceFlags( const OverhangTerrainOptions::ChannelOptions & chanopts )
	{
		return
			(chanopts.normals != NT_None ? IsoVertexElements::GEN_NORMALS : 0) |
			(chanopts.voxelRegionFlags & VRF_Colours ? IsoVertexElements::GEN_VERTEX_COLOURS : 0) |
			(chanopts.voxelRegionFlags & VRF_TexCoords ? IsoVertexElements::GEN_TEX_COORDS : 0);
	}

	WorkQueue::Response* IsoSurfaceBuilder::handleRequest( const WorkQueue::Request* req, const WorkQueue* srcQ )
	{
		switch (req->getType())
		{
		case RT_BuildSurface:
			{ OGRE_LOCK_MUTEX(mMutex);
				oht_assert_threadmodel(ThrMdl_Background);

				BuildSurfaceTaskData reqdata = any_cast< BuildSurfaceTaskData > (req->getData());
				HardwareIsoVertexShadow::ProducerQueueAccess queue = reqdata.shadow->requestProducerQueue(reqdata.lod, reqdata.stitches);

				buildImpl(
#if defined(_DEBUG) || defined(_OHT_LOG_TRACE)
					reqdata.debugs,
#endif // _DEBUG
					reqdata.channel,
					queue.resolution, reqdata.cubedata, reqdata.shadow, reqdata.surfaceFlags, reqdata.stitches, reqdata.vertexBufferCapacity
				);
				fillShadowQueues(queue, reqdata.cubedata->getGridScale());
			}
			break;
		}

		return new WorkQueue::Response(req, true, Any());
	}

	void IsoSurfaceBuilder::handleResponse( const WorkQueue::Response* res, const WorkQueue* srcQ ) 
	{
	}

	WorkQueue::RequestID IsoSurfaceBuilder::requestBuild( const Voxel::CubeDataRegion * pDataGrid, IsoSurfaceRenderable * pISR, const unsigned nLOD, const Touch3DFlags enStitches )
	{
		oht_assert_threadmodel(ThrMdl_Main);
		BuildSurfaceTaskData reqdata;

		reqdata.cubedata = pDataGrid;
#if defined(_DEBUG) || defined(_OHT_LOG_TRACE)
		reqdata.debugs = DebugInfo(pISR);
#endif // _DEBUG
		reqdata.shadow = pISR->getShadow();
		reqdata.channel = pISR->getMetaWorldFragment() ->factory->channel;
		reqdata.lod = nLOD;
		reqdata.surfaceFlags = pISR->getMetaWorldFragment() ->factory->surfaceFlags;
		reqdata.stitches = enStitches;
		reqdata.vertexBufferCapacity = pISR->getVertexBufferCapacity(nLOD);
		return Root::getSingleton().getWorkQueue()->addRequest(_nWorkQChannID, RT_BuildSurface, Any(reqdata));
	}

	void IsoSurfaceBuilder::cancelBuild( const WorkQueue::RequestID & rid )
	{
		Root::getSingleton().getWorkQueue()->abortRequest(rid);
	}

	void IsoSurfaceBuilder::build( const Voxel::CubeDataRegion * pDataGrid, IsoSurfaceRenderable * pISR, const unsigned nLOD, const Touch3DFlags enStitches )
	{
		{ OGRE_LOCK_MUTEX(mMutex);
			HardwareShadow::LOD * pResolution = pISR->getShadow() ->getDirectAccess(nLOD);

			buildImpl(
#if defined(_DEBUG) || defined(_OHT_LOG_TRACE)
				DebugInfo(pISR),
#endif // _DEBUG
				pISR->getMetaWorldFragment() ->factory->channel,
				pResolution, pDataGrid, pISR->getShadow(),
				pISR->getMetaWorldFragment() ->factory->surfaceFlags,
				enStitches, pISR->getVertexBufferCapacity(nLOD)
			);
			pISR->directlyPopulateBuffers(_pMainVtxElems, pResolution, enStitches, _pMainVtxElems->vertexShipment.size(), _pMainVtxElems->triangles.size() * 3);
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::buildImpl( 
#if defined(_DEBUG) || defined(_OHT_LOG_TRACE)
		const DebugInfo & debugs,
#endif // _DEBUG
		const Channel::Ident channel,
		HardwareShadow::LOD * pResolution,
		const Voxel::CubeDataRegion * pDataGrid, 
		SharedPtr< HardwareIsoVertexShadow > & pShadow, 
		const size_t nSurfaceFlags,
		const Touch3DFlags enStitches, 
		const size_t nVertexBufferCapacity 
	)
	{
		oht_assert_threadmodel(ThrMdl_Single);

		_pShadow = pShadow;
		_pResolution = pResolution;
		_nLOD = _pResolution->lod;
		_nSurfaceFlags = nSurfaceFlags;
		_enStitches = enStitches;
		_pCurrentChannelParams = & _chanparams[channel];

#if defined(_DEBUG) || defined(_OHT_LOG_TRACE)
		_debugs = debugs;
#endif // _DEBUG

		_nHWBufPos = _pResolution->getHWIndexBufferTail();
		_vBorderIVP = _pResolution->borderIsoVertexProperties;
		_vCenterIVP = _pResolution->middleIsoVertexProperties;
		_pMainVtxElems->clear();

		const_DataAccessor data = pDataGrid->lease();

		OHT_ISB_DBGTRACE(">>> Name/LOD/Flags: " << _debugs.name << '/' << _nLOD << '/' << Touch3DFlagNames[_enStitches]);
		//OHTDD_Translate(pDataGrid->getBoundingBox().getHalfSize() / pDataGrid->getGridScale());

		// Check to see if the hardware buffer contains basic vertices for this resolution
		if (!_pResolution->shadowed)
			attainRegularTriangulationCases(data);

		for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
		{
			const Touch3DSide side = OrthogonalNeighbor_to_Touch3DSide[s];

			if ((_enStitches & side) && !_pResolution->stitches[s] ->shadowed)
				attainTransitionTriangulationCases(data, OrthogonalNeighbor(s));
		}

		computeRegularRefinements(data);
		for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
		{
			const Touch3DSide side = OrthogonalNeighbor_to_Touch3DSide[s];

			if (_enStitches & side)
				computeTransitionRefinements(OrthogonalNeighbor(s), data);
		}

		_pResolution->restoreHWIndices(_pMainVtxElems->indices);

		clearTransitionInfo();
		for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
		{
			const Touch3DSide side = OrthogonalNeighbor_to_Touch3DSide[s];

			if (_enStitches & side) 
			{
				if (!_pResolution->stitches[s] ->gpued)
					marshalTransitionVertexElements(pDataGrid, data, OrthogonalNeighbor(s));
				if (!_pResolution->stitches[s] ->shadowed)
					collectTransitionVertexProperties(data, OrthogonalNeighbor(s));
			}
		}
		_vBorderIVP.insert(
			_vBorderIVP.end(), 
			_vTransInfos3[TransitionVRECaCC::TVT_FullOutside].begin(), 
			_vTransInfos3[TransitionVRECaCC::TVT_FullOutside].end()
		);
		_vCenterIVP.insert(
			_vCenterIVP.end(),
			_vTransInfos3[TransitionVRECaCC::TVT_Half].begin(),
			_vTransInfos3[TransitionVRECaCC::TVT_Half].end()
		);

		if (!_pResolution->gpued)
			marshalRegularVertexElements(pDataGrid, data);

		restoreTransitionVertexMappings(data);

		triangulateRegulars();
		triangulateTransitions();

		/* TODO: This is supposed to correct "plateaus" in transition cells (Lengyel p.39)
			but the results look worse, needs work. */
		//alignTransitionVertices();

		const size_t nRequiredHWBufferCapacity = _nHWBufPos + _pMainVtxElems->vertexShipment.size();
		if (nRequiredHWBufferCapacity > nVertexBufferCapacity)
		{
			OHT_ISB_DBGTRACE("Buffer resized, " << nRequiredHWBufferCapacity << " > " << nVertexBufferCapacity);
				
			_pResolution->clearHardwareState();

			_nHWBufPos = 0;
			_pMainVtxElems->rollback();

			for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
			{
				const Touch3DSide side = OrthogonalNeighbor_to_Touch3DSide[s];

				if (_enStitches & side)
					marshalTransitionVertexElements(pDataGrid, data, OrthogonalNeighbor(s));
			}

			marshalRegularVertexElements(pDataGrid, data);

			triangulateRegulars();
			triangulateTransitions();

			/* TODO: This is supposed to correct "plateaus" in transition cells (Lengyel p.39)
				but the results look worse, needs work. */
			//alignTransitionVertices();
		}

		_pResolution->borderIsoVertexProperties.insert(
			_pResolution->borderIsoVertexProperties.end(), 
			_vBorderIVP.begin() + _pResolution->borderIsoVertexProperties.size(), 
			_vBorderIVP.end()
		);
		_pResolution->middleIsoVertexProperties.insert(
			_pResolution->middleIsoVertexProperties.end(), 
			_vCenterIVP.begin() + _pResolution->middleIsoVertexProperties.size(), 
			_vCenterIVP.end()
		);
			
		_pResolution->shadowed = true;
		for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
		{
			const Touch3DSide side = OrthogonalNeighbor_to_Touch3DSide[s];

			if (_enStitches & side)
				_pResolution->stitches[s] ->shadowed = true;
		}

		_pResolution = NULL;
		_pShadow.setNull();
	}

	void IsoSurfaceBuilder::rayQuery( 
		const Channel::Ident channel,
		const CubeDataRegion * pDataGrid, 
		RayCellWalk & walker, 
		const WorldCellCoords & wcctr, 
		const Ray & ray, 
		const SharedPtr< HardwareIsoVertexShadow > & pShadow, 
		const unsigned nLOD, 
		const Touch3DFlags highs 
	)
	{
		{ OGRE_LOCK_MUTEX(mMutex);
			oht_assert_threadmodel(ThrMdl_Main);

			HardwareIsoVertexShadow::ProducerQueueAccess queue = pShadow->requestProducerQueue(nLOD, highs);

			_pShadow = pShadow;
			_pResolution = queue.resolution;
			const_DataAccessor data = pDataGrid->lease();
			_enStitches = highs;
			_nLOD = nLOD;
			_vCenterIVP = _pResolution->middleIsoVertexProperties;
			_pCurrentChannelParams = & _chanparams[channel];

			GridCell gc(_cubemeta, nLOD);
			TransitionCell tc(_cubemeta, nLOD, OrthoNaN);
			typedef std::vector< OrthogonalNeighbor > NeighborVector;
			NeighborVector neighbors(CountOrthogonalNeighbors);

			_pMainVtxElems->clear();
			restoreCaseCache(_pResolution);

			//OHTDD_Color(DebugDisplay::MC_Red);

			walker.lod = nLOD;
			GridCellCoords gcc(
				walker.wcell.i + wcctr.i,
				walker.wcell.j + wcctr.j,
				walker.wcell.k + wcctr.k,
				nLOD
			);

			OgreAssert(
				gcc.i < _cubemeta.dimensions && 
				gcc.j < _cubemeta.dimensions && 
				gcc.k < _cubemeta.dimensions,

				"GridCell Coordinates out-of-bounds"
			);
			OHT_ISB_DBGTRACE("Starting IsoSurfaceBuilder rayQuery with gcc=" << gcc);

			while (walker && 
				gcc.i < _cubemeta.dimensions &&
				gcc.j < _cubemeta.dimensions &&
				gcc.k < _cubemeta.dimensions
			)
			{
				const Touch3DFlags 
					sides = _cubemeta.getCellTouchSide(gcc),
					stitch = Touch3DFlags(sides & highs);

#if defined(_DISPDBG) && defined(_OHT_LOG_TRACE)
				const Vector3 vgcc(gcc.i, gcc.j, gcc.k);
				const Vector3 vWPos = walker.getPosition() + Vector3((Real)wcctr.i, (Real)wcctr.j, (Real)wcctr.k);
				const AxisAlignedBox bboxCell(vgcc, vgcc + Real(1 << nLOD));
				OHT_ISB_DBGTRACE("\t\t\t\t\tCell: [" << gcc.i << "," << gcc.j << "," << gcc.k << "], " 
					<< AxisAlignedBox(
						pDataGrid->getBoundingBox().getMinimum() + bboxCell.getMinimum() * pDataGrid->getGridScale(),
						pDataGrid->getBoundingBox().getMinimum() + bboxCell.getMaximum() * pDataGrid->getGridScale()
					)
					<< ", WRelPos=" << walker.getPosition() << ", WPos=" << (vWPos * pDataGrid->getGridScale() + pDataGrid->getBoundingBox().getMinimum())
				);
				//OHTDD_Cube(bboxCell);
				//OHTDD_Point(vWPos);
#endif

				if (stitch)
				{
					for (unsigned b = 0; b < CountTouch3DSides; ++b)
					{
						const Touch3DSide single = Touch3DSide(stitch & (1 << b));
						if (single)
						{
							OgreAssert(getMoore3DNeighbor(single) < CountOrthogonalNeighbors, "Expected orthogonal neighbor");
							neighbors.push_back(OrthogonalNeighbor(getMoore3DNeighbor(single)));
						}
					}

					for (NeighborVector::const_iterator i  = neighbors.begin(); i != neighbors.end(); ++i)
					{
						//OHTDD_Translate(-pDataGrid->getBoundingBox().getSize() / (pDataGrid->getGridScale() * 2));

						tc.side = *i;
						tc = gcc;
						NonTrivialTransitionCase trcase;
						trcase.cell = tc.index();
						trcase.casecode = _vvTrCase[*i][trcase.cell];
						computeTransitionVertexMappings(data, trcase, tc);
						computeTransitionRefinements(
							data, tc, trcase, 
							[&](const IsoVertexIndex coarse, const IsoVertexIndex refined, const TransitionVRECaCC::Type type, const VoxelIndex c0, const VoxelIndex c1, const Touch3DSide side3d) 
							{
								const IsoFixVec3 & dv = _pCurrentChannelParams->_txTCHalf2Full[_nLOD].side[side3d];
								IsoFixVec3::PrecisionType t = computeIsoVertexPosition(data.values, c0, c1);
								configureIsoVertex(
									_pMainVtxElems, pDataGrid, data, refined, t, c0, c1,
									dv & long(~((type & TransitionVRECaCC::TVT_Half) - 1))
								);
							}
						);

						TransitionTriangleBuilder trbuild(_cubemeta, _pMainVtxElems, nLOD, *i);
						trbuild << trcase;

						for (TransitionTriangleIterator j = trbuild.iterator(); j; ++j)
						{
							OHT_ISB_DBGTRACE(
								"\t\t\t\t\t\t\tTriangle (Transition): (" 
								<< Vector3(_pMainVtxElems->positions[j[0]]) * pDataGrid->getGridScale() + pDataGrid->getPosition()
								<< ',' 
								<< Vector3(_pMainVtxElems->positions[j[1]]) * pDataGrid->getGridScale() + pDataGrid->getPosition()
								<< ',' 
								<< Vector3(_pMainVtxElems->positions[j[2]]) * pDataGrid->getGridScale() + pDataGrid->getPosition()
								<< ')'
							);

							if (!j.collapsed() && rayCollidesTriangle(&walker.distance, ray, j[0], j[1], j[2]))
							{
								walker.intersection = true;
								return;
							}
						}
					}
				}

				gc = gcc;

				//OHTDD_Translate(-pDataGrid->getBoundingBox().getSize() / (pDataGrid->getGridScale() * 2));

				NonTrivialRegularCase rgcase;
				rgcase.cell = gc.index();
				rgcase.casecode = _vRegularCases[rgcase.cell];
				computeRegularRefinements(
					data, gc, rgcase, 
					[&](const IsoVertexIndex coarse, const IsoVertexIndex refined, const VoxelIndex c0, const VoxelIndex c1, const unsigned nLOD) 
					{
						IsoFixVec3::PrecisionType t = computeIsoVertexPosition(data.values, c0, c1);
						configureIsoVertex(_pMainVtxElems, pDataGrid, data, refined, t, c0, c1);
					}
				);

				RegularTriangleBuilder rgbuild(_cubemeta, _pMainVtxElems, nLOD);
				rgbuild << rgcase;
				for (RegularTriangleIterator j = rgbuild.iterator(); j; ++j)
				{
					OHT_ISB_DBGTRACE(
						"\t\t\t\t\t\t\tTriangle (Regular): (" 
						<< Vector3(_pMainVtxElems->positions[j[0]]) * pDataGrid->getGridScale() + pDataGrid->getPosition()
						<< ',' 
						<< Vector3(_pMainVtxElems->positions[j[1]]) * pDataGrid->getGridScale() + pDataGrid->getPosition()
						<< ',' 
						<< Vector3(_pMainVtxElems->positions[j[2]]) * pDataGrid->getGridScale() + pDataGrid->getPosition()
						<< ')'
					);

					if (!j.collapsed() && rayCollidesTriangle(&walker.distance, ray, j[0], j[1], j[2]))
					{
						walker.intersection = true;
						return;
					}
				}

				++walker;
				gcc.i = walker.wcell.i + wcctr.i;
				gcc.j = walker.wcell.j + wcctr.j;
				gcc.k = walker.wcell.k + wcctr.k;

				neighbors.clear();
			}
		}

		walker.intersection = false;
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::restoreTransitionVertexMappings( const_DataAccessor & data )
	{
		TransitionCell tc(_cubemeta, _nLOD, OrthoNaN);

		OHT_ISB_DBGTRACE("Recall neighbor flags: " << Touch3DFlagNames[_enStitches]);
		for (
			BorderIsoVertexPropertiesVector::const_iterator i = _vCenterIVP.begin(); 
			i != _vCenterIVP.end(); 
			++i
		)
		{
			tc = i->cell;
			tc.side = i->neighbor;
			restoreTransitionVertexMappings(data, i->vrec, i->touch, tc, i->index);
		}	
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::restoreTransitionVertexMappings( const_DataAccessor & data, TransitionVRECaCC vrecacc, const Touch3DSide touch, TransitionCell tc, const IsoVertexIndex index )
	{
		VoxelIndex cih0, cih1, cif0, cif1;
		TouchStatus tsx, tsy;
		Touch2DSide rside;
		IsoVertexIndex fivi;
		CubeSideCoords hcsc, csc;
		unsigned char ei;


		OgreAssert(vrecacc.getType() == TransitionVRECaCC::TVT_Half, "Vertex must be on the half-resolution face");

		if (touch & _enStitches)
		{
			vrecacc.setEdgeCode(vrecacc.getFullResEdgeCode());
			computeRefinedTransitionIsoVertex(tc, data, vrecacc, ei, cif0, cif1, tsx, tsy, rside, fivi, csc);

			if (((_enStitches | touch) ^ _enStitches) == 0)
				_pMainVtxElems->remappings[fivi] = index;
			else
				_pMainVtxElems->trmappings[index] = fivi;
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::attainRegularTriangulationCases( const_DataAccessor & data )
	{
		OgreAssert(_nLOD < _pCurrentChannelParams->clod, "Level of detail exceeds lowest allowed");
		oht_assert_threadmodel(ThrMdl_Single);

		const IsoVertexIndex nDim = _cubemeta.dimensions;
		typedef DimensionType DimType;
		const DimType nResSpan = 1 << _nLOD;
		NonTrivialRegularCase nontrivialcase;
		signed char gc7;

		iterator_GridCells i = iterate_GridCells(); 
		
		while (i)
		{
			nontrivialcase.casecode = 0;
			nontrivialcase.cell = i->gc.index();

			for (unsigned c = 0; c < 8; ++c, ++i)
			{
				OgreAssert(i->index == i->gc.corners[c], "Corner index check failed");
				nontrivialcase.casecode |= (gc7 = (data.values[i->index] >> (8 - 1 - i->corner))) & (1 << i->corner);
			}

			if ((nontrivialcase.casecode ^ ((gc7 >> 7) & 0xFF)) != 0)
			{
				// Cell has a nontrivial triangulation.
				_pResolution->regCases.push_back(nontrivialcase);
			}
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::attainTransitionTriangulationCases( const_DataAccessor & data, OrthogonalNeighbor on )
	{
		TransitionCell tc(_cubemeta, _nLOD, on);
		const IsoVertexIndex nDim = _cubemeta.dimensions;
		typedef DimensionType DimType;
		const DimType nResSpan = 1 << _nLOD;
		NonTrivialTransitionCase nontrivialcase;
		signed short tc8;

		OgreAssert(_pResolution->stitches[on] != NULL, "Transition state was not allocated");

		for (tc.x = 0; tc.x < nDim; tc.x += nResSpan)
			for (tc.y = 0; tc.y < nDim; tc.y += nResSpan)
			{
				nontrivialcase.casecode = 
					(
						signed short
						( // TODO: Operator [] here is wrong, describes figure 4.16, but requires indexing scheme illustrated in figure 4.17
							((data.values[tc.flags[1]] >> 7) & 0x01) |
							((data.values[tc.flags[2]] >> 6) & 0x02) |
							((data.values[tc.flags[3]] >> 5) & 0x04) |
							((data.values[tc.flags[4]] >> 4) & 0x08) |
							((data.values[tc.flags[5]] >> 3) & 0x10) |
							((data.values[tc.flags[6]] >> 2) & 0x20) |
							((data.values[tc.flags[7]] >> 1) & 0x40) |
							(((tc8 = data.values[tc.flags[8]]) >> 0) & 0x80)
						) << 1
					) | ((signed short (data.values[tc.flags[0]]) >> 8) & 0x1);

				tc8 <<= 8;
				if ((nontrivialcase.casecode ^ ((tc8 >> 15) & 0x1FF)) != 0)
				{
					nontrivialcase.cell = tc.index();
					_pResolution->stitches[on] ->transCases.push_back(nontrivialcase);

					OHT_ISB_DBGTRACE("Attain Case: " << tc << drawTransitionCell(nontrivialcase.casecode, 1));
				}
			}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::marshalRegularVertexElements( const CubeDataRegion * pDataGrid, const_DataAccessor & data )
	{
		OgreAssert(_nLOD < _pCurrentChannelParams->clod, "Level of detail exceeds lowest allowed");
		oht_assert_threadmodel(ThrMdl_Single);

		GridCell gc (_cubemeta, _nLOD);
		IsoFixVec3::PrecisionType t;

		processRegularIsoVertices
		(
			_pResolution->regCases.begin(), _pResolution->regCases.end(), 
			gc, 
			[&] (const IsoVertexIndex ivi, const VoxelIndex c0, const VoxelIndex c1) 
			{
				t = computeIsoVertexPosition(data.values, c0, c1);
				configureIsoVertex(_pMainVtxElems, pDataGrid, data, ivi, t, c0, c1);

				OgreAssert(_pMainVtxElems->indices[ivi] == HWVertexIndex(~0), "Expected unallocated HW iso-vertex index");
				// Assign the next index in the hardware vertex buffer to this iso-vertex
				_pMainVtxElems->indices[ivi] = _nHWBufPos++;
				_pMainVtxElems->vertexShipment.push_back(ivi);
				OHT_ISB_DBGTRACE("MarshalRegular " << ivi << " to HWIdx " << (_nHWBufPos - 1) << ", position=" << _pMainVtxElems->positions[ivi]);
			}
		);
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::marshalTransitionVertexElements( const CubeDataRegion * pDataGrid, const_DataAccessor & data, const OrthogonalNeighbor on )
	{
		OgreAssert(_nLOD < _pCurrentChannelParams->clod, "Level of detail exceeds lowest allowed");
		oht_assert_threadmodel(ThrMdl_Single);

		TransitionCell tc (_cubemeta, _nLOD, on);
		IsoFixVec3::PrecisionType t;

		//OHTDD_Color(DebugDisplay::MC_Yellow);

		processTransitionIsoVertices 
		(
			_pResolution->stitches[on]->transCases.begin(), _pResolution->stitches[on]->transCases.end(), 
			tc, 
			[&] (const IsoVertexIndex ivi, const TransitionVRECaCC::Type type, const VoxelIndex c0, const VoxelIndex c1, const Touch3DSide side3d) 
			{
				//OHTDD_Cube(AxisAlignedBox(GridPointCoords(tc), Vector3(GridPointCoords(tc)) + float(1 << tc.halfLOD)));

				const IsoFixVec3 & dv = _pCurrentChannelParams->_txTCHalf2Full[_nLOD].side[side3d];
				t = computeIsoVertexPosition(data.values, c0, c1);
				configureIsoVertex(
					_pMainVtxElems,
					pDataGrid, 
					data,
					ivi, 
					t,
					c0, 
					c1,
					dv & long(~((type & TransitionVRECaCC::TVT_Half) - 1))	/// Transform vertex from full to half resolution face iff the edge-code indicates such
				);

				// Assign the next index in the hardware vertex buffer to this iso vertex
				_pMainVtxElems->indices[ivi] = _nHWBufPos++;
				_pMainVtxElems->vertexShipment.push_back(ivi);
				OHT_ISB_DBGTRACE("MarshalTransition " << ivi << " to HWIdx " << (_nHWBufPos - 1) << ", position=" << _pMainVtxElems->positions[ivi]);
			}
		);
	}
	
#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::collectTransitionVertexProperties( const_DataAccessor & data, const OrthogonalNeighbor on )
	{
		OgreAssert(_nLOD < _pCurrentChannelParams->clod, "Level of detail exceeds lowest allowed");
		oht_assert_threadmodel(ThrMdl_Single);

		TransitionCell tc (_cubemeta, _nLOD, on);
		BorderIsoVertexProperties trprops;

		trprops.neighbor = on;

		collectTransitionVertexProperties
		(
			data, 
			_pResolution->stitches[on]->transCases.begin(), _pResolution->stitches[on]->transCases.end(), 
			tc, 
			[&] (const IsoVertexIndex ivi, const TransitionVRECaCC & vrecacc, const VoxelIndex c0, const VoxelIndex c1, const Touch3DSide side3d) 
			{
				trprops.vrec = vrecacc;
				trprops.index = ivi;
				trprops.touch = side3d;
				trprops.cell = tc.index();
#ifdef _DEBUG
				trprops.coords = _trrefiner.getCoords();
#endif
				_vTransInfos3[trprops.vrec.getType()].push_back(trprops);
			}
		);
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::triangulateRegulars()
	{
		OgreAssert(_nLOD < _pCurrentChannelParams->clod, "Level of detail exceeds lowest allowed");
		oht_assert_threadmodel(ThrMdl_Single);

		RegularTriangleBuilder tribuild (_cubemeta, _pMainVtxElems, _nLOD);
		IsoTriangle tri;
#ifdef _OHT_LOG_TRACE
		GridCell gc(_cubemeta, _nLOD);
#endif // _DEBUG
		
		for (RegularTriangulationCaseList::const_iterator i = _pResolution->regCases.begin(); i != _pResolution->regCases.end(); ++i)
		{
			tribuild << *i;

#ifdef _OHT_LOG_TRACE
			gc = i->cell;
			OHT_ISB_DBGTRACE(
				"GridCell (" << gc.x << ',' << gc.y << ',' << gc.z << ')'
				<< drawGridCell(i->casecode, 1)
			);
#endif // _DEBUG

			for (RegularTriangleIterator j = tribuild.iterator(); j; ++j)
			{
				tri.vertices[0] = j[0];
				tri.vertices[1] = j[1];
				tri.vertices[2] = j[2];

				OgreAssert(
					_pMainVtxElems->indices[tri.vertices[0]] != HWVertexIndex(~0) && 
					_pMainVtxElems->indices[tri.vertices[1]] != HWVertexIndex(~0) && 
					_pMainVtxElems->indices[tri.vertices[2]] != HWVertexIndex(~0),

					"Uhoh, unmapped vertex"
				);
#ifdef _OHT_LOG_TRACE
				tri.id = _pMainVtxElems->triangles.size();
				OHT_ISB_DBGTRACE(
					"\tIsoTriangle #" << tri.id << ": "
					<< "\n\t\t0=" << tri.vertices[0] << " - " << getWorldPos(tri.vertices[0])
					<< "\n\t\t1=" << tri.vertices[1] << " - " << getWorldPos(tri.vertices[1])
					<< "\n\t\t2=" << tri.vertices[2] << " - " << getWorldPos(tri.vertices[2])
				);
#endif

				// Prepares this triangle for the index buffer unless it contains duplicate vertices, which would result in a line and not a triangle.
				addIsoTriangle(tri);
			}
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::triangulateTransitions()
	{
		OgreAssert(_nLOD < _pCurrentChannelParams->clod, "Level of detail exceeds lowest allowed");
		oht_assert_threadmodel(ThrMdl_Single);
#ifdef _OHT_LOG_TRACE
		TransitionCell tc(_cubemeta, _nLOD, OrthoNaN);
#endif // _DEBUG

		IsoTriangle tri;

		for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
		{
			TransitionTriangleBuilder tribuild(_cubemeta, _pMainVtxElems, _nLOD, (OrthogonalNeighbor)s);
			const Touch3DSide side = OrthogonalNeighbor_to_Touch3DSide[s];

#ifdef _OHT_LOG_TRACE
			tc.side = (OrthogonalNeighbor)s;
#endif // _DEBUG
			if (_enStitches & side)
			{
				for (TransitionTriangulationCaseList::const_iterator i = _pResolution->stitches[s] ->transCases.begin(); i != _pResolution->stitches[s] ->transCases.end(); ++i)
				{
					tribuild << *i;

#ifdef _OHT_LOG_TRACE
					tc = i->cell;
					OHT_ISB_DBGTRACE(
						"GridCell (" << tc.x << ',' << tc.y << ',' << Neighborhood::name(tc.side) << ')'
						<< drawTransitionCell(i->casecode, 1);
					);
#endif // _DEBUG

					for (TransitionTriangleIterator j = tribuild.iterator(); j; ++j) 
					{
						tri.vertices[0] = j[0];
						tri.vertices[1] = j[1];
						tri.vertices[2] = j[2];

#ifdef _OHT_LOG_TRACE
						tri.id = _pMainVtxElems->triangles.size();
						OHT_ISB_DBGTRACE(
							"\tTransitionIsoTriangle #" << tri.id << ": "
							<< "\n\t\t0=" << tri.vertices[0] << " - " << getWorldPos(tri.vertices[0])
							<< "\n\t\t1=" << tri.vertices[1] << " - " << getWorldPos(tri.vertices[1])
							<< "\n\t\t2=" << tri.vertices[2] << " - " << getWorldPos(tri.vertices[2])
						);
#endif
						// Prepares this triangle for the index buffer unless it contains duplicate vertices, which would result in a line and not a triangle.
						addIsoTriangle(tri);
					}
				}
			}
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::alignTransitionVertices()
	{
		for (BorderIsoVertexPropertiesVector::const_iterator i = _vCenterIVP.begin(); i != _vCenterIVP.end(); ++i)
			alignTransitionVertex(i->index, i->touch);
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::alignTransitionVertex( const IsoVertexIndex index, const Touch3DSide t3ds )
	{
		if (_pMainVtxElems->trmappings[index] != IsoVertexIndex(~0))
			return;

		Vector3 & n = _pMainVtxElems->normals[index];
		n.normalise();

		const Matrix3 matProj (
			1 - Math::Sqr(n.x),				-n.x*n.y,		-n.x*n.z,
			-n.x*n.y,				1 - Math::Sqr(n.y),		-n.y*n.z,
			-n.x*n.z,						-n.y*n.z,		1 - Math::Sqr(n.z)
		);
		const IsoFixVec3 & dv = _pCurrentChannelParams->_txTCHalf2Full[_nLOD].side[t3ds];

		_pMainVtxElems->positions[index] += IsoFixVec3(matProj * Vector3(-dv)) + dv;
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::configureIsoVertex( IsoVertexElements * pVtxElems, const CubeDataRegion * pDataGrid, const_DataAccessor & data, const IsoVertexIndex nIsoVertexIdx, const IsoFixVec3::PrecisionType t, const VoxelIndex corner0, const VoxelIndex corner1, const IsoFixVec3 & dv /*= IsoFixVec3(signed short(0),signed short(0),signed short(0)) */ )
	{
		// Calculate the iso vertex position by interpolation
		const IsoFixVec3* vertices = pDataGrid->getVertices();
		pVtxElems->positions[nIsoVertexIdx] = vertices[corner0]*t + vertices[corner1]*(-t+signed short(1)) + dv;

		if (_nSurfaceFlags & IsoVertexElements::GEN_NORMALS)
		{
			// Generate optional normal
			switch (_pCurrentChannelParams->normalsType)
			{
			case NT_WeightedAverage:
			case NT_Average:
				pVtxElems->normals[nIsoVertexIdx] = Vector3::ZERO;
				break;

			case NT_Gradient:
				{
					const IsoFixVec3 
						g0(static_cast< const_DataAccessor::GradientField::VectorType > (data.gradients[corner0])),
						g1(static_cast< const_DataAccessor::GradientField::VectorType > (data.gradients[corner1]));

					if (_pCurrentChannelParams->flipNormals)
						pVtxElems->normals[nIsoVertexIdx] = (g0 - g1)*t - g0;
					else
						pVtxElems->normals[nIsoVertexIdx] = g0 + (g1 - g0)*t;

					pVtxElems->normals[nIsoVertexIdx].normalise();
				}
			}
		}

		if (_nSurfaceFlags & IsoVertexElements::GEN_VERTEX_COLOURS)
		{
			// Generate optional vertex colours by interpolation
			const ColourValue 
				c0 = data.colours[corner0],
				c1 = data.colours[corner1];
			pVtxElems->colours[nIsoVertexIdx] = t*c0 + (-t+signed short(1))*c1;
		}

		if (_nSurfaceFlags & IsoVertexElements::GEN_TEX_COORDS)
		{
			// Generate optional texture coordinates
			// TODO: Implementation

			pVtxElems->texcoords[nIsoVertexIdx][0] = pVtxElems->positions[nIsoVertexIdx].x;
			pVtxElems->texcoords[nIsoVertexIdx][1] = pVtxElems->positions[nIsoVertexIdx].y;
			// TODO: I think this is wrong
			//mIsoVertexTexCoords[isoVertex][2] = mIsoVertexPositions[isoVertex].z;
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::addIsoTriangle( const IsoTriangle& isoTriangle )
	{
		if ((isoTriangle.vertices[0] == isoTriangle.vertices[1]) ||
			(isoTriangle.vertices[1] == isoTriangle.vertices[2]) ||
			(isoTriangle.vertices[2] == isoTriangle.vertices[0])
		)
			return;

		if ((_nSurfaceFlags & IsoVertexElements::GEN_NORMALS) && (_pCurrentChannelParams->normalsType != NT_Gradient))
		{
			IsoFixVec3 normal = 
				(_pMainVtxElems->positions[isoTriangle.vertices[1]] - _pMainVtxElems->positions[isoTriangle.vertices[0]]) 
				%
				(_pMainVtxElems->positions[isoTriangle.vertices[2]] - _pMainVtxElems->positions[isoTriangle.vertices[0]]);

			switch (_pCurrentChannelParams->normalsType)
			{
			case NT_WeightedAverage:
				// TODO: Kludge / hack / ugly
				if (LENGTH(normal) > Real(0))
					normal = NORMALIZE(normal) / LENGTH(normal);
				break;
			case NT_Average:
				normal = NORMALIZE(normal);
				break;
			}

			_pMainVtxElems->normals[isoTriangle.vertices[0]] += normal;
			_pMainVtxElems->normals[isoTriangle.vertices[1]] += normal;
			_pMainVtxElems->normals[isoTriangle.vertices[2]] += normal;
		}

		_pMainVtxElems->triangles.push_back(isoTriangle);
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::clearTransitionInfo()
	{
		_vTransInfos3[TransitionVRECaCC::TVT_Half].clear();
		_vTransInfos3[TransitionVRECaCC::TVT_FullInside].clear();
		_vTransInfos3[TransitionVRECaCC::TVT_FullOutside].clear();
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::computeRefinedRegularIsoVertex( const GridCell & gc, const_DataAccessor & data, const VRECaCC & vrecacc, unsigned char & ei, VoxelIndex & ci0, VoxelIndex & ci1, const unsigned nLOD, IsoVertexIndex & ivi, GridPointCoords & gpc )
	{
		_rgrefiner.compute(nLOD, gc.corners, data, vrecacc.getCorner0(), vrecacc.getCorner1());
		gpc = _rgrefiner.getCoords();
		ei = vrecacc.getEdgeCode() & ~_rgrefiner.getZeroValueFlag();
		ivi = _pMainVtxElems->getRegularVertexIndex(ei, gpc);
		ci0 = _rgrefiner.getGridIndex0();
		ci1 = _rgrefiner.getGridIndex1();
		OgreAssert(
			((ci0 == _pMainVtxElems->cellindices[ivi].corner0) && (ei == 0 && ci1 != _pMainVtxElems->cellindices[ivi].corner1)) ||
			((ci1 == _pMainVtxElems->cellindices[ivi].corner1) && (ei == 0 && ci0 != _pMainVtxElems->cellindices[ivi].corner0)) ||
			((ci1 == _pMainVtxElems->cellindices[ivi].corner1) && (ci0 == _pMainVtxElems->cellindices[ivi].corner0)), 
			"Corner indicies mismatch check failed"
		);

		OHT_ISB_DBGTRACE("Refining Regular: VRECaCC=" << vrecacc << ", GC=" << gc << ": ivi=" << ivi << ", resolved EI=" << (int)ei << ", coords pair=<" << ci0 << "x" << ci1 << ">");
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::computeRefinedTransitionIsoVertex
	( 
		const TransitionCell & tc, 
		const_DataAccessor & data, 
		const TransitionVRECaCC & vrecacc, 
		unsigned char & ei, 
		VoxelIndex & ci0, VoxelIndex & ci1, 
		TouchStatus & tsx, TouchStatus & tsy, 
		Touch2DSide & rside, 
		IsoVertexIndex & ivi, 
		CubeSideCoords & csc 
	)
	{
		_trrefiner.compute(tc.halfLOD - vrecacc.isfHalfRes(), tc.corners, data, vrecacc.getCorner0(), vrecacc.getCorner1());
		csc = _trrefiner.getCoords();

#ifdef _OHT_LOG_TRACE
		const VoxelIndex ci00 = _trrefiner.getGridIndex0(), ci01 = _trrefiner.getGridIndex1();
#endif

		const static unsigned char ODD_MASK = ((1 << 1) - 1);
		register unsigned char mhr = vrecacc.isfHalfRes();
		if (mhr)
		{
			_trrefiner.oneMoreTime(tc.corners, data);

			const CubeSideCoords & csc2 = _trrefiner.getCoords();
			// Mask indicating whether the coordinates are odd (not modulo 2)
			const register signed char mch = (~bitmanip::testZero(signed int((csc2.x | csc2.y) & ODD_MASK)));
			
			// Coordinates are made coarse after they are chosen between refined truth and 0-base, condition flag: coords coarseness
			csc = (csc2 & (signed int)~mch & ~ODD_MASK);
			csc |= (_trrefiner.getCoords0() & (signed int)mch & ~ODD_MASK);
			
			_cubemeta.computeTouchProperties(csc.x, csc.y, tsx, tsy, rside);
			
			// Mask indicating a zero value isolated at an even boundary
			const register unsigned char mz = _trrefiner.getZeroValueFlag() & ~mch;
			ei = (vrecacc.getEdgeCode() & ~mz) | (7 & mz);
		}
		else
		{
			_cubemeta.computeTouchProperties(csc.x, csc.y, tsx, tsy, rside);
			ei = (vrecacc.getEdgeCode() & ~_trrefiner.getZeroValueFlag());	// Set to 0 conditionally if zero-flag is set
		}

		_pMainVtxElems->computeTransitionIndexProperties(tc.side, csc.x, csc.y, ei, tsx, tsy, rside, ivi, rside);
		ci0 = _trrefiner.getGridIndex0();
		ci1 = _trrefiner.getGridIndex1();

		OHT_ISB_DBGTRACE("Refining Transition: VRECaCC=" << vrecacc << ", TC=" << tc << ": ivi=" << ivi << ", resolved EI=" << (int)ei << ", coords pair=<" << ci0 << "x" << ci1 << ">");
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::restoreCaseCache( const HardwareShadow::LOD * pResState )
	{
		memset(_vRegularCases, 0, sizeof(*_vRegularCases) * _cubemeta.cellcount);
		for (RegularTriangulationCaseList::const_iterator i = pResState->regCases.begin(); i != pResState->regCases.end(); ++i)
			_vRegularCases[i->cell] = i->casecode;

		for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
		{
			memset(_vvTrCase[s], 0, sizeof(*_vvTrCase[s]) * _cubemeta.sidecellcount);
			for (TransitionTriangulationCaseList::const_iterator i = pResState->stitches[s] ->transCases.begin(); i != pResState->stitches[s] ->transCases.end(); ++i)
				_vvTrCase[s][i->cell] = i->casecode;
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::computeRegularRefinements( const_DataAccessor & data )
	{
		GridCell gc(_cubemeta, _nLOD);

		walkCellVertices
		(
			_pResolution->regCases.begin(), _pResolution->regCases.end(), 
			gc, 
			[&] (const GridCell & gc, const unsigned short nVRECaCC, const unsigned nLOD)
			{
				computeRegularRefinements(data, gc, nVRECaCC, [] (const IsoVertexIndex coarse, const IsoVertexIndex refined, const VoxelIndex c0, const VoxelIndex c1, const unsigned nLOD) {});
			}
		);
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::computeTransitionRefinements( const OrthogonalNeighbor on, const_DataAccessor & data )
	{
		TransitionCell tc (_cubemeta, _nLOD, on);

		walkCellVertices(
			_pResolution->stitches[on]->transCases.begin(), 
			_pResolution->stitches[on]->transCases.end(), 
			tc, 
			[&] (const TransitionCell & tc, const unsigned short nVRECaCC, const unsigned nLOD)
			{
				computeTransitionRefinements(data, tc, nVRECaCC, nLOD, [](const IsoVertexIndex, const IsoVertexIndex, const TransitionVRECaCC::Type, const VoxelIndex, const VoxelIndex, const Touch3DSide){});
			}
		);
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::computeTransitionVertexMappings( const_DataAccessor & data, const NonTrivialTransitionCase & caze, TransitionCell & tc )
	{
		collectTransitionVertexProperties(data, caze, tc, [&] (const IsoVertexIndex ivi, const TransitionVRECaCC & vrecacc, const VoxelIndex c0, const VoxelIndex c1, const Touch3DSide side3d) {
			if (vrecacc.isfHalfRes())
				restoreTransitionVertexMappings(data, vrecacc, side3d, tc, ivi);
		});
	}

	/** See the following links for details:
	 * - http://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld017.htm
	 * - http://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld018.htm
	 * - http://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld019.htm
	 */
	bool IsoSurfaceBuilder::rayCollidesTriangle( Real * pDist, const Ray & ray, const IsoVertexIndex a, const IsoVertexIndex b, const IsoVertexIndex c ) const
	{
		const Vector3
			va = _pMainVtxElems->positions[a],
			v0 = _pMainVtxElems->positions[b] - _pMainVtxElems->positions[a],
			v1 = _pMainVtxElems->positions[c] - _pMainVtxElems->positions[a],

			normal = CROSS(v0, v1);

		const Vector3
			oq = ray.getDirection() * (DOT(va - ray.getOrigin(), normal) / DOT(ray.getDirection(), normal)),
			q = ray.getOrigin() + oq,
			aq = q - va;

		const Vector3
			& ab = v0,
			bc = _pMainVtxElems->positions[c] - _pMainVtxElems->positions[b],
			ca = _pMainVtxElems->positions[a] - _pMainVtxElems->positions[c];

		const Vector3
			& u = v0,
			& v = v1,
			& w = aq;

		const Real
			vv = DOT(v,v),
			uu = DOT(u,u),
			uv = DOT(u,v),
			wv = DOT(w,v),
			wu = DOT(w,u),
			denom = (uv*uv- uu*vv),

			s = (uv*wv - vv*wu) / denom,
			t = (uv*wu - uu*wv) / denom;
		
		*pDist = oq.length();

#if defined(_OHT_LOG_TRACE) && defined(_DISPDBG)
		OHT_ISB_DBGTRACE("\t\t\t\t\t\t\t\t\tQ=" << OHTDD_Reverse(q) << ", (s,t) = (" << s << ',' << t << ')' << ", distance=" << *pDist);
#endif // _DEBUG

		//OHTDD_Color(DebugDisplay::MC_Blue);
		//OHTDD_Point(q);

#if defined(_DISPDBG)
		DebugDisplay::MaterialColor mcColor;
		if (s >= 0 && t >= 0 && s+t <= 1.0f)
			mcColor = DebugDisplay::MC_Green;
		else
			mcColor = DebugDisplay::MC_Magenta;

		//OHTDD_Color(mcColor);
		//OHTDD_Tri(mMainVtxElems->positions[a], mMainVtxElems->positions[b], mMainVtxElems->positions[c]);
#endif // _DEBUG

		return s >= 0 && t >= 0 && s+t <= 1.0f;
	}

	void IsoSurfaceBuilder::fillShadowQueues( HardwareIsoVertexShadow::ProducerQueueAccess & queue, const Real fVertScale )
	{
		oht_assert_threadmodel(ThrMdl_Background);

		for (IsoVertexVector::const_iterator i = _pMainVtxElems->vertexShipment.begin(); i != _pMainVtxElems->vertexShipment.end(); ++i)
		{
			BuilderQueue::VertexElement vtxelem;

			const IsoFixVec3 & pt = _pMainVtxElems->positions[*i];

			vtxelem.position.x = Real(pt.x) * fVertScale;
			vtxelem.position.y = Real(pt.y) * fVertScale;
			vtxelem.position.z = Real(pt.z) * fVertScale;

			if (_nSurfaceFlags & IsoVertexElements::GEN_NORMALS)
				vtxelem.normal = _pMainVtxElems->normals[*i];

			if (_nSurfaceFlags & IsoVertexElements::GEN_VERTEX_COLOURS)
				vtxelem.setColour(_pMainVtxElems->colours[*i]);

			if (_nSurfaceFlags & IsoVertexElements::GEN_TEX_COORDS)
			{
				vtxelem.texcoord.x = _pMainVtxElems->texcoords[*i][0];
				vtxelem.texcoord.y = _pMainVtxElems->texcoords[*i][1];
			}

			queue.vertexQueue.push_back(vtxelem);
		}

		for (IsoTriangleVector::const_iterator i = _pMainVtxElems->triangles.begin(); i != _pMainVtxElems->triangles.end(); ++i)
		{
			OgreAssert(
				_pMainVtxElems->indices[i->vertices[0]] != HWVertexIndex(~0) &&
				_pMainVtxElems->indices[i->vertices[1]] != HWVertexIndex(~0) &&
				_pMainVtxElems->indices[i->vertices[2]] != HWVertexIndex(~0),

				"Iso-vertex to hardware vertex index not mapped"
			);

			queue.indexQueue.push_back(_pMainVtxElems->indices[i->vertices[0]]);
			queue.indexQueue.push_back(_pMainVtxElems->indices[i->vertices[1]]);
			queue.indexQueue.push_back(_pMainVtxElems->indices[i->vertices[2]]);
		}

		queue.revmapIVI2HWVIQueue = _pMainVtxElems->vertexShipment;
		_pMainVtxElems->vertexShipment.clear();
	}

	const IsoSurfaceBuilder::MainVertexElements::Shift3D
		IsoSurfaceBuilder::MainVertexElements::_regeiivimx[4] = 
	{
		{ 1, 1, 1 },
		{ 1, 0, 1 },
		{ 0, 1, 1 },
		{ 1, 1, 0 }
	};
	const IsoSurfaceBuilder::MainVertexElements::Shift2D 
		IsoSurfaceBuilder::MainVertexElements::_transhifts[10] =
	{
		{ 2, 2 },
		{ 1, 2 },
		{ 2, 1 },
		{ 0, 2 },
		{ 1, 2 },
		{ 2, 0 },
		{ 2, 1 },
		{ 2, 2 },
		{ 0, 2 },
		{ 2, 0 }
	};
	const IsoSurfaceBuilder::MainVertexElements::Shift2D
		IsoSurfaceBuilder::MainVertexElements::_teidim1[10] =
	{
		{ 1, 1 },
		{ 1, 1 },
		{ 1, 1 },
		{ 0, 1 },
		{ 0, 1 },
		{ 1, 0 },
		{ 1, 0 },
		{ 1, 1 },
		{ 0, 1 },
		{ 1, 0 }
	};

	const unsigned char
		IsoSurfaceBuilder::MainVertexElements::_2dts2m3n
			[CountOrthogonalNeighbors]
			[Count2DTouchSideElements] =
	{
		{	// OrthoN_NORTH
			Moore3N_NORTH,				// B00,00 : Face
			Moore3N_NORTHWEST,			// B00,01 : X-low
			Moore3N_NORTHEAST,			// B00,10 : X-high
			Moore3NaN,					// B00,11
			Moore3N_BELOWNORTH,			// B01,00 : Y-low
			Moore3N_BELOWNORTHWEST,		// B01,01 : Y-low,X-low
			Moore3N_BELOWNORTHEAST,		// B01,10 : Y-low,X-high
			Moore3NaN,					// B01,11
			Moore3N_ABOVENORTH,			// B10,00 : Y-high
			Moore3N_ABOVENORTHWEST,		// B10,01 : Y-high,X-low
			Moore3N_ABOVENORTHEAST		// B10,10 : Y-high,X-high
		},
		{	// OrthoN_EAST
			Moore3N_EAST,				// B00,00 : Face
			Moore3N_NORTHEAST,			// B00,01 : X-low
			Moore3N_SOUTHEAST,			// B00,10 : X-high
			Moore3NaN,					// B00,11
			Moore3N_BELOWEAST,			// B01,00 : Y-low
			Moore3N_BELOWNORTHEAST,		// B01,01 : Y-low,X-low
			Moore3N_BELOWSOUTHEAST,		// B01,10 : Y-low,X-high
			Moore3NaN,					// B01,11
			Moore3N_ABOVEEAST,			// B10,00 : Y-high
			Moore3N_ABOVENORTHEAST,		// B10,01 : Y-high,X-low
			Moore3N_ABOVESOUTHEAST		// B10,10 : Y-high,X-high
		},
		{	// OrthoN_WEST
			Moore3N_WEST,				// B00,00 : Face
			Moore3N_NORTHWEST,			// B00,01 : X-low
			Moore3N_SOUTHWEST,			// B00,10 : X-high
			Moore3NaN,					// B00,11
			Moore3N_BELOWWEST,			// B01,00 : Y-low
			Moore3N_BELOWNORTHWEST,		// B01,01 : Y-low,X-low
			Moore3N_BELOWSOUTHWEST,		// B01,10 : Y-low,X-high
			Moore3NaN,					// B01,11
			Moore3N_ABOVEWEST,			// B10,00 : Y-high
			Moore3N_ABOVENORTHWEST,		// B10,01 : Y-high,X-low
			Moore3N_ABOVESOUTHWEST		// B10,10 : Y-high,X-high
		},
		{	// OrthoN_SOUTH
			Moore3N_SOUTH,				// B00,00 : Face
			Moore3N_SOUTHWEST,			// B00,01 : X-low
			Moore3N_SOUTHEAST,			// B00,10 : X-high
			Moore3NaN,					// B00,11
			Moore3N_BELOWSOUTH,			// B01,00 : Y-low
			Moore3N_BELOWSOUTHWEST,		// B01,01 : Y-low,X-low
			Moore3N_BELOWSOUTHEAST,		// B01,10 : Y-low,X-high
			Moore3NaN,					// B01,11
			Moore3N_ABOVESOUTH,			// B10,00 : Y-high
			Moore3N_ABOVESOUTHWEST,		// B10,01 : Y-high,X-low
			Moore3N_ABOVESOUTHEAST		// B10,10 : Y-high,X-high
		},
		{	// OrthoN_ABOVE
			Moore3N_ABOVE,				// B00,00 : Face
			Moore3N_ABOVEWEST,			// B00,01 : X-low
			Moore3N_ABOVEEAST,			// B00,10 : X-high
			Moore3NaN,					// B00,11
			Moore3N_ABOVENORTH,			// B01,00 : Y-low
			Moore3N_ABOVENORTHWEST,		// B01,01 : Y-low,X-low
			Moore3N_ABOVENORTHEAST,		// B01,10 : Y-low,X-high
			Moore3NaN,					// B01,11
			Moore3N_ABOVESOUTH,			// B10,00 : Y-high
			Moore3N_ABOVESOUTHWEST,		// B10,01 : Y-high,X-low
			Moore3N_ABOVESOUTHEAST		// B10,10 : Y-high,X-high
		},
		{	// OrthoN_BELOW
			Moore3N_BELOW,				// B00,00 : Face
			Moore3N_BELOWWEST,			// B00,01 : X-low
			Moore3N_BELOWEAST,			// B00,10 : X-high
			Moore3NaN,					// B00,11
			Moore3N_BELOWNORTH,			// B01,00 : Y-low
			Moore3N_BELOWNORTHWEST,		// B01,01 : Y-low,X-low
			Moore3N_BELOWNORTHEAST,		// B01,10 : Y-low,X-high
			Moore3NaN,					// B01,11
			Moore3N_BELOWSOUTH,			// B10,00 : Y-high
			Moore3N_BELOWSOUTHWEST,		// B10,01 : Y-high,X-low
			Moore3N_BELOWSOUTHEAST		// B10,10 : Y-high,X-high
		}
	};

	const unsigned char
		IsoSurfaceBuilder::MainVertexElements::_2dts23dts
			[CountOrthogonalNeighbors]
			[Count2DTouchSideElements] =
	{
		{	// OrthoN_NORTH
			T3DS_North,					// B00,00 : Face
			T3DS_NorthWest,				// B00,01 : X-low
			T3DS_NorthEast,				// B00,10 : X-high
			T3DS_None,					// B00,11
			T3DS_NorthNether,			// B01,00 : Y-low
			T3DS_NorthWestNether,		// B01,01 : Y-low,X-low
			T3DS_NorthEastNether,		// B01,10 : Y-low,X-high
			T3DS_None,					// B01,11
			T3DS_NorthAether,			// B10,00 : Y-high
			T3DS_NorthWestAether,		// B10,01 : Y-high,X-low
			T3DS_NorthEastAether		// B10,10 : Y-high,X-high
		},
		{	// OrthoN_East
			T3DS_East,					// B00,00 : Face
			T3DS_NorthEast,				// B00,01 : X-low
			T3DS_SouthEast,				// B00,10 : X-high
			T3DS_None,					// B00,11
			T3DS_NetherEast,			// B01,00 : Y-low
			T3DS_NorthEastNether,		// B01,01 : Y-low,X-low
			T3DS_SouthEastNether,		// B01,10 : Y-low,X-high
			T3DS_None,					// B01,11
			T3DS_AetherEast,			// B10,00 : Y-high
			T3DS_NorthEastAether,		// B10,01 : Y-high,X-low
			T3DS_SouthEastAether		// B10,10 : Y-high,X-high
		},
		{	// OrthoN_West
			T3DS_West,					// B00,00 : Face
			T3DS_NorthWest,				// B00,01 : X-low
			T3DS_SouthWest,				// B00,10 : X-high
			T3DS_None,					// B00,11
			T3DS_NetherWest,			// B01,00 : Y-low
			T3DS_NorthWestNether,		// B01,01 : Y-low,X-low
			T3DS_SouthWestNether,		// B01,10 : Y-low,X-high
			T3DS_None,					// B01,11
			T3DS_AetherWest,			// B10,00 : Y-high
			T3DS_NorthWestAether,		// B10,01 : Y-high,X-low
			T3DS_SouthWestAether		// B10,10 : Y-high,X-high
		},
		{	// OrthoN_South
			T3DS_South,					// B00,00 : Face
			T3DS_SouthWest,				// B00,01 : X-low
			T3DS_SouthEast,				// B00,10 : X-high
			T3DS_None,					// B00,11
			T3DS_SouthNether,			// B01,00 : Y-low
			T3DS_SouthWestNether,		// B01,01 : Y-low,X-low
			T3DS_SouthEastNether,		// B01,10 : Y-low,X-high
			T3DS_None,					// B01,11
			T3DS_SouthAether,			// B10,00 : Y-high
			T3DS_SouthWestAether,		// B10,01 : Y-high,X-low
			T3DS_SouthEastAether		// B10,10 : Y-high,X-high
		},
		{	// OrthoN_Aether
			T3DS_Aether,				// B00,00 : Face
			T3DS_AetherWest,			// B00,01 : X-low
			T3DS_AetherEast,			// B00,10 : X-high
			T3DS_None,					// B00,11
			T3DS_NorthAether,			// B01,00 : Y-low
			T3DS_NorthWestAether,		// B01,01 : Y-low,X-low
			T3DS_NorthEastAether,		// B01,10 : Y-low,X-high
			T3DS_None,					// B01,11
			T3DS_SouthAether,			// B10,00 : Y-high
			T3DS_SouthWestAether,		// B10,01 : Y-high,X-low
			T3DS_SouthEastAether		// B10,10 : Y-high,X-high
		},
		{	// OrthoN_Nether
			T3DS_Nether,				// B00,00 : Face
			T3DS_NetherWest,			// B00,01 : X-low
			T3DS_NetherEast,			// B00,10 : X-high
			T3DS_None,					// B00,11
			T3DS_NorthNether,			// B01,00 : Y-low
			T3DS_NorthWestNether,		// B01,01 : Y-low,X-low
			T3DS_NorthEastNether,		// B01,10 : Y-low,X-high
			T3DS_None,					// B01,11
			T3DS_SouthNether,			// B10,00 : Y-high
			T3DS_SouthWestNether,		// B10,01 : Y-high,X-low
			T3DS_SouthEastNether		// B10,10 : Y-high,X-high
		}
	};

	const unsigned char 
		IsoSurfaceBuilder::MainVertexElements::_tcg2gcg
			[CountOrthogonalNeighbors][10] = 
	{
		{	// OrthoN_NORTH
			0, 0, 0,
			2, 2,
			1, 1,
			4, 5, 6
		},
		{	// OrthoN_EAST
			0, 0, 0,
			3, 3,
			1, 1,
			4, 5, 6
		},
		{	// OrthoN_WEST

			0, 0, 0,
			3, 3,
			1, 1,
			4, 5, 6
		},
		{	// OrthoN_SOUTH
			0, 0, 0,
			2, 2,
			1, 1,
			4, 5, 6
		},
		{	// OrthoN_ABOVE
			0, 0, 0,
			2, 2,
			3, 3,
			4, 5, 6
		},
		{	// OrthoN_BELOW
			0, 0, 0,
			2, 2,
			3, 3,
			4, 5, 6
		}
	};

	size_t IsoSurfaceBuilder::MainVertexElements::computeTotalElements( const CubeDataRegionDescriptor & dgtmpl )
	{
		const size_t 
			d = dgtmpl.dimensions,
			dh = d >> 1,
			dh2 = dh - 1,
			oc = CountOrthogonalNeighbors,
			m3c = CountMoore3DEdges,
			mcc = CountMoore3DCorners;

		return 
			3*(d+1)*(d+1)*d +		// X/Y/Z axes
			(d+1)*(d+1)*(d+1) +		// Corner points

			oc*2*dh*dh2 +			// X/Y Half-resolution transition axes on each side sans edges and corners
			m3c*dh +				// X/Y Half-resolution transition axes on each edge sans corners
			oc*dh2*dh2 +			// Transition corner points on each side sans edges and corners
			m3c*dh2 +				// Transition corner points on each side edge sans corners
			mcc;					// Transition corner points on each side corner
	}

	IsoSurfaceBuilder::MainVertexElements::MainVertexElements( const CubeDataRegionDescriptor & dgtmpl )
		: IsoVertexElements(computeTotalElements(dgtmpl)),
			_cubemeta(dgtmpl), 
			remappings(new IsoVertexIndex[computeTotalElements(dgtmpl)]),
			trmappings(new IsoVertexIndex[computeTotalElements(dgtmpl)]),
			refinements(new IsoVertexIndex[computeTotalElements(dgtmpl)]),
			cellindices(new CellIndexPair[computeTotalElements(dgtmpl)]),
			trackFullOutsides(computeTotalElements(dgtmpl))			
	{
		memset(remappings, ~0, sizeof(*remappings) * count);
		memset(trmappings, ~0, sizeof(*trmappings) * count);
		memset(refinements, ~0, sizeof(*refinements) * count);

		const size_t 
			d = dgtmpl.dimensions,
			d2 = d - 1,
			dh = d >> 1,
			dh2 = dh - 1;

		IsoVertexIndex o = 0;
		size_t rlens[4];

		// Initialize iso vertex group offsets
		o = (rlens[2] = d*(d+1)*(d+1)) +			(_offsets.regular[2] = o);
		o = (rlens[1] = (d+1)*d*(d+1)) +			(_offsets.regular[1] = o);
		o = (rlens[3] = (d+1)*(d+1)*d) +			(_offsets.regular[3] = o);
		o = (rlens[0] = (d+1)*(d+1)*(d+1)) +		(_offsets.regular[0] = o);

		const IsoVertexIndex nRegularsCount = o;

		TransitionVertexGroupOffset
			* fullrestroffs_012,
			* fullrestroffs_34,
			* fullrestroffs_56;

		// Full-res vertex group offsets
		_offsets.transition[0] = 
		_offsets.transition[1] = 
		_offsets.transition[2] = fullrestroffs_012 = new TransitionVertexGroupOffset[CountOrthogonalNeighbors];
		_offsets.transition[3] = 
		_offsets.transition[4] = fullrestroffs_34 = new TransitionVertexGroupOffset[CountOrthogonalNeighbors];
		_offsets.transition[5] = 
		_offsets.transition[6] = fullrestroffs_56 = new TransitionVertexGroupOffset[CountOrthogonalNeighbors];

		// Half-res vertex group offsets
		_offsets.transition[7] = new TransitionVertexGroupOffset[CountMoore3DNeighbors];
		_offsets.transition[8] = new TransitionVertexGroupOffset[CountMoore3DEdges + CountOrthogonalNeighbors];
		_offsets.transition[9] = new TransitionVertexGroupOffset[CountMoore3DEdges + CountOrthogonalNeighbors];

		unsigned s;
		
		for (s = 0; s < CountOrthogonalNeighbors; ++s)
		{
			o = dh2*dh2 +	(_offsets.transition[7][s].o = o);
			o = dh*dh2 +	(_offsets.transition[8][s].o = o);
			o = dh2*dh +	(_offsets.transition[9][s].o = o);

#ifdef _DEBUG
			_offsets.transition[7][s].length = dh2*dh2;
			_offsets.transition[8][s].length = dh*dh2;
			_offsets.transition[9][s].length = dh2*dh;
#endif

			_offsets.transition[9][s].mx =
			_offsets.transition[7][s].mx = 1;
			_offsets.transition[8][s].mx = dh2;

			_offsets.transition[7][s].my = 
			_offsets.transition[9][s].my = dh2;
			_offsets.transition[8][s].my = 1;

			_offsets.transition[9][s].dy = 
			_offsets.transition[8][s].dx = 0;

			_offsets.transition[9][s].dx = 
			_offsets.transition[8][s].dy = 
			_offsets.transition[7][s].dx = 
			_offsets.transition[7][s].dy = 1;
		}
		for (s = BeginMoore3DEdges; s < BeginMoore3DEdges + CountMoore3DEdges; ++s)
		{
			o = dh2 +	(_offsets.transition[7][s].o = o);
			o = dh +	(
				_offsets.transition[8][s].o = 
				_offsets.transition[9][s].o = o
			);

#ifdef _DEBUG
			_offsets.transition[7][s].length = dh2;
			_offsets.transition[8][s].length = dh;
			_offsets.transition[9][s].length = dh;
#endif

			_offsets.transition[9][s].dx =
			_offsets.transition[8][s].dx =
			_offsets.transition[9][s].dy =
			_offsets.transition[8][s].dy = 0;

			_offsets.transition[7][s].dx =
			_offsets.transition[7][s].dy =

			_offsets.transition[9][s].mx =
			_offsets.transition[8][s].mx =
			_offsets.transition[7][s].mx =
			_offsets.transition[9][s].my =
			_offsets.transition[8][s].my =
			_offsets.transition[7][s].my = 1;
		}
		for (s = BeginMoore3DCorners; s < BeginMoore3DCorners + CountMoore3DCorners; ++s)
		{
			o = 1 +		(_offsets.transition[7][s].o = o);

#ifdef _DEBUG
			_offsets.transition[7][s].length = 1;
#endif

			_offsets.transition[7][s].dx =
			_offsets.transition[7][s].dy =
			_offsets.transition[7][s].mx =
			_offsets.transition[7][s].my = 0;
		}

		const static struct  
		{
			DimensionType
				nx, ny, nz;
		} gcgBounds[4] = 
		{
			{ _cubemeta.dimensions+1, _cubemeta.dimensions+1,	_cubemeta.dimensions+1 },
			{ _cubemeta.dimensions+1, _cubemeta.dimensions,		_cubemeta.dimensions+1 },
			{ _cubemeta.dimensions,	_cubemeta.dimensions+1,	_cubemeta.dimensions+1 },
			{ _cubemeta.dimensions+1, _cubemeta.dimensions+1,	_cubemeta.dimensions },
		};

		for (unsigned g = 0; g < 7; ++g)
		{
			for (s = 0; s < CountOrthogonalNeighbors; ++s)
			{
				const unsigned char gcg = _tcg2gcg[s][g];

				OgreAssert(gcg < 4, "2D Edge-Group to 3D Edge-Croup transform out-of-bounds");
				_offsets.transition[g][s].mx = 
					Mat2D3D[s].x.x*1 
					+ Mat2D3D[s].y.x*gcgBounds[gcg].nx 
					+ Mat2D3D[s].z.x*gcgBounds[gcg].ny*gcgBounds[gcg].nx;
				_offsets.transition[g][s].my = 
					Mat2D3D[s].x.y*1 
					+ Mat2D3D[s].y.y*gcgBounds[gcg].nx 
					+ Mat2D3D[s].z.y*gcgBounds[gcg].ny*gcgBounds[gcg].nx;

				_offsets.transition[g][s].o = 
					_offsets.regular[gcg] + 
					(Mat2D3D[s].x.d & (gcgBounds[gcg].nx - 1)) +
					(Mat2D3D[s].y.d & (gcgBounds[gcg].nx*(gcgBounds[gcg].ny-1))) +
						(Mat2D3D[s].z.d & (gcgBounds[gcg].nx*gcgBounds[gcg].ny*(gcgBounds[gcg].nz-1)));

#ifdef _DEBUG
				_offsets.transition[g][s].length = rlens[gcg] - (_offsets.transition[g][s].o - _offsets.regular[gcg]);
#endif
				_offsets.transition[g][s].dx = 
				_offsets.transition[g][s].dy =
					0;
			}
		}

		for (unsigned e = 0; e < 4; ++e)
		{
			const Shift3D & shift = _regeiivimx[e];
			for (DimensionType k = 0; k < _cubemeta.dimensions + shift.dz; ++k)
				for (DimensionType j = 0; j < _cubemeta.dimensions + shift.dy; ++j)
					for (DimensionType i = 0; i < _cubemeta.dimensions + shift.dx; ++i)
					{
						CellIndexPair & pair = cellindices[getRegularVertexIndex(e, i, j, k)];

						pair.corner0 = _cubemeta.getGridPointIndex(i, j, k);
						pair.corner1 = _cubemeta.getGridPointIndex(
							i + (shift.dx ^ 1), 
							j + (shift.dy ^ 1), 
							k + (shift.dz ^ 1)
						);
					}
		}

		for (unsigned s = 0; s < CountOrthogonalNeighbors; ++s)
		{
			const OrthogonalNeighbor on = OrthogonalNeighbor(s);

			for (unsigned e = 7; e < 10; ++e)
			{
				const DimensionType
					xx = _teidim1[e].dx << 1,
					yy = _teidim1[e].dy << 1;
				TransitionCell tc(_cubemeta, 1, on);

				for (tc.y = 0; tc.y < _cubemeta.dimensions + yy; tc.y += 2)
					for (tc.x = 0; tc.x < _cubemeta.dimensions + xx; tc.x += 2)
					{
						CellIndexPair & pair = cellindices[getTransitionIndex(on, tc.x, tc.y, e)];

						pair.corner0 = _cubemeta.getGridPointIndex(tc.coords(0,0));
						pair.corner1 = _cubemeta.getGridPointIndex(tc.coords(xx ^ 2, yy ^ 2));
					}
			}
		}

		OgreAssert(o == count, "Count and offset computation mismatch");
	}

	IsoSurfaceBuilder::MainVertexElements::~MainVertexElements()
	{
		delete [] remappings;
		delete [] trmappings;
		delete [] refinements;
		delete [] cellindices;

		delete [] _offsets.transition[2];
		delete [] _offsets.transition[4];
		delete [] _offsets.transition[6];
		delete [] _offsets.transition[7];
		delete [] _offsets.transition[8];
		delete [] _offsets.transition[9];
	}
	
#ifdef _DEBUG
#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::MainVertexElements::rollback()
	{
		IsoVertexElements::clear();
		trackFullOutsides.clear();
		// Refinements and mappings doesn't get cleared here, it's state must be preserved

		// Must be zero, operations are additive
		memset(normals, 0, sizeof(Vector3) * count);
#ifdef _DEBUG
		memset(positions, ~0, sizeof(Vector3) * count);
#endif
	}
#ifdef _DEBUG
#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::MainVertexElements::clear()
	{
		rollback();
		memset(refinements, ~0, sizeof(*refinements) * count);
		memset(remappings, ~0, sizeof(*remappings) * count);	// DEPS: restoreTransitionVertexMappings
		memset(trmappings, ~0, sizeof(*trmappings) * count);	// DEPS: restoreTransitionVertexMappings
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	Ogre::IsoVertexIndex IsoSurfaceBuilder::MainVertexElements::getTransitionIndex( const TransitionCell & tc, const TransitionVRECaCC & vrec ) const
	{
		IsoVertexIndex ivi;
		Touch2DSide side;
		computeTransitionIndexProperties(tc, vrec, ivi, side); // TODO: Remove 0.0
		return ivi;
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	Ogre::IsoVertexIndex IsoSurfaceBuilder::MainVertexElements::getTransitionIndex( const OrthogonalNeighbor on, const DimensionType x, const DimensionType y, const unsigned char ei ) const
	{
		IsoVertexIndex ivi;
		TouchStatus tsx, tsy;
		Touch2DSide rside;
		_cubemeta.computeTouchProperties(x, y, tsx, tsy, rside);
		computeTransitionIndexProperties(on, x, y, ei, tsx, tsy, rside, ivi, rside);
		return ivi;
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::MainVertexElements::computeTransitionIndexProperties( const TransitionCell & tc, const TransitionVRECaCC & vrec, IsoVertexIndex & ivi, Touch2DSide & rside ) const
	{
		DimensionType x, y;
		computeTransitionVertexCoordinates(tc, vrec, x, y);
		TouchStatus tsx, tsy;
		_cubemeta.computeTouchProperties(x, y, tsx, tsy, rside);
		computeTransitionIndexProperties(tc.side, x, y, vrec.getEdgeCode(), tsx, tsy, rside, ivi, rside);
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::MainVertexElements::computeTransitionIndexProperties
	( 
		const OrthogonalNeighbor tcside, 
		const DimensionType x, const DimensionType y, 
		const register unsigned char ei, 
		const TouchStatus tsx, const TouchStatus tsy, 
		const Touch2DSide side, 
		IsoVertexIndex & ivi, 
		Touch2DSide & rside 
	) const
	{
		OgreAssert(tcside < CountOrthogonalNeighbors && tcside >= 0, "Invalid orthogonal neighbor");

		const register unsigned 
			gf = getGroupFlag(ei),		// Set if ei={7,8,9}
			dlod = gf,
			igf = gf^1;					// Set if ei={0,1,2,3,4,5,6}

		rside = refine2DTouchSide(side, ei);

		const Moore3DNeighbor 				m3n = Moore3DNeighbor(_2dts2m3n[tcside][(igf-1) & rside]);				// Convert to more compact Moore3D indexing scheme
		const TransitionVertexGroupOffset & group = _offsets.transition[ei][m3n];
		const IsoVertexIndex go =
			(igf | (((tsx >> 1) | tsx) ^ 1) & 0x1) * ((x >> dlod) - group.dx) * group.mx + 
			(igf | (((tsy >> 1) | tsy) ^ 1) & 0x1) * ((y >> dlod) - group.dy) * group.my;

		OgreAssert(go < group.length, "Iso-vertex group offset overrun");
		ivi = group.o + go;
	}

	Ogre::Log::Stream & operator << ( Log::Stream & s, const IsoSurfaceBuilder::GridCell & gc )
	{
		return s << '<' << gc.x << ',' << gc.y << ',' << gc.z << '>';
	}

	Ogre::Log::Stream & operator<<( Log::Stream & s, const IsoSurfaceBuilder::TransitionCell & tc )
	{
		return s << '<' << tc.x << ',' << tc.y << '|' << Neighborhood::name(tc.side) << '>';
	}


#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	IsoSurfaceBuilder::TriangleWinder::TriangleWinder( const OrthogonalNeighbor on )
		: _on(on) 
	{
		// Test a flag at position 'on' and return 3 if set otherwise 0
		_onwn = (_triwindflags >> on) & 1;
		_onwn |= (_onwn << 1) & 2;
	}
	
#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::TransitionTriangleBuilder::operator << ( const NonTrivialTransitionCase & caze )
	{
		const signed char cTranCellClass = transitionCellClass[caze.casecode];
		const TransitionCellData & oCase = transitionCellData[cTranCellClass & 0x7F];	// High-bit must be cleared

		_tc = caze.cell;
		_winder << caze;
		_vcount = oCase.GetTriangleCount() * 3;
		_indices = oCase.vertexIndex;

		const unsigned short * vVertices = getVertexData(caze);
		const unsigned int nVertCount = getVertexCount(caze);
		IsoVertexIndex coarse, refined, mapped;

		for (unsigned int c = 0; c < nVertCount; ++c)
		{
			coarse = _vtxelems->getTransitionIndex(_tc, TransitionVRECaCC(vVertices[c]));
			refined = _vtxelems->refinements[coarse];

			OgreAssert(refined != IsoVertexIndex(~0), "Unmapped refinement");
			if (_vtxelems->trmappings[refined] != IsoVertexIndex(~0))
				mapped = _vtxelems->trmappings[refined];
			else
				mapped = refined;

			_vertices[c] = mapped;
		}
	}

#ifdef _DEBUG
	#pragma optimize("gtpy", on)
#endif
	void IsoSurfaceBuilder::RegularTriangleBuilder::operator << ( const NonTrivialRegularCase & caze )
	{
		const RegularCellData & oCase = regularCellData[regularCellClass[caze.casecode]];

		_gc = caze.cell;
		_indices = oCase.vertexIndex;
		_vcount = oCase.GetTriangleCount() * 3;

		const unsigned short * vVerticies = getVertexData(caze);
		const unsigned int nVertCount = getVertexCount(caze);
		IsoVertexIndex coarse, refined, mapped;

		for (unsigned int c = 0; c < nVertCount; ++c)
		{
			coarse = _vtxelems->getRegularVertexIndex(_gc, VRECaCC(vVerticies[c]), _lod);
			refined = _vtxelems->refinements[coarse];

			OgreAssert(refined != IsoVertexIndex(~0), "Unmapped refinement");
			if (_vtxelems->remappings[refined] != IsoVertexIndex(~0))
				mapped = _vtxelems->remappings[refined];
			else
				mapped = refined;

			_vertices[c] = mapped;
		}	
	}


#if defined(_DEBUG) || defined(_OHT_LOG_TRACE)
	IsoSurfaceBuilder::DebugInfo::DebugInfo( const IsoSurfaceRenderable * pISR ) 
	: name(pISR->getName()), center(pISR->getWorldBoundingBox(true).getCenter())
	{}
#endif


	IsoSurfaceBuilder::GridCell::GridCell( const GridCell & copy ) 
	: _cubemeta(copy._cubemeta), _lod(copy._lod), x(copy.x), y(copy.y), z(copy.z)
	{
		corners.setParent(this);
	}

	IsoSurfaceBuilder::GridCell::GridCell( const Voxel::CubeDataRegionDescriptor & dgtmpl, const unsigned short nLOD ) 
	: _cubemeta(dgtmpl), _lod(nLOD), x(0), y(0), z(0)
	{
		corners.setParent(this);
	}

	IsoSurfaceBuilder::GridCell::GridCell( const Voxel::CubeDataRegionDescriptor & dgtmpl, const unsigned short nLOD, const DimensionType x, const DimensionType y, const DimensionType z ) 
	: _cubemeta(dgtmpl), _lod(nLOD), x(x), y(y), z(z)
	{
		corners.setParent(this);
	}

	IsoSurfaceBuilder::GridCell::GridCell( const Voxel::CubeDataRegionDescriptor & dgtmpl, const unsigned short nLOD, const GridCellCoords & gcc ) 
	: _cubemeta(dgtmpl), _lod(nLOD), x(gcc.i), y(gcc.j), z(gcc.k)
	{
		corners.setParent(this);
	}

	IsoSurfaceBuilder::iterator_GridCells::Advance IsoSurfaceBuilder::iterator_GridCells::computeAdvanceCorners(const unsigned short nLOD, const Voxel::CubeDataRegionDescriptor & cdrd)
	{
		const CubeDataRegionDescriptor::IndexTx t = cdrd.coordsIndexTx;
		const int cell = 1 << nLOD;
		Advance advance;

		advance.mx = cell*t.mx;
		advance.my = cell*t.my - 2*cell*t.mx;
		advance.mz = cell*t.mz - 2*cell*t.my;

		return advance;
	}

	IsoSurfaceBuilder::iterator_GridCells::Advance IsoSurfaceBuilder::iterator_GridCells::computeAdvanceCells( const unsigned short nLOD, const Voxel::CubeDataRegionDescriptor & cdrd )
	{
		const CubeDataRegionDescriptor::IndexTx 
			tr = cdrd.coordsIndexTx;
		const VoxelIndex 
			dim = cdrd.dimensions,
			dim1 = cdrd.dimensions + 1,
			cell = 1 << nLOD;
		Advance advance;

		advance.mx = cell*tr.mx - 2*cell*tr.mz;
		advance.my = -dim*tr.mx + cell*tr.my;
		advance.mz = -dim*tr.my + cell*tr.mz;

		return advance;
	}

	IsoSurfaceBuilder::iterator_GridCells::iterator_GridCells( const unsigned short nLOD, const Voxel::CubeDataRegionDescriptor & cdrd )
		: _result(GridCell(cdrd, nLOD)), _cdrd(cdrd), _span(1 << nLOD), _advanceCorners(computeAdvanceCorners(nLOD, cdrd)), _advanceCells(computeAdvanceCells(nLOD, cdrd))
	{
		OHT_CR_INIT();
		process();
	}

	IsoSurfaceBuilder::iterator_GridCells::iterator_GridCells( const iterator_GridCells & copy )
		: _result(copy._result), _span(copy._span), _cdrd(copy._cdrd), _advanceCorners(copy._advanceCorners), _advanceCells(copy._advanceCells), OHT_CR_COPY(copy)
	{}

	void IsoSurfaceBuilder::iterator_GridCells::process()
	{
		VoxelIndex & index = _result.index;
		size_t & corner = _result.corner;
		GridCell & gc = _result.gc;

		OHT_CR_START();

		index = 0;

		for (gc.z = 0; gc.z < _cdrd.dimensions; gc.z += _span, index += _advanceCells.mz)
		{
			for (gc.y = 0; gc.y < _cdrd.dimensions; gc.y += _span, index += _advanceCells.my)
			{
				for (gc.x = 0; gc.x < _cdrd.dimensions; gc.x += _span, index += _advanceCells.mx)
				{
					corner = 0;
					OHT_CR_RETURN_VOID(CRS_0);
					corner++;
					index += _advanceCorners.mx;
					OHT_CR_RETURN_VOID(CRS_1);
					corner++;
					index += _advanceCorners.mx + _advanceCorners.my;
					OHT_CR_RETURN_VOID(CRS_2);
					corner++;
					index += _advanceCorners.mx;
					OHT_CR_RETURN_VOID(CRS_3);
					corner++;
					index += _advanceCorners.mx + _advanceCorners.my + _advanceCorners.mz;
					OHT_CR_RETURN_VOID(CRS_4);
					corner++;
					index += _advanceCorners.mx;
					OHT_CR_RETURN_VOID(CRS_5);
					corner++;
					index += _advanceCorners.mx + _advanceCorners.my;
					OHT_CR_RETURN_VOID(CRS_6);
					corner++;
					index += _advanceCorners.mx;
					OHT_CR_RETURN_VOID(CRS_7);
					corner++;
					index += _advanceCorners.mx + _advanceCorners.my + _advanceCorners.mz;
				}
			}
		}

		OHT_CR_END();
	}


	IsoSurfaceBuilder::iterator_GridCells::Result::Result( const GridCell & gc ) 
		: gc(gc)
	{}

	IsoSurfaceBuilder::iterator_GridCells::Result::Result( const Result & copy )
		: gc(copy.gc), index(copy.index), corner(copy.corner)
	{}

}/// namespace Ogre
