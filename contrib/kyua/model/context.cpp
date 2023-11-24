// Copyright 2011 The Kyua Authors.
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

#include "model/context.hpp"

#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/noncopyable.hpp"
#include "utils/text/operations.ipp"

namespace fs = utils::fs;
namespace text = utils::text;


/// Internal implementation of a context.
struct model::context::impl : utils::noncopyable {
    /// The current working directory.
    fs::path _cwd;

    /// The environment variables.
    std::map< std::string, std::string > _env;

    /// Constructor.
    ///
    /// \param cwd_ The current working directory.
    /// \param env_ The environment variables.
    impl(const fs::path& cwd_,
         const std::map< std::string, std::string >& env_) :
        _cwd(cwd_),
        _env(env_)
    {
    }

    /// Equality comparator.
    ///
    /// \param other The object to compare to.
    ///
    /// \return True if the two objects are equal; false otherwise.
    bool
    operator==(const impl& other) const
    {
        return _cwd == other._cwd && _env == other._env;
    }
};


/// Constructs a new context.
///
/// \param cwd_ The current working directory.
/// \param env_ The environment variables.
model::context::context(const fs::path& cwd_,
                         const std::map< std::string, std::string >& env_) :
    _pimpl(new impl(cwd_, env_))
{
}


/// Destructor.
model::context::~context(void)
{
}


/// Returns the current working directory of the context.
///
/// \return A path.
const fs::path&
model::context::cwd(void) const
{
    return _pimpl->_cwd;
}


/// Returns the environment variables of the context.
///
/// \return A variable name to variable value mapping.
const std::map< std::string, std::string >&
model::context::env(void) const
{
    return _pimpl->_env;
}


/// Equality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are equal; false otherwise.
bool
model::context::operator==(const context& other) const
{
    return *_pimpl == *other._pimpl;
}


/// Inequality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are different; false otherwise.
bool
model::context::operator!=(const context& other) const
{
    return !(*this == other);
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
std::ostream&
model::operator<<(std::ostream& output, const context& object)
{
    output << F("context{cwd=%s, env=[")
        % text::quote(object.cwd().str(), '\'');

    const std::map< std::string, std::string >& env = object.env();
    bool first = true;
    for (std::map< std::string, std::string >::const_iterator
             iter = env.begin(); iter != env.end(); ++iter) {
        if (!first)
            output << ", ";
        first = false;

        output << F("%s=%s") % (*iter).first
            % text::quote((*iter).second, '\'');
    }

    output << "]}";
    return output;
}
