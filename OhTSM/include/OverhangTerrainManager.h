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
#ifndef __OVERHANGTERRAINMANAGER_H__
#define __OVERHANGTERRAINMANAGER_H__

#include <OgreVector3.h>
#include <OgreAxisAlignedBox.h>

#include <list>

#include "OverhangTerrainPrerequisites.h"
#include "Util.h"
#include "Types.h"
#include "ChannelIndex.h"
#include "OverhangTerrainOptions.h"

namespace Ogre
{
	/// Provides a database of algorithms that implement the algorithm interface described below for transforming coordinates
	class AlgorithmIndex
	{
	public:
		/// Provides a set of algorithms for a specific set of implied conditions, currently only one algorithm is supported
		class IAlgorithmSet
		{
		public:
			/** Algorithm for transforming the coordinate space of the specified vector
			@param v Vector whose coordinates will be transformed
			@param nScale Scale of the coordinates if applicable */
			virtual void transformSpace(Vector3 & v, const Real nScale = 1.0f) const = 0;
		} * specializations[NumOCS][NumOCS][NumTerrainAlign];

		AlgorithmIndex();
		~AlgorithmIndex();
	};

	/** Base class for the OverhangTerrainGroup class, also knows how to transform coordinate systems */
	class OverhangTerrainManager
	{
	public:
		/// The main top-level OhTSM configuration options
		const OverhangTerrainOptions options;

		/** 
		@param opts The main top-level OhTSM configuration options
		@param tsm Pointer to the scene manager
		*/
		OverhangTerrainManager(const OverhangTerrainOptions & opts, OverhangTerrainSceneManager * tsm);

		/** Parameters used to influence a ray query */
		class _OverhangTerrainPluginExport RayQueryParams
		{
		public:
			/// Distance limit in world units to terminate ray searching
			Real limit;

			/** Describes channels factored into the ray query */
			class _OverhangTerrainPluginExport Channels
			{
			public:
				/// The list of channels (identifiers) factored-into the query
				Channel::Ident * const array;
				/// Size of the channel identifier array above
				const size_t size;

				Channels();
				/**
				@param channels A collection of channels (identifiers) used to factor-into the query, other channels will be ignored in the query
				*/
				Channels(const std::list< Channel::Ident > & channels);
				~Channels();

				class _OverhangTerrainPluginExport AbstractIterator : public std::iterator< std::input_iterator_tag, Channel::Ident >
				{
				private:
					Channel::Ident _current;

				protected:
					virtual Channel::Ident step () = 0;

					void iterate ();

				public:
					AbstractIterator (const AbstractIterator & copy);
					AbstractIterator (const AbstractIterator && move);
					AbstractIterator ();

					inline const Channel::Ident & operator * () const { return _current; }
					inline const Channel::Ident * operator -> () const { return &_current; }

					inline bool operator == (const AbstractIterator & other) const { return _current == other._current; }
					inline bool operator != (const AbstractIterator & other) const { return _current != other._current; }

					AbstractIterator & operator ++ ()
					{
						iterate();
						return *this;
					}

					AbstractIterator * operator ++ (int)
					{
						AbstractIterator * pOld = this;
						iterate();
						return this;
					}
				};

				class _OverhangTerrainPluginExport ArrayIterator : public AbstractIterator
				{
				private:
					const Channel::Descriptor _descriptor;

					const size_t _size;
					const Channel::Ident * const _array;

					enum CRState
					{
						CRS_Default = 1
					};
					OHT_CR_CONTEXT;

					size_t _c;
					Channel::Ident _current;

				protected:
					virtual Channel::Ident step();

				public:
					ArrayIterator (const ArrayIterator & copy);
					ArrayIterator (const ArrayIterator && move);

					ArrayIterator (const Channel::Descriptor & descriptor, const Channel::Ident * const array, const size_t size, const size_t c = 0);
				};

				template< typename INDEX >
				class IndexIterator : public AbstractIterator
				{
				private:
					const INDEX & _index;
					typename INDEX::const_iterator _i;

				protected:
					virtual Channel::Ident step()
					{
						return _i++ ->channel;
					}

				public:
					IndexIterator (const IndexIterator & copy)
						: AbstractIterator(copy), _index(copy._index), _i(copy._i) {}
					IndexIterator (const IndexIterator && move)
						: AbstractIterator(static_cast< AbstractIterator && > (move)), _index(move._index), _i(static_cast< INDEX::const_iterator && > (move._i)) {}
					IndexIterator(const INDEX & index, typename INDEX::const_iterator current)
						: AbstractIterator(), _index(index), _i(current)
					{
						iterate();
					}
				};

				template< typename INDEX >
				SharedPtr< AbstractIterator > begin(const INDEX & index) const
				{
					if (this->array == NULL)
						return SharedPtr< AbstractIterator > (new IndexIterator< INDEX > (index, index.begin()));
					else
						return SharedPtr< AbstractIterator > (new ArrayIterator(index.descriptor, this->array, this->size));
				}

