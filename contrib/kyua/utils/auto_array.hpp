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

/// \file utils/auto_array.hpp
/// Provides the utils::auto_array class.
///
/// The class is provided as a separate module on its own to minimize
/// header-inclusion side-effects.

#if !defined(UTILS_AUTO_ARRAY_HPP)
#define UTILS_AUTO_ARRAY_HPP

#include "utils/auto_array_fwd.hpp"

#include <cstddef>

namespace utils {


namespace detail {


/// Wrapper class to provide reference semantics for utils::auto_array.
///
/// This class is internally used, for example, to allow returning a
/// utils::auto_array from a function.
template< class T >
class auto_array_ref {
    /// Internal pointer to the dynamically-allocated array.
    T* _ptr;

    template< class > friend class utils::auto_array;

public:
    explicit auto_array_ref(T*);
};


}  // namespace detail


/// A simple smart pointer for arrays providing strict ownership semantics.
///
/// This class is the counterpart of std::unique_ptr for arrays.  The semantics of
/// the API of this class are the same as those of std::unique_ptr.
///
/// The wrapped pointer must be NULL or must have been allocated using operator
/// new[].
template< class T >
class auto_array {
    /// Internal pointer to the dynamically-allocated array.
    T* _ptr;

public:
    auto_array(T* = NULL) throw();
    auto_array(auto_array< T >&) throw();
    auto_array(detail::auto_array_ref< T >) throw();
    ~auto_array(void) throw();

    T* get(void) throw();
    const T* get(void) const throw();

    T* release(void) throw();
    void reset(T* = NULL) throw();

    auto_array< T >& operator=(auto_array< T >&) throw();
    auto_array< T >& operator=(detail::auto_array_ref< T >) throw();
    T& operator[](int) throw();
    const T& operator[](int) const throw();
    operator detail::auto_array_ref< T >(void) throw();
};


}  // namespace utils


#endif  // !defined(UTILS_AUTO_ARRAY_HPP)
