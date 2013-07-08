#include "ExamplePageProvider.h"

#include <OverhangTerrainPage.h>
#include <OgreLogManager.h>

bool ExamplePageProvider::loadPage( const int16 x, const int16 y, PageInitParams * pInitParams, IOverhangTerrainPage * pPage )
{
	const static size_t HALF = 1 << ((sizeof(size_t) << 3) - 1);
	const Real r = Real(3.1415926535897932 / 180.0);
	const size_t 
		nTverts = pInitParams->countVerticesPerPageSide,
		x0 = size_t(int16(nTverts - 1) * x + HALF),
		y0 = size_t(int16(nTverts - 1) * y + HALF);

	for (size_t j = y0; j < nTverts + y0; ++j)
		for (size_t i = x0; i < nTverts + x0; ++i)
			pInitParams->heightmap[((nTverts - 1) - (j - y0)) * nTverts + (i - x0)] = Real(cos(double(i)*r*1.5)*100 + sin(double(j)*r*1.5)*100);

	*pPage << *pInitParams;

	try
	{
		StreamSerialiser ins = StreamSerialiser(Root::getSingleton().openFileStream(createFilePath(x, y), _sResourceGroupName));

		Ogre::LogManager::getSingletonPtr() ->stream(LML_NORMAL) << "Reading from terrain page (" << x << "x" << y << ")";
		*pPage << ins;
	} catch (Exception & e)
	{
		Ogre::LogManager::getSingletonPtr() ->stream(LML_CRITICAL) << "Failure opening terrain page (" << x << "x" << y << "): " << e.getFullDescription();
	}

	pPage->conjoin();

	return true;
}

bool ExamplePageProvider::savePage( const IOverhangTerrainPage * pPage, const int16 x, const int16 y, const size_t nPageAxis, const unsigned long nTotalPageSize )
{
	try
	{
		StreamSerialiser outs = StreamSerialiser(Root::getSingleton().createFileStream(createFilePath(x, y), _sResourceGroupName, true));

		*pPage >> outs;
	} catch (Exception & e)
	{
		Ogre::LogManager::getSingletonPtr() ->stream(LML_CRITICAL) << "Failure writing terrain page (" << x << "x" << y << "): " << e.getFullDescription();
	}
	return true;	// Height maps don't need to be saved, meta-balls embody terrain deformation state
}

void ExamplePageProvider::unloadPage( const int16 x, const int16 y )
{

}

void ExamplePageProvider::preparePage( const int16 x, const int16 y, IOverhangTerrainPage * pPage )
{

}

void ExamplePageProvider::detachPage( const int16 x, const int16 y, IOverhangTerrainPage * pPage )
{

}

String ExamplePageProvider::createFilePath( const int16 x, const int16 y ) const
{
	StringStream ssName;

	ssName << "ohtst-" << std::setw(8) << std::setfill('0') << std::hex << _pPgWSect->calculatePageID(x, y) << ".dat";

	return ssName.str();
}

ExamplePageProvider::ExamplePageProvider( const OverhangTerrainPagedWorldSection * pPgWSect, const String & sResourceGroupName )
	: _pPgWSect(pPgWSect), _sResourceGroupName(sResourceGroupName)
{

}
