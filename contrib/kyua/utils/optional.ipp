// Copyright 2010 The Kyua Authors.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#if !defined(UTILS_OPTIONAL_IPP)
#define UTILS_OPTIONAL_IPP

#include <cstddef>

#include "utils/defs.hpp"
#include "utils/optional.hpp"
#include "utils/sanity.hpp"


/// Initializes an optional object to the none value.
template< class T >
utils::optional< T >::optional(void) :
    _data(NULL)
{
}


/// Explicitly initializes an optional object to the none value.
template< class T >
utils::optional< T >::optional(utils::detail::none_t /* none */) :
    _data(NULL)
{
}


/// Initializes an optional object to a non-none value.
///
/// \param data The initial value for the object.
template< class T >
utils::optional< T >::optional(const T& data) :
    _data(new T(data))
{
}


/// Copy constructor.
///
/// \param other The optional object to copy from.
template< class T >
utils::optional< T >::optional(const optional< T >& other) :
    _data(other._data == NULL ? NULL : new T(*(other._data)))
{
}


/// Destructor.
template< class T >
utils::optional< T >::~optional(void)
{
    if (_data != NULL)
        delete _data;
    _data = NULL;  // Prevent accidental reuse.
}


/// Explicitly assigns an optional object to the none value.
///
/// \return A reference to this.
template< class T >
utils::optional< T >&
utils::optional< T >::operator=(utils::detail::none_t /* none */)
{
    if (_data != NULL)
        delete _data;
    _data = NULL;
    return *this;
}


/// Assigns a new value to the optional object.
///
/// \param data The initial value for the object.
///
/// \return A reference to this.
template< class T >
utils::optional< T >&
utils::optional< T >::operator=(const T& data)
{
    T* new_data = new T(data);
    if (_data != NULL)
        delete _data;
    _data = new_data;
    return *this;
}


/// Copies an optional value.
///
/// \param other The optional object to copy from.
///
/// \return A reference to this.
template< class T >
utils::optional< T >&
utils::optional< T >::operator=(const optional< T >& other)
{
    T* new_data = other._data == NULL ? NULL : new T(*(other._data));
    if (_data != NULL)
        delete _data;
    _data = new_data;
    return *this;
}


/// Equality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are equal; false otherwise.
template< class T >
bool
utils::optional< T >::operator==(const optional< T >& other) const
{
    if (_data == NULL && other._data == NULL) {
        return true;
    } else if (_data == NULL || other._data == NULL) {
        return false;
    } else {
        INV(_data != NULL && other._data != NULL);
        return *_data == *other._data;
    }
}


/// Inequality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are different; false otherwise.
template< class T >
bool
utils::optional< T >::operator!=(const optional< T >& other) const
{
    return !(*this == other);
}


/// Gets the value hold by the optional object.
///
/// \pre The optional object must not be none.
///
/// \return A reference to the data.
template< class T >
const T&
utils::optional< T >::get(void) const
{
    PRE(_data != NULL);
    return *_data;
}


/// Gets the value of this object with a default fallback.
///
/// \param default_value The value to return if this object holds no value.
///
/// \return A reference to the data in the optional object, or the reference
/// passed in as a parameter.
template< class T >
const T&
utils::optional< T >::get_default(const T& default_value) const
{
    if (_data != NULL)
        return *_data;
    else
        return default_value;
}


/// Tests whether the optional object contains data or not.
///
/// \return True if the object is not none; false otherwise.
template< class T >
utils::optional< T >::operator bool(void) const
{
    return _data != NULL;
}


/// Tests whether the optional object contains data or not.
///
/// \return True if the object is not none; false otherwise.
template< class T >
T&
utils::optional< T >::get(void)
{
    PRE(_data != NULL);
    return *_data;
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
template< class T >
std::ostream& utils::operator<<(std::ostream& output,
                                const optional< T >& object)
{
    if (!object) {
        output << "none";
    } else {
        output << object.get();
    }
    return output;
}


/// Helper function to instantiate optional objects.
///
/// \param value The value for the optional object.  Shouldn't be none, as
///     optional objects can be constructed from none right away.
///
/// \return A new optional object.
template< class T >
utils::optional< T >
utils::make_optional(const T& value)
{
    return optional< T >(value);
}


#endif  // !defined(UTILS_OPTIONAL_IPP)
