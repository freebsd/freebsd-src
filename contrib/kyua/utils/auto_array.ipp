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

#if !defined(UTILS_AUTO_ARRAY_IPP)
#define UTILS_AUTO_ARRAY_IPP

#include "utils/auto_array.hpp"

namespace utils {


namespace detail {


/// Constructs a new auto_array_ref from a pointer.
///
/// \param ptr The pointer to wrap.
template< class T > inline
auto_array_ref< T >::auto_array_ref(T* ptr) :
    _ptr(ptr)
{
}


}  // namespace detail


/// Constructs a new auto_array from a given pointer.
///
/// This grabs ownership of the pointer unless it is NULL.
///
/// \param ptr The pointer to wrap.  If not NULL, the memory pointed to must
/// have been allocated with operator new[].
template< class T > inline
auto_array< T >::auto_array(T* ptr) throw() :
    _ptr(ptr)
{
}


/// Constructs a copy of an auto_array.
///
/// \param ptr The pointer to copy from.  This pointer is invalidated and the
/// new copy grabs ownership of the object pointed to.
template< class T > inline
auto_array< T >::auto_array(auto_array< T >& ptr) throw() :
    _ptr(ptr.release())
{
}


/// Constructs a new auto_array form a reference.
///
/// Internal function used to construct a new auto_array from an object
/// returned, for example, from a function.
///
/// \param ref The reference.
template< class T > inline
auto_array< T >::auto_array(detail::auto_array_ref< T > ref) throw() :
    _ptr(ref._ptr)
{
}


/// Destructor for auto_array objects.
template< class T > inline
auto_array< T >::~auto_array(void) throw()
{
    if (_ptr != NULL)
        delete [] _ptr;
}


/// Gets the value of the wrapped pointer without releasing ownership.
///
/// \return The raw mutable pointer.
template< class T > inline
T*
auto_array< T >::get(void) throw()
{
    return _ptr;
}


/// Gets the value of the wrapped pointer without releasing ownership.
///
/// \return The raw immutable pointer.
template< class T > inline
const T*
auto_array< T >::get(void) const throw()
{
    return _ptr;
}


/// Gets the value of the wrapped pointer and releases ownership.
///
/// \return The raw mutable pointer.
template< class T > inline
T*
auto_array< T >::release(void) throw()
{
    T* ptr = _ptr;
    _ptr = NULL;
    return ptr;
}


/// Changes the value of the wrapped pointer.
///
/// If the auto_array was pointing to an array, such array is released and the
/// wrapped pointer is replaced with the new pointer provided.
///
/// \param ptr The pointer to use as a replacement; may be NULL.
template< class T > inline
void
auto_array< T >::reset(T* ptr) throw()
{
    if (_ptr != NULL)
        delete [] _ptr;
    _ptr = ptr;
}


/// Assignment operator.
///
/// \param ptr The object to copy from.  This is invalidated after the copy.
/// \return A reference to the auto_array object itself.
template< class T > inline
auto_array< T >&
auto_array< T >::operator=(auto_array< T >& ptr) throw()
{
    reset(ptr.release());
    return *this;
}


/// Internal assignment operator for function returns.
///
/// \param ref The reference object to copy from.
/// \return A reference to the auto_array object itself.
template< class T > inline
auto_array< T >&
auto_array< T >::operator=(detail::auto_array_ref< T > ref) throw()
{
    if (_ptr != ref._ptr) {
        delete [] _ptr;
        _ptr = ref._ptr;
    }
    return *this;
}


/// Subscript operator to access the array by position.
///
/// This does not perform any bounds checking, in particular because auto_array
/// does not know the size of the arrays pointed to by it.
///
/// \param pos The position to access, indexed from zero.
///
/// \return A mutable reference to the element at the specified position.
template< class T > inline
T&
auto_array< T >::operator[](int pos) throw()
{
    return _ptr[pos];
}


/// Subscript operator to access the array by position.
///
/// This does not perform any bounds checking, in particular because auto_array
/// does not know the size of the arrays pointed to by it.
///
/// \param pos The position to access, indexed from zero.
///
/// \return An immutable reference to the element at the specified position.
template< class T > inline
const T&
auto_array< T >::operator[](int pos) const throw()
{
    return _ptr[pos];
}


/// Internal conversion to a reference wrapper.
///
/// This is used internally to support returning auto_array objects from
/// functions.  The auto_array is invalidated when used.
///
/// \return A new detail::auto_array_ref object holding the pointer.
template< class T > inline
auto_array< T >::operator detail::auto_array_ref< T >(void) throw()
{
    return detail::auto_array_ref< T >(release());
}


}  // namespace utils


#endif  // !defined(UTILS_AUTO_ARRAY_IPP)
