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

/// \file utils/defs.hpp
///
/// Definitions for compiler and system features autodetected at configuration
/// time.

#if !defined(UTILS_DEFS_HPP)
#define UTILS_DEFS_HPP


/// Attribute to mark a function as non-returning.
#define UTILS_NORETURN __attribute__((noreturn))


/// Attribute to mark a function as pure.
#define UTILS_PURE __attribute__((__pure__))


/// Attribute to mark an entity as unused.
#define UTILS_UNUSED __attribute__((__unused__))


/// Unconstifies a pointer.
///
/// \param type The target type of the conversion.
/// \param ptr The pointer to be unconstified.
#define UTILS_UNCONST(type, ptr) ((type*)(unsigned long)(const void*)(ptr))


/// Macro to mark a parameter as unused.
///
/// This macro has to be called on the name of a parameter during the
/// definition (not declaration) of a function.  When doing so, it declares
/// the parameter as unused to silence compiler warnings and also renames
/// the parameter by prefixing "unused_" to it.  This is to ensure that the
/// developer remembers to remove the call to this macro from the parameter
/// when he actually starts using it.
///
/// \param name The name of the function parameter to mark as unused.
#define UTILS_UNUSED_PARAM(name) unused_ ## name UTILS_UNUSED


#endif  // !defined(UTILS_DEFS_HPP)
