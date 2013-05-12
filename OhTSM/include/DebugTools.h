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

#ifndef __OVERHANGDEBUGTOOLS_H__
#define __OVERHANGDEBUGTOOLS_H__

#include <boost/thread.hpp>
#include <OgreLogManager.h>

#include <map>
#include <ostream>
#include <list>
#include <stack>

#include <OgreMaterial.h>

#include "OverhangTerrainPrerequisites.h"
#include "Util.h"

namespace Ogre
{
	enum ThreadingModel
	{
		ThrMdl_Main,
		ThrMdl_Background,
		ThrMdl_Single,
		ThrMdl_Any
	};

	extern class ThreadingModelManager
	{
	public:
		boost::mutex mutex;

		class AssertEx : public std::exception 
		{
		public:
			AssertEx(const char * szMsg);
		};

		ThreadingModelManager();

		void registerMainThread ();
		size_t registerThread (const char * szFuncName, const void * pThis);
		size_t deregisterThread (const char * szFuncName, const void * pThis);
		bool checkMainThread (const char * szFuncName);

	private:
		typedef std::set< boost::thread::id > ThreadIDSet;
		typedef std::map< std::string, ThreadIDSet > ThreadConcurrencyMap;
		typedef std::map< std::pair< std::string, const void * >, size_t > ThreadReferenceCountMap;

		ThreadConcurrencyMap mThreadTracker;
		ThreadReferenceCountMap mRefCounter;
		boost::thread::id mMainThread;
		bool mMainThreadSet;
	} gtmm_ThreadingModelManager;

	class ThreadingModelMonitor
	{
	public:
		ThreadingModelMonitor (const char * szFuncName, const char * szFuncFile, const size_t nFuncFileLine, const void * oInst, const ThreadingModel entmThreadingModel);
		~ThreadingModelMonitor();

	private:
		ThreadingModel mThreadingModel;
		boost::thread::id mThreadID;
		std::string mFuncID;
		const void * mInst;

		static std::string buildFuncID (const char * szFuncName, const char * szFuncFile, const size_t nFuncFileLine);
	};

	template< typename T >
	class ObjectCounter
	{
	public:
		ObjectCounter & operator << (const T & obj)
		{
			CounterMap::iterator i = _map.find(obj);

			if (i == _map.end())
				_map[obj] = 1;
			else
				++i->second;

			return *this;
		}
		size_t operator () (const T & obj) const
		{
			CounterMap::iterator i = _map.find(obj);

			if (i == _map.end())
				return 0;
			else
				return i->second;
		}

		void dump (std::ostream & outs) const
		{
			outs << '(';
			for (ObjectCounter< T >::CounterMap::const_iterator i = _map.begin(); i != _map.end(); ++i)
			{
				if (i != _map.begin())
					outs << ", ";
				outs << '[' << i->first << ", " << i->second << ']';
			}
			outs << ')';
		}

	private:
		typedef std::map< T, size_t > CounterMap;
		CounterMap _map;
	};

	extern class DebugDisplay
	{
	public:
		enum MaterialColor
		{
			MC_Red = 0,
			MC_Blue = 1,
			MC_Green = 2,
			MC_Yellow = 3,
			MC_Turquoise = 4,
			MC_Magenta = 5,

			CountMaterialColors = 6
		};

	private:
		class TransformationBase
		{
		public:
			const String file;
			const size_t line;

			TransformationBase(const String & sFile, const size_t nLine);

			virtual void apply(Vector3 & v) const = 0;
			virtual void apply(AxisAlignedBox & bbox) const = 0;
			virtual void rollback(Vector3 & v) const = 0;
			virtual void rollback(AxisAlignedBox & bbox) const = 0;
		};

		typedef std::list< TransformationBase * > TransformStack;
		typedef std::stack< MaterialColor > MaterialStack;

	public:
		class TransformationHandle
		{
		private:
			TransformStack & _trans;

			TransformationHandle(TransformStack & trans);

		public:
			TransformationHandle(TransformationHandle && move);
			~TransformationHandle();

			friend class DebugDisplay;
		};

		class MaterialStackHandle
		{
		private:
			MaterialStack & _mats;

			MaterialStackHandle(MaterialStack & mats);

		public:
			MaterialStackHandle(MaterialStackHandle && move);
			~MaterialStackHandle();

			friend class DebugDisplay;
		};

		DebugDisplay();
		~DebugDisplay();

		void init(OverhangTerrainSceneManager * pSceneManager);
		void destroy();

		void drawCube (AxisAlignedBox bbox);
		void drawTriangle (Vector3 t0, Vector3 t1, Vector3 t2);
		void drawPoint (Vector3 p);
		void drawSegment (Vector3 a, Vector3 b);
		MaterialStackHandle color (const MaterialColor mcColor);

		void clearObjects();
		
		Vector3 reverse(Vector3 v);
		Vector3 & reverse(Vector3 && v);

		TransformationHandle translate(const String & sFile, const size_t nLine, const Vector3 & tran);
		TransformationHandle scale(const String & sFile, const size_t nLine, const Real fScale);
		TransformationHandle coordinates(const String & sFile, const size_t nLine, const OverhangCoordinateSpace enocsFrom, const OverhangCoordinateSpace enocsTo);

	private:
		OverhangTerrainSceneManager * _pSceneManager;
		SceneNode * _pScNode;
		MaterialPtr _pMaterials[CountMaterialColors];
		MaterialStack _smcMaterialStack;
		TransformStack _trans;

		class Translation : public TransformationBase
		{
		private:
			const Vector3 _v;

		public:
			Translation(const String & sFile, const size_t nLine, const Vector3 & v);
			virtual void apply(Vector3 & v) const;
			virtual void apply(AxisAlignedBox & bbox) const;
			virtual void rollback(Vector3 & v) const;
			virtual void rollback(AxisAlignedBox & bbox) const;
		};

		class Scale : public TransformationBase
		{
		private:
			const Real _f;

		public:
			Scale(const String & sFile, const size_t nLine, const Real fScale);
			virtual void apply(Vector3 & v) const;
			virtual void apply(AxisAlignedBox & bbox) const;
			virtual void rollback(Vector3 & v) const;
			virtual void rollback(AxisAlignedBox & bbox) const;
		};

		class CoordinateSystem : public TransformationBase
		{
		private:
			const OverhangCoordinateSpace
				_from, _to;

			const OverhangTerrainManager * const _pManager;

		public:
			CoordinateSystem(const String & sFile, const size_t nLine, const OverhangTerrainManager * pManager, const OverhangCoordinateSpace enocsFrom, const OverhangCoordinateSpace enocsTo);

			virtual void apply(Vector3 & v) const;
			virtual void apply(AxisAlignedBox & bbox) const;
			virtual void rollback(Vector3 & v) const;
			virtual void rollback(AxisAlignedBox & bbox) const;
		};

		const String & getCurrentMaterial();

		void applyTransforms(AxisAlignedBox & bbox) const;
		void applyTransforms(Vector3 & v) const;
		void rollbackTransforms(AxisAlignedBox & bbox) const;
		void rollbackTransforms(Vector3 & v) const;		
	} gdd_DebugDisplay;
}

