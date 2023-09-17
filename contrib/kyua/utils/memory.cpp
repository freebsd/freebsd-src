// Copyright 2012 The Kyua Authors.
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

#include "utils/memory.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

extern "C" {
#if defined(HAVE_SYS_TYPES_H)
#   include <sys/types.h>
#endif
#if defined(HAVE_SYS_PARAM_H)
#   include <sys/param.h>
#endif
#if defined(HAVE_SYS_SYSCTL_H)
#   include <sys/sysctl.h>
#endif
}

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/units.hpp"
#include "utils/sanity.hpp"

namespace units = utils::units;


namespace {


/// Name of the method to query the available memory as detected by configure.
static const char* query_type = MEMORY_QUERY_TYPE;


/// Value of query_type when we do not know how to query the memory.
static const char* query_type_unknown = "unknown";


/// Value of query_type when we have to use sysctlbyname(3).
static const char* query_type_sysctlbyname = "sysctlbyname";


/// Name of the sysctl MIB with the physical memory as detected by configure.
///
/// This should only be used if memory_query_type is 'sysctl'.
static const char* query_sysctl_mib = MEMORY_QUERY_SYSCTL_MIB;


#if !defined(HAVE_SYSCTLBYNAME)
/// Stub for sysctlbyname(3) for systems that don't have it.
///
/// The whole purpose of this fake function is to allow the caller code to be
/// compiled on any machine regardless of the presence of sysctlbyname(3).  This
/// will prevent the code from breaking when it is compiled on a machine without
/// this function.  It also prevents "unused variable" warnings in the caller
/// code.
///
/// \return Nothing; this always crashes.
static int
sysctlbyname(const char* /* name */,
             void* /* oldp */,
             std::size_t* /* oldlenp */,
             const void* /* newp */,
             std::size_t /* newlen */)
{
    UNREACHABLE;
}
#endif


}  // anonymous namespace


/// Gets the value of an integral sysctl MIB.
///
/// \pre The system supports the sysctlbyname(3) function.
///
/// \param mib The name of the sysctl MIB to query.
///
/// \return The value of the MIB, if found.
///
/// \throw std::runtime_error If the sysctlbyname(3) call fails.  This might be
///     a bit drastic.  If it turns out that this causes problems, we could just
///     change the code to log the error instead of raising an exception.
static int64_t
query_sysctl(const char* mib)
{
    // This must be explicitly initialized to 0.  If the sysctl query returned a
    // value smaller in size than value_length, we would get garbage otherwise.
    int64_t value = 0;
    std::size_t value_length = sizeof(value);
    if (::sysctlbyname(mib, &value, &value_length, NULL, 0) == -1) {
        const int original_errno = errno;
        throw std::runtime_error(F("Failed to get sysctl(%s) value: %s") %
                                 mib % std::strerror(original_errno));
    }
    return value;
}


/// Queries the total amount of physical memory.
///
/// The real query is run only once and the result is cached.  Further calls to
/// this function will always return the same value.
///
/// \return The amount of physical memory, in bytes.  If the code does not know
/// how to query the memory, this logs a warning and returns 0.
units::bytes
utils::physical_memory(void)
{
    static int64_t amount = -1;
    if (amount == -1) {
        if (std::strcmp(query_type, query_type_unknown) == 0) {
            LW("Don't know how to query the physical memory");
            amount = 0;
        } else if (std::strcmp(query_type, query_type_sysctlbyname) == 0) {
            amount = query_sysctl(query_sysctl_mib);
        } else
            UNREACHABLE_MSG("Unimplemented memory query type");
        LI(F("Physical memory as returned by query type '%s': %s") %
           query_type % amount);
    }
    POST(amount > -1);
    return units::bytes(amount);
}
