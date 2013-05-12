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

#include "LODRenderable.h"
#include "DebugTools.h"

namespace Ogre
{
	#define MORPH_CUSTOM_PARAM_ID 77

	LODRenderable::LODRenderable(const size_t nLODLevels, const Real fPixelError, const bool bMorph, const Real fMorphStart, const Ogre::String & sName /*= ""*/)
	:	MovableObject(sName),
		_nLODCount(nLODLevels), _nRenderLevel(0), _nForcedRenderLevel(-1), _nNextLevel0(-1), _fPixError(fPixelError), 
		_bMorph(bMorph), _pctMorphStart(fMorphStart),
		_vMinLevelDistSQ(new Real[nLODLevels]), _vNextLevelDown(new int[nLODLevels])
	{
	}

	LODRenderable::~LODRenderable()
	{
		delete [] _vMinLevelDistSQ;
		delete [] _vNextLevelDown;
	}

	void LODRenderable::_notifyCurrentCamera( Camera* cam )
	{
		MovableObject::_notifyCurrentCamera(cam);

		if ( _nForcedRenderLevel >= 0 )
		{
			_nRenderLevel = _nForcedRenderLevel;
			return ;
		}

		RenderLevelResult lvlres;

		computeRenderLevel(cam->getLodCamera(), lvlres);

		_nRenderLevel = lvlres.level;

		if (_bMorph)
		{
			// Get the next LOD level down
			int nextLevel = _vNextLevelDown[_nRenderLevel];
			if (nextLevel == 0)
			{
				// No next level, so never morph
				_fLODMorphFactor = 0;
			}
			else
			{
				// Set the morph such that the morph happens in the last 0.25 of
				// the distance range
				Real range = _vMinLevelDistSQ[nextLevel] - _vMinLevelDistSQ[_nRenderLevel];
				if (range)
				{
					Real percent = (lvlres.L - _vMinLevelDistSQ[_nRenderLevel]) / range;
					// scale result so that msLODMorphStart == 0, 1 == 1, clamp to 0 below that
					Real rescale = 1.0f / (1.0f - _pctMorphStart);
					_fLODMorphFactor = std::max((percent - _pctMorphStart) * rescale, 
						static_cast<Real>(0.0));
				}
				else
				{
					// Identical ranges
					_fLODMorphFactor = 0.0f;
				}

				assert(_fLODMorphFactor >= 0 && _fLODMorphFactor <= 1);
			}

			// Bind the correct delta buffer if it has changed
			// nextLevel - 1 since the first entry is for LOD 1 (since LOD 0 never needs it)
			if (_nNextLevel0 != nextLevel)
			{
				if (nextLevel > 0)
				{
					setDeltaBinding(nextLevel - 1);
				}
				else
				{
					// bind dummy (incase bindings checked)
					setDeltaBinding(0);
				}
			}
			_nNextLevel0 = nextLevel;

		}
	}

	void LODRenderable::_updateCustomGpuParameter( const GpuProgramParameters::AutoConstantEntry& constantEntry, GpuProgramParameters* params ) const
	{
		if (constantEntry.data == MORPH_CUSTOM_PARAM_ID)
		{
			// Update morph LOD factor
			params->_writeRawConstant(constantEntry.physicalIndex, _fLODMorphFactor);
		}
		else
		{
			Renderable::_updateCustomGpuParameter(constantEntry, params);
		}
	}

	void LODRenderable::computeRenderLevel( const Camera* cam, RenderLevelResult & result )
	{
		Vector3 cpos = cam -> getDerivedPosition();
		const AxisAlignedBox& aabb = getWorldBoundingBox(true);
		Vector3 diff(0, 0, 0);
		diff.makeFloor(cpos - aabb.getMinimum());
		diff.makeCeil(cpos - aabb.getMaximum());

		result.L = diff.squaredLength();
		result.level = -1;

		for ( size_t i = 0; i < _nLODCount; i++ )
		{
			if ( _vMinLevelDistSQ[ i ] > result.L )
			{
				result.level = i - 1;
				break;
			}
		}

		if ( result.level < 0 )
			result.level = _nLODCount - 1;
	}

	void LODRenderable::refineMinimumLevel2Distances()
	{
		// Post validate the whole set
		for ( size_t i = 1; i < _nLODCount; i++ )
		{

			// Make sure no LOD transition within the tile
			// This is especially a problem when using large tiles with flat areas
			/* Hmm, this can look bad on some areas, disable for now
			Vector3 delta(_vertex(0,0,0), mCenter.y, _vertex(0,0,2));
			delta = delta - mCenter;
			Real minDist = delta.squaredLength();
			mMinLevelDistSqr[ i ] = std::max(mMinLevelDistSqr[ i ], minDist);
			*/

			//make sure the levels are increasing...
			if ( _vMinLevelDistSQ[ i ] < _vMinLevelDistSQ[ i - 1 ] )
			{
				_vMinLevelDistSQ[ i ] = _vMinLevelDistSQ[ i - 1 ];
			}
		}

		// Now reverse traverse the list setting the 'next level down'
		Real lastDist = -1;
		int lastIndex = 0;
		int c, i;
		for (c = i = _nLODCount - 1; c >= 0; --i, --c)
		{
			if (i == _nLODCount - 1)
			{
				// Last one is always 0
				lastIndex = i;
				lastDist = _vMinLevelDistSQ[i];
				_vNextLevelDown[i] = 0;
			}
			else
			{
				_vNextLevelDown[i] = lastIndex;
				if (_vMinLevelDistSQ[i] != lastDist)
				{
					lastIndex = i;
					lastDist = _vMinLevelDistSQ[i];
				}
			}

		}
	}

	Ogre::Real LODRenderable::computeErrorFactor( const Camera * pCam )
	{
		Real A, T;

		//A = 1 / Math::Tan(Math::AngleUnitsToRadians(opts.primaryCamera->getFOVy()));
		// Turn off detail compression at higher FOVs
		A = 1.0f;

		int vertRes = pCam->getViewport()->getActualHeight();

		T = 2 * ( Real ) _fPixError / ( Real ) vertRes;

		return A / T;
	}

	void LODRenderable::visitRenderables( Renderable::Visitor * visitor, bool debugRenderables /*= false */ )
	{
		oht_assert_threadmodel(ThrMdl_Main);
		// Will blow-up if the material for this (IsoSurfaceRenderable) is unset
		// TODO: Guarantee that material is set before rendering the scene
		visitor->visit(this, _nForcedRenderLevel >= 0 ? _nForcedRenderLevel : _nRenderLevel, false);
	}

}
