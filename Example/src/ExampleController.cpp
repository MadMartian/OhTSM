#include "ExampleController.h"

#include <OverhangTerrainSceneManager.h>
#include <stdlib.h>

ExampleController::ExampleController( 
	RenderWindow * pRendWind, 
	Camera * pCam, 
	OverhangTerrainSceneManager * pScMgr, 
	bool bBufferedKeys /*= false*/, 
	bool bBufferedMouse /*= false*/, 
	bool bBufferedJoy /*= false */ 
) : ExampleFrameListener(pRendWind, pCam, bBufferedKeys, bBufferedMouse, bBufferedJoy), 
	_digger(pScMgr, true), _builder(pScMgr, false), _nTimeTracker(0), _pScMgr(pScMgr)
{
	showDebugOverlay(false);
}

bool ExampleController::processUnbufferedMouseInput( const FrameEvent& evt )
{
	if (mMouse->getMouseState().buttonDown(OIS::MB_Left))
		shootMetaball(Ray(mCamera->getPosition(), mCamera->getDirection()), true);
	else if (mMouse->getMouseState().buttonDown(OIS::MB_Right))
		shootMetaball(Ray(mCamera->getPosition(), mCamera->getDirection()), false);

	return ExampleFrameListener::processUnbufferedMouseInput(evt);
}

bool ExampleController::processUnbufferedKeyInput( const FrameEvent& evt )
{
	_nTimeTracker += evt.timeSinceLastEvent;

	if (mKeyboard->isKeyDown(OIS::KC_ADD) && _nTimeTracker > 0.25f)
	{
		shootMetaball(Ray(mCamera->getPosition(), mCamera->getRealDirection()));
		_nTimeTracker = 0.0f;
	}
	if (mKeyboard->isKeyDown(OIS::KC_MINUS) && _nTimeTracker > 0.25f)
	{
		placeMetaball();
		_nTimeTracker = 0.0f;
	}

	if (mKeyboard->isKeyDown(OIS::KC_F3) && _nTimeTracker > 0.25f)
	{
		std::strstream ss;

		ss << "Camera position is " << mCamera->getPosition() << ", orientation " << mCamera->getDirection() << std::endl << std::ends;
		OutputDebugStringA(ss.str());

		_nTimeTracker = 0.0f;
	}

	return ExampleFrameListener::processUnbufferedKeyInput(evt);
}

void ExampleController::shootMetaball( const Ray & ray, const bool bExcavating /*= true*/ )
{
	// zAxis because by default the camera's direction is 
	RaySceneQuery * rsq = _pScMgr->createRayQuery(ray);

	rsq->execute(bExcavating ? &_digger : &_builder);

	delete rsq;
}

void ExampleController::placeMetaball( const bool bExcavating /*= true*/ )
{
	_pScMgr->addMetaBall(mCamera->getPosition(), Real(250), bExcavating);
}

bool ExampleController::DiggerRSQL::queryResult( MovableObject* obj, Real distance )
{
	return true;
}

bool ExampleController::DiggerRSQL::queryResult( SceneQuery::WorldFragment * fragment, Real distance )
{
	_pScMgr->addMetaBall(fragment->singleIntersection, Real(rand() % 100 + 90), _bExcavating);
	return true;
}

ExampleController::DiggerRSQL::DiggerRSQL( OverhangTerrainSceneManager * pScMgr, const bool bExcavating /*= true*/ )
	: _pScMgr(pScMgr), _bExcavating(bExcavating) {}

