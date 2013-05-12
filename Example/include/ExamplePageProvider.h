#ifndef __PAGEPROVIDER_H__
#define __PAGEPROVIDER_H__

#include <OverhangTerrainPagedWorldSection.h>
#include <OverhangTerrainPageProvider.h>
#include <OgreString.h>

using namespace Ogre;

class ExamplePageProvider : public IOverhangTerrainPageProvider
{
public:
	ExamplePageProvider(const OverhangTerrainPagedWorldSection * pPgWSect, const String & sResourceGroupName);

	// Called in background worker thread when a request has been made to load a page
	virtual bool loadPage (const int16 x, const int16 y, PageInitParams * pInitParams, IOverhangTerrainPage * pPage);

	// Called in background worker thread when a request has been made to flush a page to disk, usually just before it is to be unloaded
	virtual bool savePage (const Real * pfHM, const IOverhangTerrainPage * pPage, const int16 x, const int16 y, const size_t nPageAxis, const unsigned long nTotalPageSize);

	// Called in main thread when a page is about to be unloaded
	virtual void unloadPage (const int16 x, const int16 y);

	// Called last of all in main thread after page has been fully initialised
	virtual void preparePage (const int16 x, const int16 y, IOverhangTerrainPage * pPage);

	// Called initially in main thread to detach and prepare the page for deletion
	virtual void detachPage (const int16 x, const int16 y, IOverhangTerrainPage * pPage);

private:
	const String _sResourceGroupName;
	const OverhangTerrainPagedWorldSection * _pPgWSect;

	String createFilePath( const int16 x, const int16 y ) const;

};

#endif