				template< typename INDEX >
				SharedPtr< AbstractIterator > end(const INDEX & index) const
				{
					if (this->array == NULL)
						return SharedPtr< AbstractIterator > (new IndexIterator< INDEX > (index, index.end()));
					else
						return SharedPtr< AbstractIterator > (new ArrayIterator(index.descriptor, this->array, this->size, this->size));
				}

			} channels;

		private:
			/**
			@param nLimit Distance limit in world units to terminate ray searching
			@param channels A collection of channels (identifiers) used to factor-into the query, other channels will be ignored in the query
			*/
			RayQueryParams(const Real nLimit, const Channels & channels);
			/**
			@param nLimit Distance limit in world units to terminate ray searching
			*/
			RayQueryParams(const Real nLimit);

		public:
			/**
			@param nLimit Distance limit in world units to terminate ray searching
			*/
			static RayQueryParams from (const Real nLimit);
			/**
			@param nLimit Distance limit in world units to terminate ray searching
			@param channel An inline ellipsis-list of channels (identifiers) used to factor-into the query, other channels will be ignored in the query
			*/
			static RayQueryParams from (const Real nLimit, Channel::Ident channel, ...);
		};

		/** Result from a terrain ray intersection with the terrain group. */
		struct _OverhangTerrainPluginExport RayResult	// TODO: Extract to top-level and reduce inclusion tree
		{
			/// Whether an intersection occurred
			bool hit;
			/// World-space position at which the intersection occurred
			Vector3 position;
			/// Meta-fragment where the intersection occurred
			MetaFragment::Container * mwf;

			/** 
			@param hit Whether an intersection occurred
			@param pos Position at which the intersection occurred
			@param mwf Meta-fragment where the intersection occurred
			*/
			inline RayResult(bool hit, const Vector3& pos, MetaFragment::Container * mwf)
				: hit(hit), position(pos), mwf(NULL) {}
		};

		/** Add a metaball to the scene.
		@param position The absolute world position of the metaball
		@param radius The radius of the metaball sphere in world units
		@param excavating Whether or not the metaball carves out empty space or fills it in with solid
		@param synchronous Whether or not to execute the operation in this thread and not leverage threading capability */
		virtual void addMetaBall(const Vector3 & position, const Real radius, const bool excavating = true, const bool synchronous = false) = 0;
		/** Test for intersection of a given ray with any terrain in the group. If the ray hits a terrain, the point of 
			intersection and terrain instance is returned.
		 @param ray The ray to test for intersection
		 @param params Parameters influencing the ray query including the distance from the ray origin at which we will stop looking or zero for no limit
		 @return A result structure which contains whether the ray hit a terrain and if so, where. */
		virtual RayResult rayIntersects(Ray ray, const OverhangTerrainManager::RayQueryParams & params) const = 0;
		/// Sets the material used on all terrain
		virtual void setMaterial (const Channel::Ident channel, const MaterialPtr & m) = 0;

		/// @returns The scene manager used
		inline OverhangTerrainSceneManager * getSceneManager() const { return _pScnMgr; }

		/** Returns the transformed coordinates of the one specified
		@param encsFrom Source coordinate system
		@param enalAlignment The world alignment of terrain in the scene
		@param encsTo Destination coordinate system
		@param vIn The source coordinates
		@param nScale Scale of the unit distance between terrain vertices 
		@returns A vector representation of the one specified in the destination coordinate system */
		inline static Vector3 toSpace (const OverhangCoordinateSpace encsFrom, const OverhangTerrainAlignment enalAlignment, const OverhangCoordinateSpace encsTo, const Vector3 & vIn, const Real nScale = 1.0)
		{
			Vector3 vRet = vIn;

			transformSpace(encsFrom, enalAlignment, encsTo, vRet, nScale);
			return vRet;
		}

		/** Transforms coordinates
		@param encsFrom Source coordinate system
		@param enalAlignment The world alignment of terrain in the scene
		@param encsTo Destination coordinate system
		@param vIn The source coordinates that will also be transformed
		@param nScale Scale of the unit distance between terrain vertices */
		static inline 
		void transformSpace (const OverhangCoordinateSpace encsFrom, const OverhangTerrainAlignment enalAlignment, const OverhangCoordinateSpace encsTo, Vector3 & v, const Real nScale = 1.0)
		{
			_algorithms.specializations[encsFrom][encsTo][enalAlignment] ->transformSpace(v, nScale);
		}
		/** Transforms the coordinates of a bounding-box
		@param encsFrom Source coordinate system
		@param enalAlignment The world alignment of terrain in the scene
		@param encsTo Destination coordinate system
		@param bbox The source bounding box that will also be transformed
		@param nScale Scale of the unit distance between terrain vertices */
		static inline 
		void transformSpace (const OverhangCoordinateSpace encsFrom, const OverhangTerrainAlignment enalAlignment, const OverhangCoordinateSpace encsTo, AxisAlignedBox & bbox, const Real nScale = 1.0)
		{
			const AlgorithmIndex::IAlgorithmSet * pSpecialAlgo = _algorithms.specializations[encsFrom][encsTo][enalAlignment];
			
			pSpecialAlgo->transformSpace(bbox.getMinimum(), nScale);
			pSpecialAlgo->transformSpace(bbox.getMaximum(), nScale);
		}

