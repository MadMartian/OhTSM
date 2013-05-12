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
#ifndef __OVERHANGLODRENDERABLE_H__
#define __OVERHANGLODRENDERABLE_H__

#include <OgreRenderable.h>
#include <OgreCamera.h>

#include "OverhangTerrainPrerequisites.h"
#include "OverhangTerrainOptions.h"

namespace Ogre
{
	/** OGRE renderable type that supports multiple resolutions and LOD morphing between them */
	class _OverhangTerrainPluginExport LODRenderable : public Renderable, public MovableObject
	{
	public:
		/** 
		@param nLODLevels The number of levels of detail used by this renderable for multi-resolution rendering
		@param fPixelError The maximum number of pixels allowed on the screen in error before resolution switching occurs
		@param bMorph Whether to support LOD morphing
		@param fMorphStart Ratio of the camera distance between resolutions when to morphing starts
		@param sName Optional name for this renderable
		*/
		LODRenderable(const size_t nLODLevels, const Real fPixelError, const bool bMorph, const Real fMorphStart, const Ogre::String & sName = "");
		virtual ~LODRenderable();

		/// Determines the camera distance between the different LOD resolutions for switching between them
		virtual void initLODMetrics (const Camera * pCam)
		{
			const Real fErrorFactor = (pCam == NULL ? 1.0f : computeErrorFactor(pCam));
			computeMinimumLevels2Distances(Math::Sqr(fErrorFactor), _vMinLevelDistSQ, _nLODCount);
			refineMinimumLevel2Distances();
		}

        inline int getRenderLevel() const
			{ return _nRenderLevel; };
		void _adjustRenderLevel (int i)
			{ _nRenderLevel = i; }

		/// Retrieve LOD accounting for an optionally forced LOD
		inline int getEffectiveRenderLevel () const
			{ return _nForcedRenderLevel >= 0 ? _nForcedRenderLevel : _nRenderLevel; }
        inline void setForcedRenderLevel( int i )
			{ _nForcedRenderLevel = i; }

		/// Returns the maximum quantity of LOD resolutions supported
		inline size_t getNumLevels () const { return _nLODCount; }

		/// Overridden from MovableObject to update render level state here based on distance from current position of camera
		virtual void _notifyCurrentCamera( Camera* cam );
		/// Overridden from Renderable to allow the morph LOD entry to be set
		virtual void _updateCustomGpuParameter(const GpuProgramParameters::AutoConstantEntry& constantEntry, GpuProgramParameters* params) const;
		virtual void visitRenderables( Renderable::Visitor * visitor, bool debugRenderables = false );

	protected:
		/// Total number of supported LOD levels
		const size_t _nLODCount;

		virtual void computeMinimumLevels2Distances (const Real & fErrorFactorSqr, Real * pfMinLev2DistSqr, const size_t nCount) = 0;
		virtual void setDeltaBinding (const int nLevel) = 0;
	
	private:
		/// Maximum allowed pixel error then LOD changes
		Real _fPixError;

		/// Whether LOD morphing is enabled
		bool _bMorph;
		/// At what point (parametric) should LOD morphing start
		Real _pctMorphStart;
		/// Current render level (unless there is a forced one)
		int _nRenderLevel;
		/// Forced render level overriding the other render level setting, -1 otherwise
		int _nForcedRenderLevel;
		/// List of squared distances at which LODs change
		Real *_vMinLevelDistSQ;
		/// Array of LOD indexes specifying which LOD is the next one down (deals with clustered error metrics which cause LODs to be skipped)
		int *_vNextLevelDown;
		/// The previous 'next' LOD level down, for frame coherency
		int _nNextLevel0; 
		/// The morph factor between this and the next LOD level down
		Real _fLODMorphFactor;

		struct RenderLevelResult
		{
			int level;
			Real L;
		};
		/// Determine the LOD based on the specified camera and its world distance to this renderable
		void computeRenderLevel( const Camera* cam, RenderLevelResult & result );
		/// Computes an error factor based on the specified camera for computing camera distance between resolutions
		Real computeErrorFactor(const Camera * pCam);

		/// Brushes-up the minimum-level to squared distances array, called after calling computeMinimumLevel2Distances
		void refineMinimumLevel2Distances ();
	};

}

#endif
