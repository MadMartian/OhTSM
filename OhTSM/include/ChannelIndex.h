#ifndef __OVERHANGTERRAINCHANNELIDENT_H__
#define __OVERHANGTERRAINCHANNELIDENT_H__

#include "Util.h"

#include "OverhangTerrainPrerequisites.h"

#include <OgrePlatform.h>
#include <OgreStreamSerialiser.h>

#include <vector>

namespace Ogre
{
	namespace Channel
	{
		/** Wrapper for a channel identifier */
		static const
		class Ident
		{
		private:
			// The ordinal value of the identifier
			uint16 _ordinal;

		public:
			// Defaults to the special "invalid" channel identifier
			inline
			Ident()
				: _ordinal((uint16)~0) {}
			// Initialize a channel identifier from the specified ordinal
			inline
			Ident(const uint16 nHandle)
				: _ordinal(nHandle) {}

			// Convert this to its ordinal representation
			inline
			operator uint16 () const { return _ordinal; }

			friend StreamSerialiser & operator << (StreamSerialiser & outs, const Ident channel);
			friend StreamSerialiser & operator >> (StreamSerialiser & ins, Ident & channel);
		} Ident_INVALID;

		// Serialize a channel identifier to the stream
		StreamSerialiser & operator << ( StreamSerialiser & outs, const Ident channel );
		// Deserialize a channel identifier from the stream
		StreamSerialiser & operator >> ( StreamSerialiser & ins, Ident & channel );

		/** Defines a set of channels */
		class Descriptor
		{
		public:
			// The number of channels in the set
			uint16 count;

			// Initialize a descriptor specifying the number of channels
			Descriptor(const uint16 nCount) : count(nCount) {}

			/** Iterator pattern for walking through all the channel identifiers supported by this descriptor */
			class iterator
			{
			private:
				// Total number of channels
				const uint16 _num;
				// Ordinal of current channel
				Channel::Ident _current;

			public:
				/** Initialize an iterator
				@param nCurrent The current channel identifier to initialize this iterator at
				@param nNumChannels The total number of channels to iterate through
				*/
				iterator (const uint16 nCurrent, const uint16 nNumChannels)
					: _num(nNumChannels), _current(nCurrent) {}

				// Retrieve the current channel identifier
				inline const Channel::Ident & operator * () const { return _current; }
				// Retrieve the current channel identifier
				inline const Channel::Ident * operator -> () const { return &_current; }

				// Advance to the next channel identifier
				iterator & operator ++ ()
				{
					_current = Ident(_current+1);
					return *this;
				}
				// Advance to the next channel identifier
				iterator operator ++ (int)
				{
					iterator old = *this;
					_current = Ident(_current+1);
					return old;
				}

				inline bool operator == (const iterator & other) const { return _current == other._current; }
				inline bool operator != (const iterator & other) const { return _current != other._current; }
			};

			// Retrieve an iterator at the beginning of the sequence
			iterator begin() const { return iterator(0, count); }
			// Retrieve an iterator past the end of the sequence
			iterator end() const { return iterator(count, count); }
		};

		// Type factory that calls a simple no-arg constructor
		template< typename T >
		struct StandardFactory
		{
			T * instantiate(const Channel::Ident channel) { return new T(); }
		};

		// Type factory that always returns NULL
		template< typename T >
		struct FauxFactory
		{
			T * instantiate (const Channel::Ident channel) { return NULL; }
		};

		/** Create a map-like index keyed by channel identifier for the specified type
			using the specified type factory */
		template< typename T, typename Loader = StandardFactory< T > >
		class Index
		{
		private:
			typedef std::vector< T * > VectorType;

			mutable Loader _loader;
			mutable VectorType _vec;

		protected:
			/** Initialize this index with type instances generated by the specified initializer lambda
			@remarks The lambda returns a pointer to an allocated object that will be managed by this class
			@param fnInitializer A lambda function that returns a pointer to type */
			template< typename Fn >
			void init(Fn fnInitializer)
			{
				size_t c = 0;
				for (vector_iterator i = _vec.begin(); i != _vec.end(); ++i)
					*i = fnInitializer(Ident(c++));
			}

		public:
			const Descriptor descriptor;

			/** Exception thrown when an attempt to reference a channel that does not exist */
			class NoSuchElementEx : public std::exception
			{
			public:
				NoSuchElementEx(const char * szMessage) : std::exception(szMessage) {}
			};

