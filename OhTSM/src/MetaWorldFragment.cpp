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

#include "pch.h"

#include "MetaWorldFragment.h"
#include "CubeDataRegion.h"
#include "MetaObject.h"
#include "IsoSurfaceBuilder.h"
#include "IsoSurfaceRenderable.h"
#include "OverhangTerrainManager.h"
#include "DebugTools.h"
#include "MetaFactory.h"
#include "Util.h"
#include "OverhangTerrainListener.h"

namespace Ogre
{
	using namespace Voxel;
	using namespace HardwareShadow;

	namespace MetaFragment 
	{
		Post::Post(Voxel::CubeDataRegion * pCubeDataRegoin, const YLevel & ylevel /*= YLevel()*/) 
		: block(pCubeDataRegoin), ylevel(ylevel), surface(NULL), custom(NULL)
		{

		}

		Core::Core( const MetaVoxelFactory * pFactory, Voxel::CubeDataRegion * pBlock, const YLevel & ylevel )
			:	_bResetting(false), _pSceneNode(NULL),
				Post(pBlock, ylevel),

			factory(pFactory),
			_pMatInfo(NULL), _nLOD_Requested0(~0), _enStitches_Requested0((Touch3DFlags)~0), _ridBuilderLast(~0)
		{
			memset(_vpNeighbors, 0, sizeof(Container *) * 6);
		}

		void Core::initialise( const Camera * pPrimaryCam, SceneNode * pSceneNode, const String & sSurfName /*= StringUtil::BLANK*/ )
		{
			oht_assert_threadmodel(ThrMdl_Main);
			OgreAssert(surface == NULL && _pSceneNode == NULL, "Surface already initialized");
			if (_pMatInfo != NULL)
			{
				_pMaterial = factory->base->acquireMaterial(_pMatInfo->name, _pMatInfo->group);
				delete _pMatInfo;
				_pMatInfo = NULL;
			}

			Container * pMWFThis = static_cast< Container * > (this);

			OgreAssert(pMWFThis != NULL, "This must be an instance of a meta-fragment");

			surface = factory->createIsoSurfaceRenderable(pMWFThis, sSurfName);
			surface->initLODMetrics(pPrimaryCam);
			if (!_pMaterial.isNull())
				surface->setMaterial(_pMaterial);

			_pSceneNode = pSceneNode;
			_pSceneNode->attachObject(surface);
		}

		// TODO: Verify that the associated onDestroy event is called first
		Core::~Core()
		{
			oht_assert_threadmodel(ThrMdl_Single);

			OgreAssert(_pSceneNode == NULL, "Scene node must be detached first");

			OHT_DBGTRACE("Delete " << this);
			delete block;
			delete _pMatInfo;
		}

		void Core::addMetaObject( MetaObject * const mo )
		{ 
			oht_assert_threadmodel(ThrMdl_Single);
			_vMetaObjects.push_back(mo); 
		}

		bool Core::removeMetaObject( const MetaObject * const mo )
		{
			for (MetaObjsList::const_iterator i = _vMetaObjects.begin(); i != _vMetaObjects.end(); ++i)
				if (*i == mo)
				{
					_vMetaObjects.erase(i);
					return true;
				}
			return false;
		}

		void Core::clearMetaObjects()
		{
			oht_assert_threadmodel(ThrMdl_Single);
			_vMetaObjects.clear();
		}

		///Updates IsoSurface
		void Core::updateSurface()
		{
			oht_assert_threadmodel(ThrMdl_Main);
			OgreAssert(surface != NULL, "Surface not initialized");

			surface->deleteGeometry();
			_bResetting = false;
			if (_ridBuilderLast != ~0)
				factory->base->getIsoSurfaceBuilder()->cancelBuild(_ridBuilderLast);

			_ridBuilderLast = ~0;
			_nLOD_Requested0 = ~0;
			_enStitches_Requested0 = (Touch3DFlags)~0;
		}

