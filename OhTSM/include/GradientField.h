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

#ifndef __OVERHANGTERRAINGRADIENTFIELD_H__
#define __OVERHANGTERRAINGRADIENTFIELD_H__

#include "OverhangTerrainPrerequisites.h"

#include "CubeDataRegionDescriptor.h"

#include "Util.h"

namespace Ogre 
{
	namespace Voxel
	{
		/** Provides uniform access to the gradient field distributed by vector instead of by its native distribution by vector component */
		class _OverhangTerrainPluginExport GradientField
		{
		private:
			/// Number of gradient elements in the field and element size of each channel
			const size_t _count;

			/// Pointers to the gradient field vector x,y, and z components respectively
			signed char 
				* const _dx, 
				* const _dy, 
				* const _dz;

		public:
			typedef signed short PublicPrimitive;

			/**
			@remarks The dx, dy, and dz are pointers to pre-allocated, pre-initialized, and pre-populated blocks of memory containing
				meaningful vector component data
			@param dgtmpl The entire scene's voxel cube descriptor singleton
			@param dx The x channel of the gradient field
			@param dy The y channel of the gradient field
			@param dz The z channel of the gradient field
			*/
			GradientField(const CubeDataRegionDescriptor & cubemeta, signed char * dx, signed char * dy, signed char * dz);

			/// Exposes a single component of a single vector as directed by field offset
			template< typename CHAR >
			class template_ComponentReference
			{
			private:
				friend class GradientField;

			protected:
				/// Pointer to the vector component of a single vector element
				CHAR * const _component;

				/**
				@param pComponent Pointer to the vector component of a single vector element
				*/
				inline
					template_ComponentReference(CHAR * pComponent)
					: _component(pComponent)
				{}

			public:
				/// Exposes the vector component with read only access
				inline
					operator const PublicPrimitive () const
				{
					return PublicPrimitive(*_component) << 1;
				}
			};

			typedef template_ComponentReference< const signed char > const_ComponentReference;

			/// Exposes read/write access to a single component of a single vector as directed by field offset
			class ComponentReference : public template_ComponentReference< signed char >
			{
			protected:
				/**
				@param pComponent Pointer to the vector component of a single vector element
				*/
				inline
					ComponentReference(signed char * pComponent)
					: template_ComponentReference(pComponent) {}

				friend class GradientField;

			public:
				/// Allows assignment to the component represented by this
				inline
					ComponentReference & operator = (const PublicPrimitive & f)
				{
					*_component = signed char (f >> 1);
					return *this;
				}
			};

			/// Convenience class for exposing accessors to each vector component channel
			class ComponentAccessor
			{
			private:
				/// The channel data to expose access to
				signed char * const _channel;

			protected:
				ComponentAccessor();

				/// Dependency injection, sets a pointer to the channel to expose
				void inject (signed char * pChannel);

				friend class GradientField;

			public:
				/// Expose read-only access to the component at the specified index in the field
				inline
					const_ComponentReference operator [] (const size_t index) const { return const_ComponentReference(&_channel[index]); }
				/// Expose read/write access to the component at the specified index in the field
				inline
					ComponentReference operator [] (const size_t index) { return ComponentReference(&_channel[index]); }

			} dx, dy, dz;	/// Channel accessors for all 3 vector components in the field

			typedef FixVector3< 8, int > VectorType;

			/// Recomposes the field to distribution by vector instead of distribution by component channel providing access to a single vector element
			template< typename CHAR >
			class template_Reference
			{
			private:
				friend class GradientField;

			protected:
				/// Pointers to the vector component values that will be recomposed into a single vector element
				CHAR
					* const _dx, 
					* const _dy, 
					* const _dz;

				/**
				@remarks Exposes access to the vector component values using the following pointers.  The result is recomposed into a VectorType.
				@param dx Pointer to the x component value to expose
				@param dy Pointer to the y component value to expose
				@param dz Pointer to the z component value to expose
				*/
				inline
					template_Reference(CHAR * dx, CHAR * dy, CHAR * dz)
					: _dx(dx), _dy(dy), _dz(dz)
				{}

			public:
				/// Recomposes the vector component values into a single read-only vector element
				inline
					operator VectorType () const
				{
					return 
						VectorType
						(
							int(*_dx) << 1,
							int(*_dy) << 1,
							int(*_dz) << 1
						);
				}
			};

			typedef template_Reference< const signed char > const_Reference;

			/// Recomposes the field to distribution by vector instead of distribution by component channel providing read/write access to a single vector element
			class Reference : public template_Reference< signed char >
			{
			protected:
				/**
				@remarks Exposes access to the vector component values using the following pointers.  The result is recomposed into a VectorType.
				@param dx Pointer to the x component value to expose
				@param dy Pointer to the y component value to expose
				@param dz Pointer to the z component value to expose
				*/
				inline
					Reference(signed char * dx, signed char * dy, signed char * dz)
					: template_Reference(dx, dy, dz) {}

				friend class GradientField;

			public:
				/// Allows assignment to the vector component values by extrapolating them from a vector type
				inline
					Reference & operator = (const VectorType & v)
				{
					*_dx = signed char (int(v.x * 128));
					*_dy = signed char (int(v.y * 128));
					*_dz = signed char (int(v.z * 128));
					return *this;
				}
			};

			/// Retrieve a const recomposed vector-type accessor to the component values at the specified index into the field
			inline
				const_Reference operator [] (const size_t index) const
			{
				return const_Reference(&_dx[index], &_dy[index], &_dz[index]);
			}
			/// Retrieve a mutable recomposed vector-type accessor to the component values at the specified index into the field
			inline
				Reference operator [] (const size_t index)
			{
				return Reference(&_dx[index], &_dy[index], &_dz[index]);
			}

			/// Resets all channels to zeros
			void clear ();
		};
	}
}

#endif