			/** Defines a channel identifier and index value together */
			template< typename TT >
			class pair
			{
			public:
				// The channel identifier
				Ident channel;
				// The type value
				TT * value;

				pair () {}
				pair (const Ident ident, TT * value)
					: channel(ident), value(value) {}
				pair (const pair< T > & copy)
					: channel(copy.channel), value(copy.value) {}
			};

			/** Initialize from the specified channel descriptor
			@param descriptor The descriptor describing how many channels to initialize at base zero */
			Index(const Descriptor & descriptor)
				: descriptor(descriptor), _vec(descriptor.count), _loader(Loader()) {}
			/** Initialize from the specified channel descriptor using the specified loader instance
			@param descriptor The descriptor describing how many channels to initialize at base zero
			@param loader The factory used to create new instances of type */
			Index(const Descriptor & descriptor, const Loader & loader)
				: descriptor(descriptor), _vec(descriptor.count), _loader(loader) {}

			/** Initialize from the specified channel descriptor and the lambda initializer
			@remarks Loops through all channels for each calling the lambda function.  The lambda must return
			an allocated pointer to a new object and that object will be managed by this class
			@param descriptor The descriptor describing how many channels to initialize at base zero
			@param fnInitializer A lambda function that returns a pointer to type */
			template< typename Fn >
			Index(const Descriptor & descriptor, Fn fnInitializer)
				: descriptor(descriptor), _vec(descriptor.count), _loader(Loader())
			{
				init(fnInitializer);
			}

			/** Initialize from the specified channel descriptor and the lambda initializer, the loader will be used later
			@remarks Loops through all channels for each calling the lambda function.  The lambda must return
			an allocated pointer to a new object and that object will be managed by this class
			@param descriptor The descriptor describing how many channels to initialize at base zero
			@param loader The factory used to create new instances of type
			@param fnInitializer A lambda function that returns a pointer to type */
			template< typename Fn >
			Index(const Descriptor & descriptor, const Loader & loader, Fn fnInitializer)
				: descriptor(descriptor), _vec(descriptor.count), _loader(loader)
			{
				init(fnInitializer);
			}

			typedef typename VectorType::iterator vector_iterator;
			typedef typename VectorType::const_iterator vector_const_iterator;

			/** Iterator pattern for all allocated pairs of channel identifier and the associated type */
			template< typename Iter, typename TT >
			class template_Iterator : public std::iterator <std::input_iterator_tag, pair< TT > >
			{
			private:
				// Internal vector iterators for the current, beginning, and end respectively
				Iter _i, _0, _N;

				// Current channel identifier ordinal corresponding to current vector iterator
				uint16 _c;

				enum CRState
				{
					CRS_Default = 1
				};

				OHT_CR_CONTEXT;

			protected:
				// Current pair in the interation, this is the value returned by the iterator
				pair < TT > _current;

				typedef Iter Iter;

				// Performs a single iteration
				inline
				void process()
				{
					OHT_CR_START();

					for (_i = _0, _c = 0; _i != _N; ++_i, ++_c)
					{
						if (*_i != NULL)
						{
							_current = value_type(Ident(_c), *_i);
							OHT_CR_RETURN_VOID(CRS_Default);
						}
					}

					_current = value_type(Ident(), NULL);

					OHT_CR_END();
				}

			public:
				/**
				@param begin Vector iterator head of the iteration
				@param end Vector iterator end of the iteration
				*/
				template_Iterator (Iter begin, Iter end)
					: _0 (begin), _N(end), _c(0)
				{
					OHT_CR_INIT();
					process();
				}
				template_Iterator (Iter && move)
					: _i(move._i), _0(move._0), _N(move._N), _c(move._c), _current(move._current), OHT_CR_COPY(move) {}
				template_Iterator (const Iter & copy)
					: _i(copy._i), _0(copy._0), _N(copy._N), _c(copy._c), _current(copy._current), OHT_CR_COPY(copy) {}
				template_Iterator (const template_Iterator< vector_iterator, T > & copy)
					: _i(copy._i), _0(copy._0), _N(copy._N), _c(copy._c), _current(copy._current), OHT_CR_COPY(copy) {}

				inline const pair< TT > & operator * () const { return _current; }
				inline const pair< TT > * operator -> () const { return &_current; }

				inline bool operator == (const template_Iterator & other) const { return _i == other._i; }
				inline bool operator != (const template_Iterator & other) const { return _i != other._i; }

