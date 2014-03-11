#include "pch.h"

#include "OverhangTerrainSlot.h"

#include "PageSection.h"

namespace Ogre
{
	OverhangTerrainSlot::OverhangTerrainSlot( OverhangTerrainGroup * pGrp, const int16 x, const int16 y ) 
		: group(pGrp), x(x), y(y), instance (NULL), data(NULL), _enState0(TSS_Empty), _enState(TSS_Empty), queryNeighbors(0), queryCount(0)
	{

	}

	void OverhangTerrainSlot::freeLoadData()
	{
		delete data;
		data = NULL;
	}

	OverhangTerrainSlot::~OverhangTerrainSlot()
	{
		delete data; 
		delete instance;
	}

	void OverhangTerrainSlot::setNeighborQuery( const VonNeumannNeighbor evnNeighbor )
	{
		if (_enState != TSS_Neutral && _enState != TSS_Empty && (_enState != TSS_NeighborQuery || (queryNeighbors & Neighborhood::flag(evnNeighbor))))
			throw StateEx("Cannot set query neighbor state, slot is busy");

		if (!queryNeighbors)
			_enState0 = _enState;
		_enState = TSS_NeighborQuery;
		queryNeighbors |= Neighborhood::flag(evnNeighbor);
	}

	void OverhangTerrainSlot::clearNeighborQuery(const VonNeumannNeighbor evnNeighbor)
	{
		if (_enState != TSS_NeighborQuery || !(queryNeighbors & Neighborhood::flag(evnNeighbor)))
			throw StateEx("Cannot clear neighbor query state, wasn't previously set");

		queryNeighbors ^= Neighborhood::flag(evnNeighbor);

		if (!queryNeighbors)
		{
			_enState = _enState0;
			if (_enState == TSS_Neutral || _enState == TSS_Empty)
				processPendingTasks();
		}
	}

	bool OverhangTerrainSlot::canNeighborQuery( const VonNeumannNeighbor evnNeighbor ) const
	{
		switch (_enState)
		{
		case TSS_Neutral:
		case TSS_Empty:
			return true;
		case TSS_NeighborQuery:
			return !(queryNeighbors & Neighborhood::flag(evnNeighbor));
		default:
			return false;
		}
	}

	bool OverhangTerrainSlot::isNeighborQueried( const VonNeumannNeighbor evnNeighbor ) const
	{
		return _enState == TSS_NeighborQuery && (queryNeighbors & Neighborhood::flag(evnNeighbor));
	}


	void OverhangTerrainSlot::saving()
	{
		if (_enState != TSS_Neutral)
			throw StateEx("State was not neutral or save-unload");

		_enState0 = _enState;
		_enState = TSS_Saving;
	}

	void OverhangTerrainSlot::doneSaving()
	{
		if (_enState != TSS_Saving)
			throw StateEx("State was not saving");

		_enState = _enState0;
		if (_enState == TSS_Neutral || _enState == TSS_Empty)
			processPendingTasks();
	}

	void OverhangTerrainSlot::saveUnload()
	{
		if (_enState != TSS_Neutral)
			throw StateEx("State was not neutral");

		_enState0 = TSS_Unloading;
		_enState = TSS_Saving;
	}

	void OverhangTerrainSlot::mutating()
	{
		if (_enState != TSS_Neutral)
			throw StateEx("State was not neutral");

		_enState = TSS_Mutate;
	}

	void OverhangTerrainSlot::doneMutating()
	{
		if (_enState != TSS_Mutate)
			throw StateEx("State was not mutating");

		_enState = TSS_Neutral;
		processPendingTasks();
	}

	void OverhangTerrainSlot::query()
	{
		if (_enState != TSS_Neutral && _enState != TSS_Query)
			throw StateEx("State was not neutral or query");

		queryCount++;
		_enState = TSS_Query;
	}

	void OverhangTerrainSlot::doneQuery()
	{
		if (_enState != TSS_Query)
			throw StateEx("State was not query");

		if (--queryCount == 0)
			_enState = TSS_Neutral;

		processPendingTasks();
	}

	void OverhangTerrainSlot::loading()
	{
		if (_enState != TSS_Empty)
			throw StateEx("State was not empty");

		_enState = TSS_Loading;
	}

	void OverhangTerrainSlot::doneLoading()
	{
		if (_enState != TSS_Loading)
			throw StateEx("State was not loading");

		_enState = TSS_Neutral;
		processPendingTasks();
	}

	void OverhangTerrainSlot::unloading()
	{
		if (_enState != TSS_Neutral)
			throw StateEx("State was not neutral");

		_enState = TSS_Unloading;
	}

	void OverhangTerrainSlot::doneUnloading()
	{
		if (_enState != TSS_Unloading)
			throw StateEx("State was not unloading");

		_enState = TSS_Empty;
		processPendingTasks();
	}

	void OverhangTerrainSlot::destroy()
	{
		if (_enState != TSS_Neutral)
			throw StateEx("State was not neutral");

		_enState = TSS_Destroy;
	}

	void OverhangTerrainSlot::processPendingTasks()
	{
		while(!_qJoinTasks.empty())
		{
			JoinTask task = _qJoinTasks.front();
			_qJoinTasks.pop();
			switch (task.type)
			{
			case JoinTask::JTT_Destroy: 
				delete instance;
				instance = NULL;
				destroy();
				break;
			case JoinTask::JTT_SetMaterial:
				instance->setMaterial(task.channel, task.material);
				break;
			case JoinTask::JTT_SetQID:
				instance->setRenderQueue(task.channel, task.qid);
				break;
			}
		}
	}

	void OverhangTerrainSlot::setMaterial( const Channel::Ident channel, MaterialPtr pMaterial )
	{
		if (_enState == TSS_Neutral)
			instance->setMaterial(channel, pMaterial);
		else
		{
			JoinTask task;
			task.type = JoinTask::JTT_SetMaterial;
			task.material = pMaterial;
			task.channel = channel;
			_qJoinTasks.push(task);
		}
	}

	void OverhangTerrainSlot::setRenderQueueGroup( const Channel::Ident channel, const int nQID )
	{
		if (_enState == TSS_Neutral)
			instance->setRenderQueue(channel, nQID);
		else
		{
			JoinTask task;
			task.type = JoinTask::JTT_SetQID;
			task.qid = nQID;
			task.channel = channel;
			_qJoinTasks.push(task);
		}
	}

	void OverhangTerrainSlot::destroySlot()
	{
		if (_enState == TSS_Neutral || _enState == TSS_Empty)
		{
			delete instance;
			instance = NULL;
			destroy();
		}
		else
		{
			JoinTask task;
			task.type = JoinTask::JTT_Destroy;
			_qJoinTasks.push(task);
		}
	}

	OverhangTerrainSlot::StateEx::StateEx( const char * szMsg ) 
		: std::exception(szMsg) 
	{
		LogManager::getSingleton().stream(LML_CRITICAL) << szMsg;
	}

}
