/*
-----------------------------------------------------------------------------
This source file is part of the OverhangTerrainSceneManager
Plugin for OGRE
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2007 Martin Enge
martin.enge@gmail.com

Modified (2013) by Jonathan Neufeld (http://www.extollit.com) to implement Transvoxel
Transvoxel conceived by Eric Lengyel (http://www.terathon.com/voxels/)

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

-----------------------------------------------------------------------------
*/
#include "pch.h"

#include "OverhangTerrainManager.h"
#include "OverhangTerrainSceneManager.h"
#include "MetaWorldFragment.h"
#include "DebugTools.h"

namespace Ogre
{
	AlgorithmIndex OverhangTerrainManager::_algorithms;

	template< OverhangCoordinateSpace FROM, OverhangCoordinateSpace TO, OverhangTerrainAlignment ALIGN >
	class SpecializedAlgorithmSet : public AlgorithmIndex::IAlgorithmSet
	{
	public:
		void transformSpace(Vector3 & v, const Real nScale = 1.0f) const;
	};

	AlgorithmIndex::AlgorithmIndex()
	{
		// X/Z plane algorithm set
		// From any to OCS_Terrain, X/Z plane
		specializations[OCS_Terrain]			[OCS_Terrain]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_Terrain,			ALIGN_X_Z > ();
		specializations[OCS_Vertex]				[OCS_Terrain]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_Terrain,			ALIGN_X_Z > ();
		specializations[OCS_World]				[OCS_Terrain]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_World,				OCS_Terrain,			ALIGN_X_Z > ();
		specializations[OCS_PagingStrategy]		[OCS_Terrain]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_Terrain,			ALIGN_X_Z > ();
		specializations[OCS_DataGrid]			[OCS_Terrain]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_Terrain,			ALIGN_X_Z > ();

		// From any to OCS_Vertex, X/Z plane
		specializations[OCS_Terrain]			[OCS_Vertex]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_Vertex,				ALIGN_X_Z > ();
		specializations[OCS_Vertex]				[OCS_Vertex]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_Vertex,				ALIGN_X_Z > ();
		specializations[OCS_World]				[OCS_Vertex]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_World,				OCS_Vertex,				ALIGN_X_Z > ();
		specializations[OCS_PagingStrategy]		[OCS_Vertex]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_Vertex,				ALIGN_X_Z > ();
		specializations[OCS_DataGrid]			[OCS_Vertex]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_Vertex,				ALIGN_X_Z > ();

		// From any to OCS_World, X/Z plane
		specializations[OCS_Terrain]			[OCS_World]				[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_World,				ALIGN_X_Z > ();
		specializations[OCS_Vertex]				[OCS_World]				[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_World,				ALIGN_X_Z > ();
		specializations[OCS_World]				[OCS_World]				[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_World,				OCS_World,				ALIGN_X_Z > ();
		specializations[OCS_PagingStrategy]		[OCS_World]				[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_World,				ALIGN_X_Z > ();
		specializations[OCS_DataGrid]			[OCS_World]				[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_World,				ALIGN_X_Z > ();

		// From any to OCS_PagingStrategy, X/Z plane
		specializations[OCS_Terrain]			[OCS_PagingStrategy]	[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_PagingStrategy,		ALIGN_X_Z > ();
		specializations[OCS_Vertex]				[OCS_PagingStrategy]	[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_PagingStrategy,		ALIGN_X_Z > ();
		specializations[OCS_World]				[OCS_PagingStrategy]	[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_World,				OCS_PagingStrategy,		ALIGN_X_Z > ();
		specializations[OCS_PagingStrategy]		[OCS_PagingStrategy]	[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_PagingStrategy,		ALIGN_X_Z > ();
		specializations[OCS_DataGrid]			[OCS_PagingStrategy]	[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_PagingStrategy,		ALIGN_X_Z > ();

		// From any to OCS_DataGrid, X/Z plane
		specializations[OCS_Terrain]			[OCS_DataGrid]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_DataGrid,			ALIGN_X_Z > ();
		specializations[OCS_Vertex]				[OCS_DataGrid]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_DataGrid,			ALIGN_X_Z > ();
		specializations[OCS_World]				[OCS_DataGrid]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_World,				OCS_DataGrid,			ALIGN_X_Z > ();
		specializations[OCS_PagingStrategy]		[OCS_DataGrid]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_DataGrid,			ALIGN_X_Z > ();
		specializations[OCS_DataGrid]			[OCS_DataGrid]			[ALIGN_X_Z] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_DataGrid,			ALIGN_X_Z > ();

		// X/Y plane algorithm set
		// From any to OCS_Terrain, X/Y plane
		specializations[OCS_Terrain]			[OCS_Terrain]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_Terrain,			ALIGN_X_Y > ();
		specializations[OCS_Vertex]				[OCS_Terrain]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_Terrain,			ALIGN_X_Y > ();
		specializations[OCS_World]				[OCS_Terrain]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_World,				OCS_Terrain,			ALIGN_X_Y > ();
		specializations[OCS_PagingStrategy]		[OCS_Terrain]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_Terrain,			ALIGN_X_Y > ();
		specializations[OCS_DataGrid]			[OCS_Terrain]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_Terrain,			ALIGN_X_Y > ();

		// From any to OCS_Vertex, X/Y plane
		specializations[OCS_Terrain]			[OCS_Vertex]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_Vertex,				ALIGN_X_Y > ();
		specializations[OCS_Vertex]				[OCS_Vertex]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_Vertex,				ALIGN_X_Y > ();
		specializations[OCS_World]				[OCS_Vertex]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_World,				OCS_Vertex,				ALIGN_X_Y > ();
		specializations[OCS_PagingStrategy]		[OCS_Vertex]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_Vertex,				ALIGN_X_Y > ();
		specializations[OCS_DataGrid]			[OCS_Vertex]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_Vertex,				ALIGN_X_Y > ();

		// From any to OCS_World, X/Y plane
		specializations[OCS_Terrain]			[OCS_World]				[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_World,				ALIGN_X_Y > ();
		specializations[OCS_Vertex]				[OCS_World]				[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_World,				ALIGN_X_Y > ();
		specializations[OCS_World]				[OCS_World]				[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_World,				OCS_World,				ALIGN_X_Y > ();
		specializations[OCS_PagingStrategy]		[OCS_World]				[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_World,				ALIGN_X_Y > ();
		specializations[OCS_DataGrid]			[OCS_World]				[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_World,				ALIGN_X_Y > ();

		// From any to OCS_PagingStrategy, X/Y plane
		specializations[OCS_Terrain]			[OCS_PagingStrategy]	[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_PagingStrategy,		ALIGN_X_Y > ();
		specializations[OCS_Vertex]				[OCS_PagingStrategy]	[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_PagingStrategy,		ALIGN_X_Y > ();
		specializations[OCS_World]				[OCS_PagingStrategy]	[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_World,				OCS_PagingStrategy,		ALIGN_X_Y > ();
		specializations[OCS_PagingStrategy]		[OCS_PagingStrategy]	[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_PagingStrategy,		ALIGN_X_Y > ();
		specializations[OCS_DataGrid]			[OCS_PagingStrategy]	[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_PagingStrategy,		ALIGN_X_Y > ();

		// From any to OCS_DataGrid, X/Y plane
		specializations[OCS_Terrain]			[OCS_DataGrid]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_DataGrid,			ALIGN_X_Y > ();
		specializations[OCS_Vertex]				[OCS_DataGrid]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_DataGrid,			ALIGN_X_Y > ();
		specializations[OCS_World]				[OCS_DataGrid]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_World,				OCS_DataGrid,			ALIGN_X_Y > ();
		specializations[OCS_PagingStrategy]		[OCS_DataGrid]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_DataGrid,			ALIGN_X_Y > ();
		specializations[OCS_DataGrid]			[OCS_DataGrid]			[ALIGN_X_Y] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_DataGrid,			ALIGN_X_Y > ();

		// Y/Z plane algorithm set
		// From any to OCS_Terrain, Y/Z plane
		specializations[OCS_Terrain]			[OCS_Terrain]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_Terrain,			ALIGN_Y_Z > ();
		specializations[OCS_Vertex]				[OCS_Terrain]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_Terrain,			ALIGN_Y_Z > ();
		specializations[OCS_World]				[OCS_Terrain]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_World,				OCS_Terrain,			ALIGN_Y_Z > ();
		specializations[OCS_PagingStrategy]		[OCS_Terrain]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_Terrain,			ALIGN_Y_Z > ();
		specializations[OCS_DataGrid]			[OCS_Terrain]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_Terrain,			ALIGN_Y_Z > ();

		// From any to OCS_Vertex, Y/Z plane
		specializations[OCS_Terrain]			[OCS_Vertex]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_Vertex,				ALIGN_Y_Z > ();
		specializations[OCS_Vertex]				[OCS_Vertex]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_Vertex,				ALIGN_Y_Z > ();
		specializations[OCS_World]				[OCS_Vertex]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_World,				OCS_Vertex,				ALIGN_Y_Z > ();
		specializations[OCS_PagingStrategy]		[OCS_Vertex]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_Vertex,				ALIGN_Y_Z > ();
		specializations[OCS_DataGrid]			[OCS_Vertex]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_Vertex,				ALIGN_Y_Z > ();

		// From any to OCS_World, Y/Z plane
		specializations[OCS_Terrain]			[OCS_World]				[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_World,				ALIGN_Y_Z > ();
		specializations[OCS_Vertex]				[OCS_World]				[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_World,				ALIGN_Y_Z > ();
		specializations[OCS_World]				[OCS_World]				[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_World,				OCS_World,				ALIGN_Y_Z > ();
		specializations[OCS_PagingStrategy]		[OCS_World]				[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_World,				ALIGN_Y_Z > ();
		specializations[OCS_DataGrid]			[OCS_World]				[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_World,				ALIGN_Y_Z > ();

		// From any to OCS_PagingStrategy, Y/Z plane
		specializations[OCS_Terrain]			[OCS_PagingStrategy]	[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_PagingStrategy,		ALIGN_Y_Z > ();
		specializations[OCS_Vertex]				[OCS_PagingStrategy]	[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_PagingStrategy,		ALIGN_Y_Z > ();
		specializations[OCS_World]				[OCS_PagingStrategy]	[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_World,				OCS_PagingStrategy,		ALIGN_Y_Z > ();
		specializations[OCS_PagingStrategy]		[OCS_PagingStrategy]	[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_PagingStrategy,		ALIGN_Y_Z > ();
		specializations[OCS_DataGrid]			[OCS_PagingStrategy]	[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_PagingStrategy,		ALIGN_Y_Z > ();

		// From any to OCS_DataGrid, Y/Z plane
		specializations[OCS_Terrain]			[OCS_DataGrid]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_Terrain,			OCS_DataGrid,			ALIGN_Y_Z > ();
		specializations[OCS_Vertex]				[OCS_DataGrid]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_Vertex,			OCS_DataGrid,			ALIGN_Y_Z > ();
		specializations[OCS_World]				[OCS_DataGrid]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_World,				OCS_DataGrid,			ALIGN_Y_Z > ();
		specializations[OCS_PagingStrategy]		[OCS_DataGrid]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_PagingStrategy,	OCS_DataGrid,			ALIGN_Y_Z > ();
		specializations[OCS_DataGrid]			[OCS_DataGrid]			[ALIGN_Y_Z] =	new SpecializedAlgorithmSet< OCS_DataGrid,			OCS_DataGrid,			ALIGN_Y_Z > ();
	}

	AlgorithmIndex::~AlgorithmIndex()
	{
		for (unsigned a = 0; a < NumOCS; ++a)
			for (unsigned b = 0; b < NumOCS; ++b)
				for (unsigned c = 0; c < NumTerrainAlign; ++c)
					delete specializations[a][b][c];
	}

	OverhangTerrainManager::OverhangTerrainManager( const OverhangTerrainOptions & opts, OverhangTerrainSceneManager * tsm) 
		: options(opts), _pScnMgr(tsm)
	{
		oht_register_mainthread();
		tsm->setTerrainManager(this);
	}

	// TODO: Test space conversion from OCS_DataGrid to OCS_World and OCS_PagingStrategy, confusion about nature of enalAlignment.
	
	template< OverhangCoordinateSpace FROM, OverhangCoordinateSpace TO, OverhangTerrainAlignment ALIGN >
	void SpecializedAlgorithmSet<FROM, TO, ALIGN>::transformSpace(Vector3 & v, const Real nScale = 1.0f) const
	{
		switch (FROM)
		{
		case OCS_DataGrid:
			std::swap(v.y, v.z);
			// CONTINUE to next CASE
		case OCS_Vertex:
			v *= nScale;
			// CONTINUE to next CASE
		case OCS_PagingStrategy:
		case OCS_Terrain:
			if (FROM == OCS_PagingStrategy)
			{
				switch (ALIGN)
				{
				case ALIGN_X_Z:
					v.y = -v.y;
					break;
				case ALIGN_Y_Z:
					v.x = -v.x;
					break;
				}
			}

			switch (TO)
			{
			case OCS_DataGrid:
				std::swap(v.y, v.z);
				// CONTINUE to next CASE
			case OCS_Vertex:
				v /= nScale;
				break;
			case OCS_World:
				switch (ALIGN)
				{
				case ALIGN_X_Z:
					std::swap(v.y, v.z);
					break;
				case ALIGN_Y_Z:
					std::swap(v.x, v.z);
					break;
				}
				break;
			case OCS_PagingStrategy:
				switch (ALIGN)
				{
				case ALIGN_X_Z:
					v.y = -v.y;
					break;
				case ALIGN_Y_Z:
					v.x = -v.x;
					break;
				}
				break;
			}
			break;
		case OCS_World:
			switch (TO)
			{
			case OCS_DataGrid:
				std::swap(v.y, v.z);
				// CONTINUE to next CASE
			case OCS_Vertex:
				v /= nScale;
				// CONTINUE to next CASE
			case OCS_Terrain:
			case OCS_PagingStrategy:
				if (TO == OCS_PagingStrategy)
				{
					switch (ALIGN)
					{
					case ALIGN_X_Z:
					case ALIGN_Y_Z:
						v.z = -v.z;
						break;
					}
				}

				switch (ALIGN)
				{
				case ALIGN_X_Z:
					std::swap(v.y, v.z);
					break;
				case ALIGN_Y_Z:
					std::swap(v.x, v.z);
					break;
				}
				break;
			}

			break;
		}
	} 

	OverhangTerrainManager::RayQueryParams::Channels::Channels()
		: size(0), array(NULL)
	{

	}

	OverhangTerrainManager::RayQueryParams::Channels::Channels( const std::list< Channel::Ident > & channels )
		: array( new Channel::Ident[channels.size()]), size(channels.size())
	{
		size_t c = 0;
		for (std::list< Channel::Ident >::const_iterator i = channels.begin(); i != channels.end(); ++i)
			array[c++] = *i;
	}

	OverhangTerrainManager::RayQueryParams::Channels::~Channels()
	{
		delete [] array;
	}


	OverhangTerrainManager::RayQueryParams::RayQueryParams( const Real nLimit, const Channels & channels )
		: limit(nLimit), channels(channels)
	{

	}

	OverhangTerrainManager::RayQueryParams::RayQueryParams( const Real nLimit )
		: limit(nLimit)
	{

	}

	Ogre::OverhangTerrainManager::RayQueryParams OverhangTerrainManager::RayQueryParams::from( const Real nLimit )
	{
		return RayQueryParams(nLimit);
	}

	Ogre::OverhangTerrainManager::RayQueryParams OverhangTerrainManager::RayQueryParams::from( const Real nLimit, Channel::Ident channel, ... )
	{
		va_list aenChannels;
		std::list< Channel::Ident > channels;

		va_start (aenChannels, channel);

		while (channel != Channel::Ident_INVALID)
		{
			channels.push_back(channel);
			channel = va_arg(aenChannels, const Channel::Ident);
		}
		va_end(aenChannels);

		return RayQueryParams(nLimit, channels);
	}


	OverhangTerrainManager::RayQueryParams::Channels::AbstractIterator::AbstractIterator( const AbstractIterator & copy )
		: _current(copy._current)
	{

	}

	OverhangTerrainManager::RayQueryParams::Channels::AbstractIterator::AbstractIterator( const AbstractIterator && move )
		: _current(move._current)
	{

	}

	OverhangTerrainManager::RayQueryParams::Channels::AbstractIterator::AbstractIterator()
	{
	}

	void OverhangTerrainManager::RayQueryParams::Channels::AbstractIterator::iterate()
	{
		_current = step();
	}

	OverhangTerrainManager::RayQueryParams::Channels::ArrayIterator::ArrayIterator( const ArrayIterator & copy )
	:	AbstractIterator(copy),
		_size(copy._size), _array(copy._array), _descriptor(copy._descriptor),
		_c(copy._c), _current(copy._current), OHT_CR_COPY(copy)
	{

	}

	OverhangTerrainManager::RayQueryParams::Channels::ArrayIterator::ArrayIterator( const ArrayIterator && move )
	:	AbstractIterator(static_cast< const AbstractIterator && > (move)),
		_size(move._size), _array(move._array), _descriptor(static_cast< const Channel::Descriptor && > (move._descriptor)),
		_c(move._c), _current(move._current), OHT_CR_COPY(move)
	{

	}

	OverhangTerrainManager::RayQueryParams::Channels::ArrayIterator::ArrayIterator( const Channel::Descriptor & descriptor, const Channel::Ident * const array, const size_t size, const size_t c /*= 0*/ )
	:	AbstractIterator(),
		_size(size), _array(array), _descriptor(descriptor),
		_c (c)
	{
		OHT_CR_INIT();
		iterate();
	}

	Channel::Ident OverhangTerrainManager::RayQueryParams::Channels::ArrayIterator::step()
	{
		OHT_CR_START();

		while (_c++ < _size)
		{
			OHT_CR_RETURN(CRS_Default, Channel::Ident(_array[_c]));
		}

		return Channel::Ident();

		OHT_CR_END();
	}

}