		bool Core::generateConfiguration( const unsigned nLOD, const Touch3DFlags enStitches )
		{
			oht_assert_threadmodel(ThrMdl_Main);
			OgreAssert(surface != NULL, "Surface not initialized");

			if (_bResetting)
			{
				surface->deleteGeometry();
				_bResetting = false;
			}

			if (!surface->isConfigurationBuilt(nLOD, enStitches))
			{
				OHTDD_Translate(-surface->getWorldBoundingBox(true).getCenter());
				OHTDD_Coords(OCS_World, OCS_DataGrid);

				factory->base->getIsoSurfaceBuilder()->build(block, surface, nLOD, enStitches);
				_nLOD_Requested0 = nLOD;
				_enStitches_Requested0 = enStitches;
			}
			return true;
		}

		bool Core::requestConfiguration( const unsigned nLOD, const Touch3DFlags enStitches )
		{
			oht_assert_threadmodel(ThrMdl_Main);	// Main thread to re-use tracking vars
			OgreAssert(surface != NULL, "Surface not initialized");
			IsoSurfaceBuilder * pISB = factory->base->getIsoSurfaceBuilder();

			if (_bResetting)
			{
				surface->deleteGeometry();
				_bResetting = false;
				if (_ridBuilderLast != ~0)
					pISB->cancelBuild(_ridBuilderLast);

				_ridBuilderLast = ~0;
				_nLOD_Requested0 = ~0;
				_enStitches_Requested0 = (Touch3DFlags)~0;
			}

			if (!surface->isConfigurationBuilt(nLOD, enStitches))
			{
				HardwareIsoVertexShadow::ConsumerLock lock = surface->getShadow() ->requestConsumerLock(nLOD, enStitches);

				if (lock)
				{
					surface->populateBuffers(lock.openQueue());
					return true;
				} else
					if (_nLOD_Requested0 != nLOD || _enStitches_Requested0 != enStitches)	// IsoSurfaceBuilder is busy, no data available yet
					{
						if (_ridBuilderLast != ~0)
							pISB->cancelBuild(_ridBuilderLast);

						_ridBuilderLast = pISB->requestBuild(block, surface, nLOD, enStitches);
						_nLOD_Requested0 = nLOD;
						_enStitches_Requested0 = enStitches;
					}

					return false;
			} else
				return true;
		}

		DataAccessor::EmptySet Core::updateGrid()
		{
			oht_assert_threadmodel(ThrMdl_Background);

			DataAccessor access = block->lease();

			access.reset();
			for(MetaObjsList::iterator it = _vMetaObjects.begin(); it != _vMetaObjects.end(); ++it)
			{
				(*it)->updateDataGrid(block, &access);
			}

			if (block->hasGradient())
				access.updateGradient();

			_bResetting = true;

			return access.getEmptyStatus();
		}

		StreamSerialiser & Core::operator >> (StreamSerialiser & output) const
		{
			oht_assert_threadmodel(ThrMdl_Single);
			String sMatName, sMatGroup;

			if (_pMatInfo != NULL)
			{
				sMatName = _pMatInfo->name;
				sMatGroup = _pMatInfo->group;
			}
			if (!_pMaterial.isNull())
			{
				sMatName = _pMaterial->getName();
				sMatGroup = _pMaterial->getGroup();
			}
			/*
			#ifdef _DEBUG
			if (mMaterial.isNull())
			OHT_DBGTRACE("Material is null!");
			#endif // _DEBUG*/

			output.write(&ylevel);
			output.write(&sMatName);
			output.write(&sMatGroup);
			OgreAssert(block != NULL, "Data grid was unset");
			bool bCustomData = custom != NULL;

			output.write(&bCustomData);

			if (bCustomData)
				*custom >> output;
			*block >> output;

			//OHT_DBGTRACE("wrote y-level=" << _ylevel << ", material=" << sMatName);
			return output;
		}
		StreamSerialiser & Core::operator << (StreamSerialiser & input)
		{
			oht_assert_threadmodel(ThrMdl_Single);
			String sMatName, sMatGroup;

			if (_pMatInfo == NULL)
				_pMatInfo = new Core::MaterialInfo;

			// TODO: Put CRC / version checks in, otherwise in the case of corrupt data is a pain to debug
			input.read(&ylevel);
			input.read(&_pMatInfo->name);
			input.read(&_pMatInfo->group);
			OgreAssert(block != NULL, "Data grid was unset");

			bool bCustomData;
			input.read(&bCustomData);
			if (bCustomData)
			{
				OgreAssert(custom != NULL, "Custom data was unset");
				*custom << input;
			}
			*block << input;

			//OHT_DBGTRACE("read y-level=" << _ylevel << ", material=" << sMatName);
			return input;
		}

