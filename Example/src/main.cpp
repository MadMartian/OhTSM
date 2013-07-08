#include <windows.h>
#include <Ogre.h>
#include <Paging/OgrePaging.h>
#include <ExampleFrameListener.h>

#include <OverhangTerrainPrerequisites.h>
#include <OverhangTerrainSceneManager.h>
#include <OverhangTerrainGroup.h>
#include <OverhangTerrainPage.h>
#include <OverhangTerrainPagedWorldSection.h>
#include <OverhangTerrainPaging.h>

#include "ExampleController.h"
#include "ExamplePageProvider.h"

using namespace Ogre;

class DummyPageProvider : public PageProvider
{
public:
	bool prepareProceduralPage(Page* page, PagedWorldSection* section) { return true; }
	bool loadProceduralPage(Page* page, PagedWorldSection* section) { return true; }
	bool unloadProceduralPage(Page* page, PagedWorldSection* section) { return true; }
	bool unprepareProceduralPage(Page* page, PagedWorldSection* section) { return true; }
};

#ifdef _DEBUG
#define PLUGIN(x) x "_d"
#else
#define PLUGIN(x) x
#endif

int WINAPI WinMain ( HINSTANCE hInst, HINSTANCE, LPSTR szCmdLine, int)
{
	Root * pRoot = new Root(StringUtil::BLANK);

	pRoot->loadPlugin(PLUGIN("RenderSystem_GL"));
	pRoot->loadPlugin(PLUGIN("RenderSystem_Direct3D9"));
	pRoot->addResourceLocation("paging", "FileSystem", "Paging");

	if (pRoot->showConfigDialog())
	{
		OverhangTerrainSceneManager * pScMgr = OGRE_NEW OverhangTerrainSceneManager("Default");
		PageManager * pPgMan = OGRE_NEW PageManager();
		OverhangTerrainPaging * pOhPging = new OverhangTerrainPaging(pPgMan);

		RenderWindow * pRendWindow = pRoot->initialise(true);
		ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

		Light * pSun = pScMgr->createLight("Sun");
		pSun->setPosition(0,5000, 0);
		pSun->setType(Light::LT_POINT);

		MaterialPtr pMat = MaterialManager::getSingleton().create("BaseMaterial", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		Pass * pPass = pMat->getTechnique(0) ->getPass(0);
		pPass->setLightingEnabled(true);
		pPass->setDiffuse(0.1f, 0.5f, 1.0f, 1.0f);
		pPass->setAmbient(0.05f, 0.1f, 0.2f);
		pPass->setSpecular(1.0f, 1.0f, 1.0f, 1.0f);
		pPass->setShininess(80);

		Camera * pCam = pScMgr->createCamera("Photographer");
		pCam->setNearClipDistance(0.1f);
		pCam->setFarClipDistance(7000);
		pCam->setPosition(1, 500, 1);
		pCam->lookAt(Vector3::ZERO);
		Viewport * vp = pRendWindow->addViewport(pCam);
		pCam->setAspectRatio(Real(vp->getActualWidth()) / Real(vp->getActualHeight()));

		SceneNode 
			* pnCam = pScMgr->getRootSceneNode()->createChildSceneNode("Helicopter"),
			* pnLight = pScMgr->getRootSceneNode() ->createChildSceneNode("Aura");

		pnCam->attachObject(pCam);
		pnLight->attachObject(pSun);

		OverhangTerrainOptions options;

		options.primaryCamera = pCam;
		options.pageSize = 129;
		options.tileSize = 33;
		options.cellScale = 50.0;
		options.heightScale = 8.0;
		options.channels[TERRAIN_ENTITY_CHANNEL].material = pMat;
		options.channels[TERRAIN_ENTITY_CHANNEL].maxGeoMipMapLevel = 6;
		options.channels[TERRAIN_ENTITY_CHANNEL].maxPixelError = 10;

		pScMgr->setOptions(options);

		OverhangTerrainGroup * pGrp = OGRE_NEW OverhangTerrainGroup(pScMgr, NULL, "Paging");

		pPgMan->addCamera(pCam);
		DummyPageProvider dpp;
		pPgMan->setPageProvider(&dpp);
		PagedWorld * pWorld = pPgMan->createWorld("OhTSM");
		OverhangTerrainPagedWorldSection * pOhPgSect = pOhPging->createWorldSection(pWorld, pGrp);
		pScMgr->initialise();

		ExamplePageProvider pp(pOhPgSect, pGrp->getResourceGroupName());
		pGrp->setPageProvider(&pp);
		ExampleController * pController = new ExampleController (pRendWindow, pCam, pScMgr);
		pRoot->addFrameListener(pController);
	
		pRoot->startRendering();

		delete pController;

		OGRE_DELETE pOhPgSect;
		OGRE_DELETE pOhPging;
		OGRE_DELETE pPgMan;
		OGRE_DELETE pScMgr;
	}

	delete pRoot;

	return 0;
}