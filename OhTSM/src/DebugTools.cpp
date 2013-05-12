/*
-----------------------------------------------------------------------------
This source file is part of the Overhang Terrain Scene Manager library (OhTSM)
for use with OGRE.

Copyright (c) 2013 extollIT Enterprises
http://www.extollit.com

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

You may alternatively use this source under the terms of a specific version of
the OGRE Unrestricted License provided you have obtained such a license from
Torus Knot Software Ltd.
-----------------------------------------------------------------------------
*/

#include "pch.h"

#include <OgreException.h>

#include <strstream>

#include "OverhangTerrainSceneManager.h"
#include "OverhangTerrainManager.h"
#include "DebugTools.h"

namespace Ogre
{
	ThreadingModelManager gtmm_ThreadingModelManager;

	ThreadingModelManager::AssertEx::AssertEx( const char * szMsg )
	: std::exception(szMsg) {}

#ifdef NDEBUG
	#define ASSERTION(x,y) { if (!(x)) throw ThreadingModelManager::AssertEx(y); }
#else
	#define ASSERTION(x,y) { OgreAssert(x, y); }
#endif // NDEBUG

	ThreadingModelMonitor::ThreadingModelMonitor( const char * szFuncName, const char * szFuncFile, const size_t nFuncFileLine, const void * oInst, const ThreadingModel entmThreadingModel ) 
		: mThreadingModel(entmThreadingModel), mThreadID(boost::this_thread::get_id()), mFuncID(buildFuncID(szFuncName, szFuncFile, nFuncFileLine)), mInst(oInst)
	{
		const char * szFuncID = mFuncID.c_str();
		if (mThreadingModel == ThrMdl_Main)
		{
			if (!gtmm_ThreadingModelManager.checkMainThread(szFuncID))
				OHT_LOG_FATAL("Only the MAIN thread may enter this block");

			ASSERTION(gtmm_ThreadingModelManager.checkMainThread(szFuncID), "Only the MAIN thread may enter this block");
		}
		else
		{
			const size_t nRefCount = gtmm_ThreadingModelManager.registerThread(szFuncID, oInst);

			if (!(!gtmm_ThreadingModelManager.checkMainThread(szFuncID) || mThreadingModel != ThrMdl_Background))
				OHT_LOG_FATAL("Only a BACKGROUND thread may enter this block");

			ASSERTION (!gtmm_ThreadingModelManager.checkMainThread(szFuncID) || mThreadingModel != ThrMdl_Background, "Only a BACKGROUND thread may enter this block");

			if (nRefCount > 1 && mThreadingModel == ThrMdl_Single)
			{
				if (mThreadingModel != ThrMdl_Any)
					OHT_LOG_FATAL("Threading model is SINGLE but this method is not synchronized");

				ASSERTION(mThreadingModel == ThrMdl_Any, "Threading model is SINGLE but this method is not synchronized");
			}
		}
	}

	ThreadingModelMonitor::~ThreadingModelMonitor()
	{
		if (mThreadingModel != ThrMdl_Main)
		{
			const size_t nRefCount = gtmm_ThreadingModelManager.deregisterThread(mFuncID.c_str(), mInst);
			if (!(nRefCount == 0 || mThreadingModel == ThrMdl_Any))
				OHT_LOG_FATAL("Threading model is SINGLE or MAIN but this method is not synchronized");

			ASSERTION(nRefCount == 0 || mThreadingModel == ThrMdl_Any, "Threading model is SINGLE or MAIN but this method is not synchronized");
		}
	}

	std::string ThreadingModelMonitor::buildFuncID( const char * szFuncName, const char * szFuncFile, const size_t nFuncFileLine )
	{
		std::strstream ss;

		ss << szFuncFile << ':' << nFuncFileLine << ' ' << szFuncName << std::ends;
		return ss.str();
	}

