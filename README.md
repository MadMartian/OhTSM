OverhangTerrainSceneManager (OhTSM) - Volume Rendered Terrain Paging Engine
===========================================================================
Version: 0.2a
Contributors: Jonathan Neufeld (http://www.extollit.com)
Original Author: Martin Enge (martin.enge@gmail.com)
Credit: Transvoxel by Eric Lengyel (http://www.terathon.com/voxels/)
Description: 
	This is a volumetric terrain rendering library that supports paging.  It is
based on the old OverhangTerrainSceneManager library originally written by 
Martin Enge.  It has been modified to use the Transvoxel algorithm for multi-
resolution isosurface stitching.  If you are familiar with legacy 
OverhangTerrainSceneManager see CHANGES (below).

NOTE: Please review the KNOWN ISSUES (below) before using this library

MODULES
=======
Example(OhTSM)
--------------
	This is a sample application that demonstrates the 
OverhangTerrainSceneManager library with simple waveform-generated terrain 
using a straight blue material.  Click with the left mouse-button to dig and 
right mouse-button to build.  Use the keys W.A.S.D. to move around the scene.

OhTSM
-----
	This is the OverhangTerrainSceneManager library.  Files in the 
"Core Sources" and "Core Headers" folders are internal APIs whereas the 
"Header Files" and "Source Files" folders are meant to be public-facing.

CHANGES
=======
There are many difference from this version of OverhangTerrainSceneManager from
the legacy version originally written by Martin Enge.  More than 90% of the 
codebase has been rewritten to facilitate the use of Transvoxel and concurrent 
page loading.
-	There are no height-map terrain renderables anymore, heightmaps are 
	converted into voxel data and isosurfaces are extracted from them.  This 
	was necessary to overcome the impossibly complex challenges of multi-
	resolution stitching between isosurfaces and non-isosurface renderables.
-	IsoSurfaceBuilder has been completely rewritten to use Transvoxel
-	Code for plugging into an editor has been dropped
-	Terrain material logic has been dropped, library users are fully 
	responsible for supplying materials for the terrain
-	Dynamic WorkQueue-based terrain paging and serialization
-	The scene manager no longer inherits from OctreeSceneManager, the octree 
	doesn't play nice with dynamic paged terrain
-	Voxel data is preserved for each isosurface and compressed using RLE 
	compression rather than relying on dynamically populating a singleton voxel
	grid from meta objects for each query, which would be prohibitively 
	expensive
-	Isosurface metadata is preserved for fast generation of multiple 
	resolutions
-	New vertices from new stitch configurations of the same resolution for an 
	isosurface are appended to the same hardware vertex buffer rather than 
	replacing its contents completely
-	Geomorphing has been dropped
-	Metaball voxelization algorithm uses a different formula than the one 
	recommended by Geiss (http://www.geisswerks.com/ryan/BLOBS/blobs.html)

KNOWN ISSUES
============
(10-May-2013)
-	Only ray scene queries are supported, other types of scene queries are not 
	implemented yet
-	Some shading problems along transition cell seams due to incorrect normal 
	calculation
-	Geometry batching is rather high resulting in a choppy render frame rate
-	Only metaballs are currently supported for volumetric deformation
-	Volumetric deformation performance is inconsistent possibly related to 
	geometry batching, it is fastest for example when digging straight downward
	and beneath the ground some
-	Geomorphing between resolutions is not supported
-	Some cracks appear between two adjacent surfaces when the LOD difference 
	between them is greater than one, this is due to a simplistic LOD 
	calculation algorithm
-	Gradient normal calculation is the only reliable method for producing 
	geometry with the least shading artefacts, weighted average and average 
	normal calculation does not take into account adjacent isosurface 
	geometries
-	Transition cell geometry erratic and current implementation of Transvoxel's
	response to this (Lengyel p.39) actually makes the problem worse
-	Threading and concurrency has had only limited testing, race conditions may
	exist
-	Adding meta objects to the scene may touch more terrain pages than is 
	necessary
-	No versioning or CRC check on terrain serialization / deserialization
-	Clean-up is a little klunky, Ogre::WorkQueue doesn't appear to support 
	waiting on task completion
-	Texture-coordinates are not properly implemented, most obvious solution is 
	to employ (expensive) triplanar texturing
-	Meta objects are not removed from memory after a page is destroyed / 
	unloaded
-	Ray queries sometimes fail, unclear why this is, behavior is inconsistent
-	Material-per-tile not tested, might even be non-functional
-	Loading or changing main top-level configuration options during runtime has
	no impact on the scene, there is no business logic to re-initialize the 
	scene and/or dependencies
-	Iteration through meta-fragments is not public-facing, only used for 
	internal purposes
-	Meta balls do not account for terrain group origin

INSTALLATION
============
Prerequisites:
- OGRE 1.7
- Visual Studio 2010

A good understanding of and familiarity with the OGRE SDK is a prerequisite to
using this library.  OverhangTerrainSceneManager was built against OGRE v1.7.
The Visual Studio development environment depends on the settings of the 
following environment variables each of which points to a path on disk:
- DXSDK_DIR: Location of your DirectX SDK
- BOOST_HOME: C++ Boost library used by OGRE
- OGRE_SRC: OGRE source files and includes including "lib/Debug" and 
	"lib/Release" subfolders
- OGRE_DEPS: Home directory of OGRE dependencies including "lib/Debug" and 
	"lib/Release" subfolders