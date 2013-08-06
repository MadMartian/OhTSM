#ifndef __EXAMPLECONTROLLER_H__
#define __EXAMPLECONTROLLER_H__

#include <ExampleFrameListener.h>

#include <OverhangTerrainPrerequisites.h>

using namespace Ogre;

class ExampleController : public ExampleFrameListener
{
public:
	ExampleController(
		RenderWindow * pRendWind, 
		Camera * pCam, 
		OverhangTerrainSceneManager * pScMgr, 
		bool bBufferedKeys = false, 
		bool bBufferedMouse = false, 
		bool bBufferedJoy = false 
	);

	bool processUnbufferedMouseInput(const FrameEvent& evt);
	bool processUnbufferedKeyInput(const FrameEvent& evt);

protected:
	virtual void updateStats(void) {}

private:
	Real _nTimeTracker;
	OverhangTerrainSceneManager * _pScMgr;

	void shootMetaball(const bool bExcavating = true);
	void placeMetaball(const bool bExcavating = true);

	class DiggerRSQL : public RaySceneQueryListener
	{
	private:
		OverhangTerrainSceneManager * _pScMgr;
		bool _bExcavating;

	public:
		DiggerRSQL (OverhangTerrainSceneManager * pScMgr, const bool bExcavating = true);
		bool queryResult(MovableObject* obj, Real distance);
		bool queryResult(SceneQuery::WorldFragment* fragment, Real distance);
	} _digger, _builder;
};

#endif