	void ThreadingModelManager::registerMainThread()
	{
		{ boost::mutex::scoped_lock lock(mutex);
			if (mMainThreadSet)
				OHT_LOG_FATAL("Main thread already registered");
			OgreAssert(!mMainThreadSet, "Main thread already registered");
			mMainThread = boost::this_thread::get_id();
			mMainThreadSet = true;
		}
	}

	bool ThreadingModelManager::checkMainThread( const char * szFuncName )
	{
		{ boost::mutex::scoped_lock lock(mutex);
			return mMainThreadSet && boost::this_thread::get_id() == mMainThread && mThreadTracker.find(szFuncName) == mThreadTracker.end();
		}
	}

	size_t ThreadingModelManager::registerThread (const char * szFuncName, const void * pThis)
	{
		{ boost::mutex::scoped_lock lock(mutex);
			ThreadConcurrencyMap::iterator i = mThreadTracker.find(szFuncName);

			if (i == mThreadTracker.end())
				i = mThreadTracker.insert(mThreadTracker.begin(), std::pair< std::string, ThreadIDSet > (szFuncName, ThreadIDSet()));

			i->second.insert(boost::this_thread::get_id());

			std::pair< const char *, const void * > hCallHandle(szFuncName, pThis);
			ThreadReferenceCountMap::iterator j = mRefCounter.find(hCallHandle);

			if (j == mRefCounter.end())
				return mRefCounter[hCallHandle] = 1;
			else
			{
				return ++j->second;
			}
		}
	}
	size_t ThreadingModelManager::deregisterThread(const char * szFuncName, const void * pThis)
	{
		{ boost::mutex::scoped_lock lock(mutex);
			std::pair< const char *, const void * > hCallHandle(szFuncName, pThis);
			ThreadReferenceCountMap::iterator j = mRefCounter.find(hCallHandle);

			if (j == mRefCounter.end())
			{
				if (j == mRefCounter.end())
					OHT_LOG_FATAL("Impossible, no reference encountered in DTOR");
				OgreAssert(j != mRefCounter.end(), "Impossible, no reference encountered in DTOR");
			}
			else
			{
				const size_t nRef = --j->second;
				if (nRef == 0)
					mRefCounter.erase(j);
				return nRef;
			}
			return 0;
		}
	}

	ThreadingModelManager::ThreadingModelManager() 
		: mMainThreadSet(false) {}

	DebugDisplay gdd_DebugDisplay;

	DebugDisplay::DebugDisplay()
		: _pScNode(NULL), _pSceneManager(NULL)
	{
		_smcMaterialStack.push(MC_Red);
	}

	DebugDisplay::~DebugDisplay()
	{
		for (TransformStack::iterator i = _trans.begin(); i != _trans.end(); ++i)
			delete *i;
	}

