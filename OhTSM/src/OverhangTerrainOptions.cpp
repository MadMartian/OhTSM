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
		stream.read(&cellScale);
		stream.read(&heightScale);
		stream.read(&materialPerTile);
		stream.read(&autoSave);

		uint16 nChannelCount;

		stream.read(&nChannelCount);
		channels = Channel::Index< ChannelOptions > (channels.descriptor, [&stream] (const Channel::Ident channel) -> ChannelOptions *
		{
			unsigned char cEnumTemp;
			ChannelOptions * pOpts = new ChannelOptions;
			stream.read(&pOpts->flipNormals);
			String sMatName, sMatGroup;
			stream.read(&sMatGroup);
			stream.read(&sMatName);
			pOpts->material = MaterialManager::getSingleton().load(sMatName, sMatGroup);
			stream.read(&cEnumTemp);
			pOpts->normals = static_cast< NormalsType > (cEnumTemp);
			stream.read(&pOpts->transitionCellWidthRatio);
			stream.read(&cEnumTemp);
			pOpts->voxelRegionFlags = static_cast< OverhangTerrainVoxelRegionFlags > (cEnumTemp);
			stream.read(&pOpts->maxGeoMipMapLevel);
			stream.read(&pOpts->maxPixelError);

			return pOpts;
		});

		return stream;
	}

	StreamSerialiser & OverhangTerrainOptions::operator >> ( StreamSerialiser & stream ) const
	{
		stream.writeChunkBegin(CHUNK_ID, CHUNK_VERSION);
		stream.write(&alignment);
		stream.write(&pageSize);
		stream.write(&tileSize);
		stream.write(&cellScale);
		stream.write(&heightScale);
		stream.write(&materialPerTile);
		stream.write(&autoSave);
		stream.writeChunkEnd(CHUNK_ID);

		stream.write(&channels.descriptor.count);
		for (Channel::Descriptor::iterator i = channels.descriptor.begin(); i != channels.descriptor.end(); ++i)
		{
			unsigned char cEnumTemp;
			const ChannelOptions & opts = channels[*i];
			stream.write(&opts.flipNormals);
			const String
				sMatName = opts.material->getName(),
				sMatGroup = opts.material->getGroup();
			stream.write(&sMatGroup);
			stream.write(&sMatName);
			cEnumTemp = opts.normals;
			stream.write(&cEnumTemp);
			stream.write(&opts.transitionCellWidthRatio);
			cEnumTemp = opts.voxelRegionFlags;
			stream.write(&cEnumTemp);
			stream.write(&opts.maxGeoMipMapLevel);
			stream.write(&opts.maxPixelError);
		}

		return stream;
	}

	OverhangTerrainOptions::OverhangTerrainOptions() 
	:	pageSize(0),
		tileSize(0),
		alignment(ALIGN_X_Z),
		cellScale(1.0f),
		heightScale(1.0f),
		primaryCamera(NULL),
		autoSave(true),
		materialPerTile(true),
		channels(Channel::Descriptor(1))
	{
	}


	OverhangTerrainOptions::ChannelOptions::ChannelOptions()
	:	materialPerTile(false),
		normals(NT_Gradient),
		flipNormals(false),
		transitionCellWidthRatio(0.5f)
	{
	}

}