		void Core::setMaterial( const MaterialPtr & pMat )
		{
			oht_assert_threadmodel(ThrMdl_Main);
			delete _pMatInfo;
			_pMatInfo = NULL;
			_pMaterial = pMat;
			if (surface != NULL)
				surface->setMaterial(_pMaterial);
		}

		void Core::linkNeighbor( const OrthogonalNeighbor on, Container * pMWF )
		{
			using Neighborhood::opposite;

			oht_assert_threadmodel(ThrMdl_Single);
			OgreAssert(
				pMWF != NULL &&
				(
					_vpNeighbors[on] == pMWF
					|| _vpNeighbors[on] == NULL
				),

				"Expected either unset or same neighbor"
			);

			_vpNeighbors[on] = pMWF;

			static_cast< Core * > (pMWF)->_vpNeighbors[opposite(on)] = this;
		}
		void Core::unlinkNeighbor(const VonNeumannNeighbor vnnNeighbor)
		{
			oht_assert_threadmodel(ThrMdl_Main);

			if (_vpNeighbors[vnnNeighbor] != NULL)
			{
				_vpNeighbors[vnnNeighbor] = 
					_vpNeighbors[vnnNeighbor] ->_vpNeighbors[Neighborhood::opposite(vnnNeighbor)] = NULL;
			}
		}

		const MaterialPtr & Core::getMaterial() const
		{
			return _pMaterial;
		}

		void Core::detachFromScene()
		{
			oht_assert_threadmodel(ThrMdl_Main);

			if (surface != NULL)
				surface->detachFromParent();

			if (_pSceneNode != NULL)
			{
				_pSceneNode->detachAllObjects();
				_pSceneNode->getCreator()->destroySceneNode(_pSceneNode);
				_pSceneNode = NULL;
			}
		}

		Ogre::Touch3DFlags Core::getNeighborFlags( const unsigned nLOD ) const
		{
			oht_assert_threadmodel(ThrMdl_Main);
			size_t nTransitionFlags = 0;

			for (int c = 0; c < CountOrthogonalNeighbors; ++c)
			{
				const OrthogonalNeighbor enon = static_cast< OrthogonalNeighbor > (c);
				const Core * pMWFNeighbor = _vpNeighbors[enon];

				if (pMWFNeighbor != NULL)
				{
					const LODRenderable * pRendC = pMWFNeighbor->surface;

					if (pRendC != NULL && pRendC ->getEffectiveRenderLevel() < (int)nLOD)
					{
						//OgreAssert(nLOD - pRendC->getEffectiveRenderLevel() <= 1, "LOD delta too high");
						nTransitionFlags |= OrthogonalNeighbor_to_Touch3DSide[enon];
					}
				}
			};
			return Touch3DFlags(nTransitionFlags);
		}

		std::pair< bool, Real > Core::rayQuery( const Ray & ray, const Real limit )
		{
			oht_assert_threadmodel(ThrMdl_Main);
			OgreAssert(surface != NULL, "Must be initialised before performing a ray query");

			const unsigned nLOD = surface->getEffectiveRenderLevel();
			const Touch3DFlags t3dFlags = getNeighborFlags(nLOD);

			if (_bResetting)
				generateConfiguration(nLOD, t3dFlags);

			return factory->base->getIsoSurfaceBuilder()
				->rayQuery(
					limit,
					factory->channel,
					block,
					ray,
					surface->getShadow(), nLOD, t3dFlags
				);
		}

