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

/// \file utils/sanity.hpp
///
/// Set of macros that replace the standard assert macro with more semantical
/// expressivity and meaningful diagnostics.  Code should never use assert
/// directly.
///
/// In general, the checks performed by the macros in this code are only
/// executed if the code is built with debugging support (that is, if the NDEBUG
/// macro is NOT defined).

#if !defined(UTILS_SANITY_HPP)
#define UTILS_SANITY_HPP

#include "utils/sanity_fwd.hpp"

#include <cstddef>
#include <string>

#include "utils/defs.hpp"

namespace utils {


void sanity_failure(const assert_type, const char*, const size_t,
                    const std::string&) UTILS_NORETURN;


void install_crash_handlers(const std::string&);


}  // namespace utils


/// \def _UTILS_ASSERT(type, expr, message)
/// \brief Performs an assertion check.
///
/// This macro is internal and should not be used directly.
///
/// Ensures that the given expression expr is true and, if not, terminates
/// execution by calling utils::sanity_failure().  The check is only performed
/// in debug builds.
///
/// \param type The assertion type as defined by assert_type.
/// \param expr A boolean expression.
/// \param message A string describing the nature of the error.
#if !defined(NDEBUG)
#   define _UTILS_ASSERT(type, expr, message) \
    do { \
        if (!(expr)) \
            utils::sanity_failure(type, __FILE__, __LINE__, message); \
    } while (0)
#else  // defined(NDEBUG)
#   define _UTILS_ASSERT(type, expr, message) do {} while (0)
#endif  // !defined(NDEBUG)


/// Ensures that an invariant holds.
///
/// If the invariant does not hold, execution is immediately terminated.  The
/// check is only performed in debug builds.
///
/// The error message printed by this macro is a textual representation of the
/// boolean condition.  If you want to provide a custom error message, use
/// INV_MSG instead.
///
/// \param expr A boolean expression describing the invariant.
#define INV(expr) _UTILS_ASSERT(utils::invariant, expr, #expr)


/// Ensures that an invariant holds using a custom error message.
///
/// If the invariant does not hold, execution is immediately terminated.  The
/// check is only performed in debug builds.
///
/// \param expr A boolean expression describing the invariant.
/// \param msg The error message to print if the condition is false.
#define INV_MSG(expr, msg) _UTILS_ASSERT(utils::invariant, expr, msg)


/// Ensures that a precondition holds.
///
/// If the precondition does not hold, execution is immediately terminated.  The
/// check is only performed in debug builds.
///
/// The error message printed by this macro is a textual representation of the
/// boolean condition.  If you want to provide a custom error message, use
/// PRE_MSG instead.
///
/// \param expr A boolean expression describing the precondition.
#define PRE(expr) _UTILS_ASSERT(utils::precondition, expr, #expr)


/// Ensures that a precondition holds using a custom error message.
///
/// If the precondition does not hold, execution is immediately terminated.  The
/// check is only performed in debug builds.
///
/// \param expr A boolean expression describing the precondition.
/// \param msg The error message to print if the condition is false.
#define PRE_MSG(expr, msg) _UTILS_ASSERT(utils::precondition, expr, msg)


/// Ensures that an postcondition holds.
///
/// If the postcondition does not hold, execution is immediately terminated.
/// The check is only performed in debug builds.
///
/// The error message printed by this macro is a textual representation of the
/// boolean condition.  If you want to provide a custom error message, use
/// POST_MSG instead.
///
/// \param expr A boolean expression describing the postcondition.
#define POST(expr) _UTILS_ASSERT(utils::postcondition, expr, #expr)


/// Ensures that a postcondition holds using a custom error message.
///
/// If the postcondition does not hold, execution is immediately terminated.
/// The check is only performed in debug builds.
///
/// \param expr A boolean expression describing the postcondition.
/// \param msg The error message to print if the condition is false.
#define POST_MSG(expr, msg) _UTILS_ASSERT(utils::postcondition, expr, msg)


/// Ensures that a code path is not reached.
///
/// If the code path in which this macro is located is reached, execution is
/// immediately terminated.  Given that such a condition is critical for the
/// execution of the program (and to prevent build failures due to some code
/// paths not initializing variables, for example), this condition is fatal both
/// in debug and production builds.
///
/// The error message printed by this macro is a textual representation of the
/// boolean condition.  If you want to provide a custom error message, use
/// POST_MSG instead.
#define UNREACHABLE UNREACHABLE_MSG("")


/// Ensures that a code path is not reached using a custom error message.
///
/// If the code path in which this macro is located is reached, execution is
/// immediately terminated.  Given that such a condition is critical for the
/// execution of the program (and to prevent build failures due to some code
/// paths not initializing variables, for example), this condition is fatal both
/// in debug and production builds.
///
/// \param msg The error message to print if the condition is false.
#define UNREACHABLE_MSG(msg) \
    do { \
        utils::sanity_failure(utils::unreachable, __FILE__, __LINE__, msg); \
    } while (0)


#endif  // !defined(UTILS_SANITY_HPP)
