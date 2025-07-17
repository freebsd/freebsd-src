// Copyright 2014 The Kyua Authors.
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

#if !defined(UTILS_FORMAT_CONTAINERS_IPP)
#define UTILS_FORMAT_CONTAINERS_IPP

#include "utils/format/containers.hpp"

#include <ostream>


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
template< typename K, typename V >
std::ostream&
std::operator<<(std::ostream& output, const std::map< K, V >& object)
{
    output << "map(";
    typename std::map< K, V >::size_type counter = 0;
    for (typename std::map< K, V >::const_iterator iter = object.begin();
         iter != object.end(); ++iter, ++counter) {
        if (counter != 0)
            output << ", ";
        output << (*iter).first << "=" << (*iter).second;
    }
    output << ")";
    return output;
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
template< typename T1, typename T2 >
std::ostream&
std::operator<<(std::ostream& output, const std::pair< T1, T2 >& object)
{
    output << "pair(" << object.first << ", " << object.second << ")";
    return output;
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
template< typename T >
std::ostream&
std::operator<<(std::ostream& output, const std::shared_ptr< T > object)
{
    if (object.get() == NULL) {
        output << "<NULL>";
    } else {
        output << *object;
    }
    return output;
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
template< typename T >
std::ostream&
std::operator<<(std::ostream& output, const std::set< T >& object)
{
    output << "set(";
    typename std::set< T >::size_type counter = 0;
    for (typename std::set< T >::const_iterator iter = object.begin();
         iter != object.end(); ++iter, ++counter) {
        if (counter != 0)
            output << ", ";
        output << (*iter);
    }
    output << ")";
    return output;
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
template< typename T >
std::ostream&
std::operator<<(std::ostream& output, const std::vector< T >& object)
{
    output << "[";
    for (typename std::vector< T >::size_type i = 0; i < object.size(); ++i) {
        if (i != 0)
            output << ", ";
        output << object[i];
    }
    output << "]";
    return output;
}


#endif  // !defined(UTILS_FORMAT_CONTAINERS_IPP)
