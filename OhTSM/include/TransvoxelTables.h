//================================================================================
//
// The Transvoxel Algorithm look-up tables
//
// The following data originates from Eric Lengyel's Transvoxel Algorithm.
// http://www.terathon.com/voxels/
//
// The data in this file may be freely used in implementations of the Transvoxel
// Algorithm. If you do use this data, or any transformation of it, in your own
// projects, commercial or otherwise, please give credit by indicating in your
// source code that the data is part of the author's implementation of the
// Transvoxel Algorithm and that it came from the web address given above.
// (Simply copying and pasting the two lines of the previous paragraph would be
// perfect.) If you distribute a commercial product with source code included,
// then the credit in the source code is required.
//
// If you distribute any kind of product that uses this data, a credit visible to
// the end-user would be appreciated, but it is not required. However, you may
// not claim that the entire implementation of the Transvoxel Algorithm is your
// own if you use the data in this file or any transformation of it.
//
// The format of the data in this file is described in the dissertation "Voxel-
// Based Terrain for Real-Time Virtual Simulations", available at the web page
// given above. References to sections and figures below pertain to that paper.
//
//================================================================================

// The RegularCellData structure holds information about the triangulation
// used for a single equivalence class in the modified Marching Cubes algorithm,
// described in Section 3.2.

#ifndef __TRANSVOXELTABLES_H__
#define __TRANSVOXELTABLES_H__

namespace Ogre
{
	struct RegularCellData
	{
		unsigned char	geometryCounts;		// High nibble is vertex count, low nibble is triangle count.
		unsigned char	vertexIndex[15];	// Groups of 3 indexes giving the triangulation.

		long GetVertexCount(void) const
		{
			return (geometryCounts >> 4);
		}

		long GetTriangleCount(void) const
		{
			return (geometryCounts & 0x0F);
		}
	};


	// The TransitionCellData structure holds information about the triangulation
	// used for a single equivalence class in the Transvoxel Algorithm transition cell,
	// described in Section 4.3.

	struct TransitionCellData
	{
		long			geometryCounts;		// High nibble is vertex count, low nibble is triangle count.
		unsigned char	vertexIndex[36];	// Groups of 3 indexes giving the triangulation.

		long GetVertexCount(void) const
		{
			return (geometryCounts >> 4);
		}

		long GetTriangleCount(void) const
		{
			return (geometryCounts & 0x0F);
		}
	};


	// The regularCellClass table maps an 8-bit regular Marching Cubes case index to
	// an equivalence class index. Even though there are 18 equivalence classes in our
	// modified Marching Cubes algorithm, a couple of them use the same exact triangulations,
	// just with different vertex locations. We combined those classes for this table so
	// that the class index ranges from 0 to 15.

	const extern unsigned char regularCellClass[256];


	// The regularCellData table holds the triangulation data for all 16 distinct classes to
	// which a case can be mapped by the regularCellClass table.

	const extern RegularCellData regularCellData[16];

	// The regularVertexData table gives the vertex locations for every one of the 256 possible
	// cases in the modified Marching Cubes algorithm. Each 16-bit value also provides information
	// about whether a vertex can be reused from a neighboring cell. See Section 3.3 for details.
	// The low byte contains the indexes for the two endpoints of the edge on which the vertex lies,
	// as numbered in Figure 3.7. The high byte contains the vertex reuse data shown in Figure 3.8.

	const extern unsigned short regularVertexData[256][12];


	// The transitionCellClass table maps a 9-bit transition cell case index to an equivalence
	// class index. Even though there are 73 equivalence classes in the Transvoxel Algorithm,
	// several of them use the same exact triangulations, just with different vertex locations.
	// We combined those classes for this table so that the class index ranges from 0 to 55.
	// The high bit is set in the cases for which the inverse state of the voxel data maps to
	// the equivalence class, meaning that the winding order of each triangle should be reversed.

	const extern unsigned char transitionCellClass[512];


	// The transitionCellData table holds the triangulation data for all 56 distinct classes to
	// which a case can be mapped by the transitionCellClass table. The class index should be ANDed
	// with 0x7F before using it to look up triangulation data in this table.

	const extern TransitionCellData transitionCellData[56];


	// The transitionCornerData table contains the transition cell corner reuse data
	// shown in Figure 4.18.

	const extern unsigned char transitionCornerData[13];


	// The transitionVertexData table gives the vertex locations for every one of the 512 possible
	// cases in the Transvoxel Algorithm. Each 16-bit value also provides information about whether
	// a vertex can be reused from a neighboring cell. See Section 4.5 for details. The low byte
	// contains the indexes for the two endpoints of the edge on which the vertex lies, as numbered
	// in Figure 4.16. The high byte contains the vertex reuse data shown in Figure 4.18.

	const extern unsigned short transitionVertexData[512][12];
}

#endif