		void Core::bindCamera( const Camera * pCam )
		{
			oht_assert_threadmodel(ThrMdl_Main);
			if (surface != NULL)
				surface->initLODMetrics(pCam);
		}

		bool Core::isInitialized() const
		{
			return surface != NULL && _pSceneNode != NULL;
		}

		const Container * Core::neighbor( const Moore3DNeighbor enNeighbor ) const
		{
			return dynamic_cast< const Container * > (_vpNeighbors[enNeighbor]);
		}

		Container * Core::neighbor( const Moore3DNeighbor enNeighbor )
		{
			return dynamic_cast< Container * > (_vpNeighbors[enNeighbor]);
		}

		namespace Interfaces
		{
			ReadOnly_facet::ReadOnly_facet( const Core * pCore, const Post * pPost ) 
			: _core (pCore)
			{
			}

			ReadOnly_facet::ReadOnly_facet( ReadOnly_facet && move ) : _core(move._core)
			{

			}

			Shared::Shared( boost::shared_mutex & m, const Core * pCore, const Post * pPost ) 
				: SharedLockable(m), const_Base(pCore, pPost)
			{

			}

			Shared::Shared( Shared && move ) 
				:	SharedLockable(static_cast< SharedLockable && > (move)),
					const_Base(static_cast< const_Base && > (move))
			{

			}

			Unique_base::Unique_base( Core * pCore, Post * pPost ) 
			: template_Base(pCore, pPost)
			{

			}

			Unique_base::Unique_base( Unique_base && move ) : Base(static_cast< Base && > (move))
			{

			}

			Unique::Unique( boost::shared_mutex & m, Core * pCore, Post * pPost ) 
				: UniqueLockable(m), Unique_base(pCore, pPost)
			{

			}

			Unique::Unique( Unique && move ) :	Unique_base(static_cast< Unique_base && > (move)),
				UniqueLockable(static_cast< UniqueLockable && > (move))
			{

			}

			Upgradable::Upgradable( boost::shared_mutex & m, Core * pCore, Post * pPost ) 
				:	UpgradableLockable(m), 
					const_Base(pCore, pPost),
					_core(pCore), _post(pPost)
			{

			}

			Upgradable::Upgradable( Upgradable && move ) :	const_Base(static_cast< const_Base && > (move)),
				UpgradableLockable(static_cast< UpgradableLockable && > (move)),
				_core(move._core), _post(move._post)
			{

			}

			Upgraded Upgradable::upgrade()
			{
				return Upgraded(_lock, _core, _post);
			}

			Upgraded::Upgraded( UpgradableLockable::LockType & lock, Core * pCore, Post * pPost ) 
				: UpgradedLockable(lock), Unique_base(pCore, pPost)
			{

			}

			Upgraded::Upgraded( Upgraded && move )
				:	Unique_base(static_cast< Unique_base && > (move)),
					UpgradedLockable(static_cast< UpgradedLockable && > (move))
			{

			}

			Builder::Builder( Core * pCore, Post * pPost ) 
			: template_Base(pCore, pPost)
			{

			}

			Builder::Builder( Builder && move )
			: template_Base(static_cast< template_Base && > (move))
			{

			}

			const_Basic::const_Basic( const Core * pCore, const Post * pPost ) : Basic_base(pCore, pPost)
			{

			}

			const_Basic::const_Basic( const Basic & copy ) : Basic_base(copy)
			{

			}

			Basic::Basic( Core * pCore, Post * pPost ) : Basic_base(pCore, pPost)
			{

			}
		}


		Container::Container(const Voxel::MetaVoxelFactory * pFact, Voxel::CubeDataRegion * pDG, const YLevel yl /*= YLevel()*/)
			: 	Core(pFact, pDG, yl), factory(pFact)

		{
		}

		Container::~Container()
		{

		}
	}
}/// namespace Ogre
