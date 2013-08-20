#ifndef __OVERHANGTERRAINRENDERMANAGER_H__
#define __OVERHANGTERRAINRENDERMANAGER_H__

#include "OverhangTerrainPrerequisites.h"

#include <OgreRenderQueue.h>
#include <OgreSceneManager.h>

namespace Ogre
{
	class RenderManager : private RenderQueueListener, RenderQueue::RenderableListener
	{
	private:
		class DetermineRenderStateVisitor : public QueuedRenderableVisitor
		{
		public:
			bool flag;

			DetermineRenderStateVisitor();

			virtual void visit(RenderablePass* rp) {}
			virtual bool visit(const Pass* p) { return true; }
			virtual void visit(Renderable* r);
		};

		SceneManager * _pScMgr;

		virtual bool renderableQueued(
			Renderable * pRenderable, 
			uint8 nQID, 
			ushort nPriority, 
			Technique ** ppTech, 
			RenderQueue * pQueue
		);

		virtual void renderQueueStarted(
			uint8 nQID, 
			const String & invocation, 
			bool & bSkip
		);

		virtual void renderQueueEnded(
			uint8 nQID, 
			const String& invocation, 
			bool & bRepeat
		);

	public:
		class QueueManager
		{
		private:
			bool _bRenderFlag;

		public:
			QueueManager();

			inline
				bool isCurrentRenderStateAvailable() const { return _bRenderFlag; }

			friend class RenderManager;
		};

		bool checkBuffers(const IndexData * pIdxData, const HardwareVertexBufferSharedPtr pHWVtxB, VertexDeclaration * pVtxDecl, const float fScale);

		inline const QueueManager & operator [] (const size_t index) const { return _queues[index]; }
		inline const QueueManager * queue (const size_t index) const { return &_queues[index]; }

	private:
		QueueManager _queues[RENDER_QUEUE_MAX + 1];

	public:
		RenderManager(SceneManager * pScMgr);
	};
}

#endif