	void DebugDisplay::init( OverhangTerrainSceneManager * pSceneManager)
	{
		oht_assert_threadmodel(ThrMdl_Main);

		_pSceneManager = pSceneManager;

		MaterialManager & matman = MaterialManager::getSingleton();
		MaterialPtr pMat;
		TextureUnitState * pTexUnit;
		Pass * pPass;

		pMat = matman.create("Debug/Red", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		pPass = pMat->getTechnique(0) ->getPass(0);
		pPass->setPointSize(5.0f);
		pTexUnit = pPass->createTextureUnitState("color");
		pTexUnit->setColourOperationEx(LBX_SOURCE1, LBS_MANUAL, LBS_CURRENT, ColourValue::Red);
		_pMaterials[MC_Red] = pMat;

		pMat = matman.create("Debug/Green", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		pPass = pMat->getTechnique(0) ->getPass(0);
		pPass->setPointSize(5.0f);
		pTexUnit = pPass->createTextureUnitState("color");
		pTexUnit->setColourOperationEx(LBX_SOURCE1, LBS_MANUAL, LBS_CURRENT, ColourValue::Green);
		_pMaterials[MC_Green] = pMat;

		pMat = matman.create("Debug/Blue", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		pPass = pMat->getTechnique(0) ->getPass(0);
		pPass->setPointSize(5.0f);
		pTexUnit = pPass->createTextureUnitState("color");
		pTexUnit->setColourOperationEx(LBX_SOURCE1, LBS_MANUAL, LBS_CURRENT, ColourValue::Blue);
		_pMaterials[MC_Blue] = pMat;

		pMat = matman.create("Debug/Yellow", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		pPass = pMat->getTechnique(0) ->getPass(0);
		pPass->setPointSize(5.0f);
		pTexUnit = pPass->createTextureUnitState("color");
		pTexUnit->setColourOperationEx(LBX_SOURCE1, LBS_MANUAL, LBS_CURRENT, ColourValue(1.0f, 1.0f, 0.0f));
		_pMaterials[MC_Yellow] = pMat;

		pMat = matman.create("Debug/Turquoise", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		pPass = pMat->getTechnique(0) ->getPass(0);
		pPass->setPointSize(5.0f);
		pTexUnit = pPass->createTextureUnitState("color");
		pTexUnit->setColourOperationEx(LBX_SOURCE1, LBS_MANUAL, LBS_CURRENT, ColourValue(0.0f, 1.0f, 1.0f));
		_pMaterials[MC_Turquoise] = pMat;

		pMat = matman.create("Debug/Magenta", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		pPass = pMat->getTechnique(0) ->getPass(0);
		pPass->setPointSize(5.0f);
		pTexUnit = pPass->createTextureUnitState("color");
		pTexUnit->setColourOperationEx(LBX_SOURCE1, LBS_MANUAL, LBS_CURRENT, ColourValue(1.0f, 0.0f, 1.0f));
		_pMaterials[MC_Magenta] = pMat;

		_pScNode = _pSceneManager->getRootSceneNode()->createChildSceneNode("OverhangTerrain::DebugTools");
	}

	void DebugDisplay::drawCube( AxisAlignedBox bbox )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		ManualObject * pManObj = _pSceneManager->createManualObject();

		rollbackTransforms(bbox);

		pManObj->begin(getCurrentMaterial(), RenderOperation::OT_LINE_LIST);

		const Vector3
			m0 = bbox.getMinimum(),
			mN = bbox.getMaximum();

		pManObj->position(m0.x, m0.y, m0.z);
		pManObj->position(mN.x, m0.y, m0.z);

		pManObj->position(m0.x, m0.y, m0.z);
		pManObj->position(m0.x, mN.y, m0.z);

		pManObj->position(m0.x, m0.y, m0.z);
		pManObj->position(m0.x, m0.y, mN.z);

		pManObj->position(mN.x, mN.y, m0.z);
		pManObj->position(mN.x, m0.y, m0.z);

		pManObj->position(mN.x, mN.y, m0.z);
		pManObj->position(m0.x, mN.y, m0.z);

		pManObj->position(mN.x, mN.y, m0.z);
		pManObj->position(mN.x, mN.y, mN.z);

		pManObj->position(m0.x, mN.y, mN.z);
		pManObj->position(m0.x, mN.y, m0.z);

		pManObj->position(m0.x, mN.y, mN.z);
		pManObj->position(m0.x, m0.y, mN.z);

		pManObj->position(m0.x, mN.y, mN.z);
		pManObj->position(mN.x, mN.y, mN.z);

		pManObj->position(mN.x, m0.y, mN.z);
		pManObj->position(m0.x, m0.y, mN.z);

		pManObj->position(mN.x, m0.y, mN.z);
		pManObj->position(mN.x, m0.y, m0.z);

		pManObj->position(mN.x, m0.y, mN.z);
		pManObj->position(mN.x, mN.y, mN.z);

		pManObj->end();

		_pScNode->attachObject(pManObj);
	}

	void DebugDisplay::drawTriangle( Vector3 t0, Vector3 t1, Vector3 t2 )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		ManualObject * pManObj = _pSceneManager->createManualObject();

		rollbackTransforms(t0);
		rollbackTransforms(t1);
		rollbackTransforms(t2);

		pManObj->begin(getCurrentMaterial(), RenderOperation::OT_TRIANGLE_LIST);

		pManObj->position(t0);
		pManObj->position(t1);
		pManObj->position(t2);

		pManObj->end();

		_pScNode->attachObject(pManObj);
	}

	void DebugDisplay::drawPoint( Vector3 p )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		ManualObject * pManObj = _pSceneManager->createManualObject();

		rollbackTransforms(p);

		pManObj->begin(getCurrentMaterial(), RenderOperation::OT_POINT_LIST);
		pManObj->position(p);
		pManObj->end();

		_pScNode->attachObject(pManObj);
	}

	void DebugDisplay::drawSegment( Vector3 a, Vector3 b )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		ManualObject * pManObj = _pSceneManager->createManualObject();

		rollbackTransforms(a);
		rollbackTransforms(b);

		pManObj->begin(getCurrentMaterial(), RenderOperation::OT_LINE_LIST);
		pManObj->position(a);
		pManObj->position(b);
		pManObj->end();

		_pScNode->attachObject(pManObj);
	}

	DebugDisplay::TransformationHandle DebugDisplay::translate( const String & sFile, const size_t nLine, const Vector3 & tran )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		_trans.push_back(new Translation(sFile, nLine, tran));
		return TransformationHandle(_trans);
	}

	DebugDisplay::TransformationHandle DebugDisplay::scale( const String & sFile, const size_t nLine, const Real fScale )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		_trans.push_back(new Scale(sFile, nLine, fScale));
		return TransformationHandle(_trans);
	}

