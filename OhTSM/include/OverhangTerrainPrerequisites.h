/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org

Copyright (c) 2000-2006 Torus Knot Software Ltd
Also see acknowledgments in Readme.html

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.

You may alternatively use this source under the terms of a specific version of
the OGRE Unrestricted License provided you have obtained such a license from
Torus Knot Software Ltd.

Modified by Martin Enge (martin.enge@gmail.com) 2007 to fit into the OverhangTerrain Scene Manager
Modified by Jonathan Neufeld (http://www.extollit.com) 2011-2013 to implement Transvoxel
Transvoxel conceived by Eric Lengyel (http://www.terathon.com/voxels/)

-----------------------------------------------------------------------------
*/

#ifndef __OverhangTerrainPrerequisites_H__
#define __OverhangTerrainPrerequisites_H__

//#include "OgrePrerequisites.h"
//-----------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------

namespace Ogre
{
	class OverhangTerrainOptions;
    class OverhangTerrainSceneManager;
	class OverhangTerrainManager;
	class OverhangTerrainPagedWorldSection;
	class OverhangTerrainGroup;

	class IOverhangTerrainPage;
	class ISerializeCustomData;
	class IOverhangTerrainListener;

	class TerrainTile;
    class PageSection;
	class PagePrivateNonthreaded;
	class MetaObjectIterator;
	class MetaFragmentIterator;

	namespace Voxel
	{
		class CubeDataRegionDescriptor;
		class CubeDataRegion;
		class DataAccessor;
		class const_DataAccessor;
		class CompressedDataAccessor;
		class const_CompressedDataAccessor;
		class ColourChannelSet;
		class FieldAccessor;
		class GradientField;

		class MetaVoxelFactory;
	}

	namespace HardwareShadow
	{
		class LOD;
		class HardwareIsoVertexShadow;
	}

	namespace MetaFragment
	{
		namespace Interfaces
		{
			class Basic;
			class const_Basic;
			class Builder;
			class Unique;
			class Shared;
			class Upgraded;
			class Upgradable;
		}

		class Container;
	}

	class DynamicRenderable;
	class IsoSurfaceBuilder;
	class IsoSurfaceRenderable;
	class MetaObject;
	class MetaBall;
	class MetaHeightMap;
	class MetaBaseFactory;

}
//-----------------------------------------------------------------------
// Windows Settings
//-----------------------------------------------------------------------

#if (OGRE_PLATFORM == OGRE_PLATFORM_WIN32 ) && !defined(OGRE_STATIC_LIB)
#   ifdef OGRE_TERRAINPLUGIN_EXPORTS
#       define _OverhangTerrainPluginExport __declspec(dllexport)
#   else
#       if defined( __MINGW32__ )
#           define _OverhangTerrainPluginExport
#       else
#    		define _OverhangTerrainPluginExport
#       endif
#   endif
#elif defined ( OGRE_GCC_VISIBILITY )
#    define _OverhangTerrainPluginExport  __attribute__ ((visibility("default")))
#else
#   define _OverhangTerrainPluginExport
#endif

#endif

