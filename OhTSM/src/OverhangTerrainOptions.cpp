#include "pch.h"

#include "OverhangTerrainOptions.h"

#include <OgreException.h>

namespace Ogre
{
	const uint32 OverhangTerrainOptions::CHUNK_ID = StreamSerialiser::makeIdentifier("OHTO");
	const uint16 OverhangTerrainOptions::CHUNK_VERSION = 1;

	StreamSerialiser & OverhangTerrainOptions::operator << ( StreamSerialiser & stream )
	{
		if (!stream.readChunkBegin(CHUNK_ID, CHUNK_VERSION))
			OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND, "Stream does not contain OverhangTerrainOptions data", "operator <<");

		stream.read(&alignment);
		stream.read(&pageSize);
		stream.read(&tileSize);
		stream.read(&maxGeoMipMapLevel);
		stream.read(&cellScale);
		stream.read(&heightScale);
		stream.read(&maxPixelError);
		stream.read(&coloured);
		stream.read(&terrainMaterial);
		stream.read(&materialPerTile);
		stream.read(&autoSave);

		return stream;
	}

	StreamSerialiser & OverhangTerrainOptions::operator >> ( StreamSerialiser & stream ) const
	{
		stream.writeChunkBegin(CHUNK_ID, CHUNK_VERSION);
		stream.write(&alignment);
		stream.write(&pageSize);
		stream.write(&tileSize);
		stream.write(&maxGeoMipMapLevel);
		stream.write(&cellScale);
		stream.write(&heightScale);
		stream.write(&maxPixelError);
		stream.write(&coloured);
		stream.write(&terrainMaterial);
		stream.write(&materialPerTile);
		stream.write(&autoSave);
		stream.writeChunkEnd(CHUNK_ID);

		return stream;
	}

	OverhangTerrainOptions::OverhangTerrainOptions() 
	:	pageSize(0),
		tileSize(0),
		maxGeoMipMapLevel(3),
		alignment(ALIGN_X_Z),
		cellScale(1.0f),
		heightScale(1.0f),
		maxPixelError(8.0f),
		coloured(true),
		primaryCamera(NULL),
		autoSave(true),
		materialPerTile(true)
	{
		terrainMaterial.setNull();
	}

}