	DebugDisplay::TransformationHandle DebugDisplay::coordinates( const String & sFile, const size_t nLine, const OverhangCoordinateSpace enocsFrom, const OverhangCoordinateSpace enocsTo )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		_trans.push_back(new CoordinateSystem(sFile, nLine, _pSceneManager->getTerrainManager(), enocsFrom, enocsTo));
		return TransformationHandle(_trans);
	}

	const String & DebugDisplay::getCurrentMaterial()
	{
		oht_assert_threadmodel(ThrMdl_Main);

		return _pMaterials[_smcMaterialStack.top()] ->getName();
	}

	void DebugDisplay::applyTransforms( AxisAlignedBox & bbox ) const
	{
		oht_assert_threadmodel(ThrMdl_Main);

		for (TransformStack::const_iterator i = _trans.begin(); i != _trans.end(); ++i)
			(*i) ->apply(bbox);
	}

	void DebugDisplay::applyTransforms( Vector3 & v ) const
	{
		oht_assert_threadmodel(ThrMdl_Main);

		for (TransformStack::const_iterator i = _trans.begin(); i != _trans.end(); ++i)
			(*i) ->apply(v);
	}

	void DebugDisplay::rollbackTransforms( AxisAlignedBox & bbox ) const
	{
		oht_assert_threadmodel(ThrMdl_Main);

		for (TransformStack::const_reverse_iterator i = _trans.rbegin(); i != _trans.rend(); ++i)
			(*i) ->rollback(bbox);
	}

	void DebugDisplay::rollbackTransforms( Vector3 & v ) const
	{
		oht_assert_threadmodel(ThrMdl_Main);

		for (TransformStack::const_reverse_iterator i = _trans.rbegin(); i != _trans.rend(); ++i)
			(*i) ->rollback(v);
	}

	Ogre::Vector3 DebugDisplay::reverse( Vector3 v )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		rollbackTransforms(v);
		return v;
	}

	Vector3 & DebugDisplay::reverse( Vector3 && v )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		rollbackTransforms(v);
		return v;
	}

	Ogre::DebugDisplay::MaterialStackHandle DebugDisplay::color( const MaterialColor mcColor )
	{
		oht_assert_threadmodel(ThrMdl_Main);

		_smcMaterialStack.push(mcColor);
		return MaterialStackHandle(_smcMaterialStack);
	}

	void DebugDisplay::destroy()
	{
		oht_assert_threadmodel(ThrMdl_Main);

		for (unsigned c = 0; c < CountMaterialColors; ++c)
			_pMaterials[c].setNull();
	}

	void DebugDisplay::clearObjects()
	{
		oht_assert_threadmodel(ThrMdl_Main);

		_pScNode->removeAndDestroyAllChildren();
		_pScNode->detachAllObjects();
	}

	DebugDisplay::TransformationBase::TransformationBase( const String & sFile, const size_t nLine )
		: file(sFile), line(nLine)
	{}

	DebugDisplay::Translation::Translation( const String & sFile, const size_t nLine, const Vector3 & v ) 
	: TransformationBase(sFile, nLine), _v(v) {}

	void DebugDisplay::Translation::apply( Vector3 & v ) const
	{
		v += _v;
	}

	void DebugDisplay::Translation::apply( AxisAlignedBox & bbox ) const
	{
		bbox.getMinimum() += _v;
		bbox.getMaximum() += _v;
	}

	void DebugDisplay::Translation::rollback( Vector3 & v ) const
	{
		v -= _v;
	}

	void DebugDisplay::Translation::rollback( AxisAlignedBox & bbox ) const
	{
		bbox.getMinimum() -= _v;
		bbox.getMaximum() -= _v;
	}

	DebugDisplay::Scale::Scale( const String & sFile, const size_t nLine, const Real fScale ) 
	: TransformationBase(sFile, nLine), _f(fScale) {}

	void DebugDisplay::Scale::apply( Vector3 & v ) const
	{
		v *= _f;
	}

	void DebugDisplay::Scale::apply( AxisAlignedBox & bbox ) const
	{
		bbox.scale(Vector3(_f));
	}

	void DebugDisplay::Scale::rollback( Vector3 & v ) const
	{
		v /= _f;
	}

	void DebugDisplay::Scale::rollback( AxisAlignedBox & bbox ) const
	{
		bbox.scale(Vector3(1.0f/_f));
	}


	DebugDisplay::CoordinateSystem::CoordinateSystem( 
		const String & sFile, const size_t nLine,
		const OverhangTerrainManager * pManager, 
		const OverhangCoordinateSpace enocsFrom, 
		const OverhangCoordinateSpace enocsTo 
	) : TransformationBase(sFile, nLine), _pManager(pManager), _from(enocsFrom), _to(enocsTo)
	{
		OgreAssert(pManager != NULL, "Manager is NULL");
	}

	void DebugDisplay::CoordinateSystem::apply( Vector3 & v ) const
	{
		_pManager->transformSpace(_from, _to, v);
	}

	void DebugDisplay::CoordinateSystem::apply( AxisAlignedBox & bbox ) const
	{
		_pManager->transformSpace(_from, _to, bbox);
	}

	void DebugDisplay::CoordinateSystem::rollback( Vector3 & v ) const
	{
		_pManager->transformSpace(_to, _from, v);
	}

	void DebugDisplay::CoordinateSystem::rollback( AxisAlignedBox & bbox ) const
	{
		_pManager->transformSpace(_to, _from, bbox);
	}


	DebugDisplay::TransformationHandle::~TransformationHandle()
	{
		delete _trans.back();
		_trans.pop_back();
	}

	DebugDisplay::TransformationHandle::TransformationHandle( TransformStack & trans ) 
		: _trans(trans) {}

	DebugDisplay::TransformationHandle::TransformationHandle( TransformationHandle && move ) 
		: _trans(move._trans) {}


	DebugDisplay::MaterialStackHandle::MaterialStackHandle( MaterialStackHandle && move ) 
		: _mats(move._mats)
	{}

	DebugDisplay::MaterialStackHandle::MaterialStackHandle( MaterialStack & mats ) 
		: _mats(mats)
	{}

	DebugDisplay::MaterialStackHandle::~MaterialStackHandle()
	{
		_mats.pop();
	}



}