#if defined(_THRDBG)
	#define oht_assert_threadmodel(entmThreadingModel) Ogre::ThreadingModelMonitor tmm(__FUNCTION__, __FILE__, __LINE__, this, entmThreadingModel)
	#define oht_register_mainthread() gtmm_ThreadingModelManager.registerMainThread()
#else
	#define oht_assert_threadmodel(x)
	#define oht_register_mainthread()
#endif

#if defined(_DISPDBG)
	#define OHTDD_Init(x) gdd_DebugDisplay.init(x)
	#define OHTDD_Destroy() gdd_DebugDisplay.destroy()
	#define OHTDD_Reverse(x) gdd_DebugDisplay.reverse(x)
	#define OHTDD_Scale(x) volatile auto UNIQUEVAR(TranHandScale) (gdd_DebugDisplay.scale(__FILE__, __LINE__, x))
	#define OHTDD_Translate(x) volatile auto UNIQUEVAR(TranHandTranslate) (gdd_DebugDisplay.translate(__FILE__, __LINE__, x))
	#define OHTDD_Coords(a,b) volatile auto UNIQUEVAR(TranHandCoords) (gdd_DebugDisplay.coordinates(__FILE__, __LINE__, a, b))
	#define OHTDD_Color(c) volatile auto UNIQUEVAR(MatHandColor) (gdd_DebugDisplay.color(c))
	#define OHTDD_Cube(bbox) gdd_DebugDisplay.drawCube(bbox)
	#define OHTDD_Line(a,b) gdd_DebugDisplay.drawSegment(a,b)
	#define OHTDD_Tri(t0,t1,t2) gdd_DebugDisplay.drawTriangle(t0,t1,t2)
	#define OHTDD_Point(p) gdd_DebugDisplay.drawPoint(p)
	#define OHTDD_Clear() gdd_DebugDisplay.clearObjects()
#else
	#define OHTDD_Init(x)
	#define OHTDD_Destroy()
	#define OHTDD_Reverse(x)
	#define OHTDD_Scale(x)
	#define OHTDD_Translate(x)
	#define OHTDD_Coords(a,b)
	#define OHTDD_Color(c)
	#define OHTDD_Cube(bbox)
	#define OHTDD_Line(a,b)
	#define OHTDD_Tri(t0,t1,t2)
	#define OHTDD_Point(p)
	#define OHTDD_Clear()
#endif

#define OHT_LOG_FATAL(x) Ogre::LogManager::getSingleton().stream(Ogre::LML_CRITICAL) << __FILE__ << ":" << __LINE__ << " - (thread:" << boost::this_thread::get_id() << ") " << __FUNCTION__ << " [CLK:" << clock() << "]\n\t" << x

#if defined(_OHT_LOG_TRACE)
	#define OHT_DBGTRACE(x) Ogre::LogManager::getSingleton().stream() << __FILE__ << ":" << __LINE__ << " - (thread:" << boost::this_thread::get_id() << ") " << __FUNCTION__ << " [CLK:" << clock() << "]\n\t" << x
#else
	#define OHT_DBGTRACE(x)
#endif

#endif