		/** Returns the transformed coordinates of the one specified based on the main top-level configuration options
		@param encsFrom Source coordinate system
		@param encsTo Destination coordinate system
		@param vIn The source coordinates
		@returns A vector representation of the one specified in the destination coordinate system */
		inline Vector3 toSpace (const OverhangCoordinateSpace encsFrom, const OverhangCoordinateSpace encsTo, const Vector3 & vIn) const
		{
			Vector3 vRet = vIn;

			transformSpace(encsFrom, encsTo, vRet);
			return vRet;
		}
		/** Transforms coordinates based on the main top-level configuration options
		@param encsFrom Source coordinate system
		@param encsTo Destination coordinate system
		@param vIn The source coordinates that will also be transformed */
		inline void transformSpace (const OverhangCoordinateSpace encsFrom, const OverhangCoordinateSpace encsTo, Vector3 & v) const
			{ transformSpace(encsFrom, options.alignment, encsTo, v, options.cellScale); }

		/** Returns the transformed bounding-box of the one specified
		@param encsFrom Source coordinate system
		@param enalAlignment The world alignment of terrain in the scene
		@param encsTo Destination coordinate system
		@param bboxIn The source bounding box
		@param nScale Scale of the unit distance between terrain vertices 
		@returns A bounding-box representation of the one specified in the destination coordinate system */
		inline static AxisAlignedBox toSpace (const OverhangCoordinateSpace encsFrom, const OverhangTerrainAlignment enalAlignment, const OverhangCoordinateSpace encsTo, const AxisAlignedBox & bboxIn, const Real nScale = 1.0)
		{
			AxisAlignedBox bboxRet = bboxIn;

			transformSpace(encsFrom, enalAlignment, encsTo, bboxRet, nScale);
			return bboxRet;
		}
		/** Returns the transformed bounding-box of the one specified using the main top-level configuration options
		@param encsFrom Source coordinate system
		@param encsTo Destination coordinate system
		@param bboxIn The source bounding box
		@returns A bounding-box representation of the one specified in the destination coordinate system */
		inline AxisAlignedBox toSpace (const OverhangCoordinateSpace encsFrom, const OverhangCoordinateSpace encsTo, const AxisAlignedBox & bboxIn) const
		{
			AxisAlignedBox bboxRet = bboxIn;

			transformSpace(encsFrom, encsTo, bboxRet);
			return bboxRet;
		}
		/** Transforms the bounding-box using the main top-level configuration options
		@param encsFrom Source coordinate system
		@param encsTo Destination coordinate system
		@param bboxIn The source bounding box that will also be transformed */
		inline void transformSpace(const OverhangCoordinateSpace encsFrom, const OverhangCoordinateSpace encsTo, AxisAlignedBox & bbox) const
			{ transformSpace(encsFrom, options.alignment, encsTo, bbox, options.cellScale); }

		/** Returns a transformed set of coordinates in the specified space for the specified Y-level
		@param yl The Y-level to return the transform
		@param encsTo The destination coordinate space
		@returns A vector whose y-component reflects the Y-level and the x and z components set to zero */
		inline Vector3 toSpace( const YLevel yl, const OverhangCoordinateSpace encsTo ) const
		{
			return toSpace(OCS_World, encsTo, (Vector3::ZERO + yl) * options.getTileWorldSize());
		}

		/** Returns the transformed ray in the specified space
		@param encsFrom Source coordinate system
		@param enalAlignment The world alignment of terrain in the scene
		@param encsTo Destination coordinate system
		@param rayIn The source ray
		@param nScale Scale of the unit distance between terrain vertices
		*/
		static inline
		void transformSpace (const OverhangCoordinateSpace encsFrom, const OverhangTerrainAlignment enalAlignment, const OverhangCoordinateSpace encsTo, Ray & rayIn, const Real nScale = 1.0)
		{
			Vector3
				origin = rayIn.getOrigin(),
				direction = rayIn.getDirection();

			transformSpace(encsFrom, enalAlignment, encsTo, origin, nScale);
			transformSpace(encsFrom, enalAlignment, encsTo, direction, nScale);
			direction.normalise();

			rayIn.setOrigin(origin);
			rayIn.setDirection(direction);
		}

		/** Returns the transformed ray of the one specified using the main top-level configuration options
		@param encsFrom Source coordinate system
		@param encsTo Destination coordinate system
		@param bboxIn The source ray
		@returns A ray representation of the one specified in the destination coordinate system */
		inline Ray toSpace(const OverhangCoordinateSpace encsFrom, const OverhangCoordinateSpace encsTo, const Ray & ray) const
		{
			Ray vRet = ray;
			transformSpace(encsFrom, options.alignment, encsTo, vRet, options.cellScale);
			return vRet;
		}

	private:
		/// Pointer to the scene manager used currently
		OverhangTerrainSceneManager * _pScnMgr;
		/// Database of transformation algorithms
		static AlgorithmIndex _algorithms;
	};
}

#endif