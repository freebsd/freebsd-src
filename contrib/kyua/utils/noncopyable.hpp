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

/// \file utils/noncopyable.hpp
/// Provides the utils::noncopyable class.
///
/// The class is provided as a separate module on its own to minimize
/// header-inclusion side-effects.

#if !defined(UTILS_NONCOPYABLE_HPP)
#define UTILS_NONCOPYABLE_HPP


namespace utils {


/// Forbids copying a class at compile-time.
///
/// Inheriting from this class delivers a private copy constructor and an
/// assignment operator that effectively forbid copying the class during
/// compilation.
///
/// Always use private inheritance.
class noncopyable {
    /// Data placeholder.
    ///
    /// The class cannot be empty; otherwise we get ABI-stability warnings
    /// during the build, which will break it due to strict checking.
    int _noncopyable_dummy;

    /// Private copy constructor to deny copying of subclasses.
    noncopyable(const noncopyable&);

    /// Private assignment constructor to deny copying of subclasses.
    ///
    /// \return A reference to the object.
    noncopyable& operator=(const noncopyable&);

protected:
    // Explicitly needed to provide some non-private functions.  Otherwise
    // we also get some warnings during the build.
    noncopyable(void) {}
    ~noncopyable(void) {}
};


}  // namespace utils


#endif  // !defined(UTILS_NONCOPYABLE_HPP)
