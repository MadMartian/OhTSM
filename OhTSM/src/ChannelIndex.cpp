#include "pch.h"

#include "ChannelIndex.h"

namespace Ogre
{
	namespace Channel
	{
		StreamSerialiser & operator<<( StreamSerialiser & outs, const Ident channel )
		{
			outs.write(&channel._ordinal);
			return outs;
		}

		StreamSerialiser & operator>>( StreamSerialiser & ins, Ident & channel )
		{
			ins.read(&channel._ordinal);
			return ins;
		}

	}
}