				template_Iterator & operator ++ ()
				{
					process();
					return *this;
				}
				template_Iterator operator ++ (int)
				{
					template_Iterator old = *this;
					process();
					return old;
				}

			protected:
				friend template_Iterator< vector_const_iterator, const T >;
			};

			typedef template_Iterator< vector_const_iterator, const T > const_iterator;
			class iterator : public template_Iterator< vector_iterator, T >
			{
			public:
				iterator (Iter begin, Iter end) : template_Iterator(begin, end) {}
				iterator (Iter && move) : template_Iterator(static_cast< Iter && > (move)) {}
				iterator (const iterator & copy) : template_Iterator(copy) {}

				inline pair< T > & operator * () { return _current; }
				inline pair< T > * operator -> () { return &_current; }

				inline bool operator == (const iterator & other) const { return template_Iterator::operator == (other); }
				inline bool operator != (const iterator & other) const { return template_Iterator::operator != (other); }

				iterator & operator ++ ()
				{
					process();
					return *this;
				}
				iterator operator ++ (int)
				{
					template_Iterator old = *this;
					process();
					return old;
				}

				friend const_iterator;
			};

			// Acquire an iterator at the beginning of the channel index
			iterator begin () { return iterator(_vec.begin(), _vec.end()); }
			// Acquire an iterator at the end of the channel index
			iterator end () { return iterator(_vec.end(), _vec.end()); }

			// Acquire an iterator at the beginning of the channel index
			const_iterator begin () const { return const_iterator(_vec.begin(), _vec.end()); }
			// Acquire an iterator at the end of the channel index
			const_iterator end () const { return const_iterator(_vec.end(), _vec.end()); }

			/** Find a value in the index at the specified channel identifier
			@param ident The channel identifier of the value to look-up
			@returns An iterator to the sought element in the index or an iterator matching the end if not found */
			iterator find(const Ident ident)
			{
				if (_vec[ident] != NULL)
					return iterator(_vec.begin() + ident, _vec.end());
				else
					return end();
			}
			/** Find a value in the index at the specified channel identifier
			@param ident The channel identifier of the value to look-up
			@returns An iterator to the sought element in the index or an iterator matching the end if not found */
			const_iterator find(const Ident ident) const
			{
				if (_vec[ident] != NULL)
					return const_iterator(_vec.begin() + ident, _vec.end());
				else
					return end();
			}

			/** Remove an element from the index at the specified iterator
			@param i The iterator into this index specifying the element to remove
			@returns A new iterator based on the specified iterator */
			iterator erase(iterator & i)
			{
				delete i->value;
				_vec[i->channel] = NULL;
				return iterator(i);
			}

			/** Lazy retrieval of the element in the index associated with the specified channel
				identifier.
			@remarks This method always returns a value (depending on the type factory), if
				there is no element associated with the specified channel identifier then a new
				element is created using the type factory configured.
			@throws NoSuchElementEx If the specified identifier is outside the allowed range according
				to the descriptor
			@param ident The channel identifier to retrieve an associated element of from this index
			@returns The element associated at the specified identifier in the index */
			T & operator [] (const Ident ident)
			{
				if (ident >= _vec.size())
					throw NoSuchElementEx("No such element at specified index");

				VectorType::reference r = _vec[ident];
				if (r == NULL)
					r = _loader.instantiate(ident);

				return *r;
			}

			/** Lazy retrieval of the element in the index associated with the specified channel
				identifier.
			@remarks This method always returns a value (depending on the type factory), if
				there is no element associated with the specified channel identifier then a new
				element is created using the type factory configured.
			@throws NoSuchElementEx If the specified identifier is outside the allowed range according
				to the descriptor
			@param ident The channel identifier to retrieve an associated element of from this index
			@returns The element associated at the specified identifier in the index */
			const T & operator [] (const Ident ident) const
			{
				if (ident >= _vec.size())
					throw NoSuchElementEx("No such element at specified index");

				VectorType::reference r = _vec[ident];
				if (r == NULL)
					r = _loader.instantiate(ident);

				return *r;
			}

			Index & operator = (const Index & other)
			{
				for (vector_iterator i = _vec.begin(); i != _vec.end(); ++i)
					delete *i;

				_vec.resize(other._vec.size());

				size_t c = 0;
				for (vector_const_iterator i = other._vec.begin(); i != other._vec.end(); ++i)
					_vec[c++] = new T(**i);

				return *this;
			}
		};
	}
}

#endif
