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

#include "utils/text/regex.hpp"

extern "C" {
#include <sys/types.h>

#include <regex.h>
}

#include "utils/auto_array.ipp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"
#include "utils/text/exceptions.hpp"

namespace text = utils::text;


namespace {


static void throw_regex_error(const int, const ::regex_t*, const std::string&)
    UTILS_NORETURN;


/// Constructs and raises a regex_error.
///
/// \param error The error code returned by regcomp(3) or regexec(3).
/// \param preg The native regex object that caused this error.
/// \param prefix Error message prefix string.
///
/// \throw regex_error The constructed exception.
static void
throw_regex_error(const int error, const ::regex_t* preg,
                  const std::string& prefix)
{
    char buffer[1024];

    // TODO(jmmv): Would be nice to handle the case where the message does
    // not fit in the temporary buffer.
    (void)::regerror(error, preg, buffer, sizeof(buffer));

    throw text::regex_error(F("%s: %s") % prefix % buffer);
}


}  // anonymous namespace


/// Internal implementation for regex_matches.
struct utils::text::regex_matches::impl : utils::noncopyable {
    /// String on which we are matching.
    ///
    /// In theory, we could take a reference here instead of a copy, and make
    /// it a requirement for the caller to ensure that the lifecycle of the
    /// input string outlasts the lifecycle of the regex_matches.  However, that
    /// contract is very easy to break with hardcoded strings (as we do in
    /// tests).  Just go for the safer case here.
    const std::string _string;

    /// Maximum number of matching groups we expect, including the full match.
    ///
    /// In other words, this is the size of the _matches array.
    const std::size_t _nmatches;

    /// Native regular expression match representation.
    utils::auto_array< ::regmatch_t > _matches;

    /// Constructor.
    ///
    /// This executes the regex on the given string and sets up the internal
    /// class state based on the results.
    ///
    /// \param preg The native regex object.
    /// \param str The string on which to execute the regex.
    /// \param ngroups Number of capture groups in the regex.  This is an upper
    ///     bound and may be greater than the actual matches.
    ///
    /// \throw regex_error If the call to regexec(3) fails.
    impl(const ::regex_t* preg, const std::string& str,
         const std::size_t ngroups) :
        _string(str),
        _nmatches(ngroups + 1),
        _matches(new ::regmatch_t[_nmatches])
    {
        const int error = ::regexec(preg, _string.c_str(), _nmatches,
                                    _matches.get(), 0);
        if (error == REG_NOMATCH) {
            _matches.reset(NULL);
        } else if (error != 0) {
            throw_regex_error(error, preg,
                              F("regexec on '%s' failed") % _string);
        }
    }

    /// Destructor.
    ~impl(void)
    {
    }
};


/// Constructor.
///
/// \param pimpl Constructed implementation of the object.
text::regex_matches::regex_matches(std::shared_ptr< impl > pimpl) :
    _pimpl(pimpl)
{
}


/// Destructor.
text::regex_matches::~regex_matches(void)
{
}


/// Returns the number of matches in this object.
///
/// Note that this does not correspond to the number of groups provided at
/// construction time.  The returned value here accounts for only the returned
/// valid matches.
///
/// \return Number of matches, including the full match.
std::size_t
text::regex_matches::count(void) const
{
    std::size_t total = 0;
    if (_pimpl->_matches.get() != NULL) {
        for (std::size_t i = 0; i < _pimpl->_nmatches; ++i) {
            if (_pimpl->_matches[i].rm_so != -1)
                ++total;
        }
        INV(total <= _pimpl->_nmatches);
    }
    return total;
}


/// Gets a match.
///
/// \param index Number of the match to get.  Index 0 always contains the match
///     of the whole regex.
///
/// \pre There regex must have matched the input string.
/// \pre index must be lower than count().
///
/// \return The textual match.
std::string
text::regex_matches::get(const std::size_t index) const
{
    PRE(*this);
    PRE(index < count());

    const ::regmatch_t* match = &_pimpl->_matches[index];

    return std::string(_pimpl->_string.c_str() + match->rm_so,
                       match->rm_eo - match->rm_so);
}


/// Checks if there are any matches.
///
/// \return True if the object contains one or more matches; false otherwise.
text::regex_matches::operator bool(void) const
{
    return _pimpl->_matches.get() != NULL;
}


/// Internal implementation for regex.
struct utils::text::regex::impl : utils::noncopyable {
    /// Native regular expression representation.
    ::regex_t _preg;

    /// Number of capture groups in the regular expression.  This is an upper
    /// bound and does NOT include the default full string match.
    std::size_t _ngroups;

    /// Constructor.
    ///
    /// This compiles the given regular expression.
    ///
    /// \param regex_ The regular expression to compile.
    /// \param ngroups Number of capture groups in the regular expression.  This
    ///     is an upper bound and does NOT include the default full string
    ///     match.
    /// \param ignore_case Whether to ignore case during matching.
    ///
    /// \throw regex_error If the call to regcomp(3) fails.
    impl(const std::string& regex_, const std::size_t ngroups,
         const bool ignore_case) :
        _ngroups(ngroups)
    {
        const int flags = REG_EXTENDED | (ignore_case ? REG_ICASE : 0);
        const int error = ::regcomp(&_preg, regex_.c_str(), flags);
        if (error != 0)
            throw_regex_error(error, &_preg, F("regcomp on '%s' failed")
                              % regex_);
    }

    /// Destructor.
    ~impl(void)
    {
        ::regfree(&_preg);
    }
};


/// Constructor.
///
/// \param pimpl Constructed implementation of the object.
text::regex::regex(std::shared_ptr< impl > pimpl) : _pimpl(pimpl)
{
}


/// Destructor.
text::regex::~regex(void)
{
}


/// Compiles a new regular expression.
///
/// \param regex_ The regular expression to compile.
/// \param ngroups Number of capture groups in the regular expression.  This is
///     an upper bound and does NOT include the default full string match.
/// \param ignore_case Whether to ignore case during matching.
///
/// \return A new regular expression, ready to match strings.
///
/// \throw regex_error If the regular expression is invalid and cannot be
///     compiled.
text::regex
text::regex::compile(const std::string& regex_, const std::size_t ngroups,
                     const bool ignore_case)
{
    return regex(std::shared_ptr< impl >(new impl(regex_, ngroups,
                                                  ignore_case)));
}


/// Matches the regular expression against a string.
///
/// \param str String to match the regular expression against.
///
/// \return A new regex_matches object with the results of the match.
text::regex_matches
text::regex::match(const std::string& str) const
{
    std::shared_ptr< regex_matches::impl > pimpl(new regex_matches::impl(
        &_pimpl->_preg, str, _pimpl->_ngroups));
    return regex_matches(pimpl);
}


/// Compiles and matches a regular expression once.
///
/// This is syntactic sugar to simplify the instantiation of a new regex object
/// and its subsequent match on a string.
///
/// \param regex_ The regular expression to compile and match.
/// \param str String to match the regular expression against.
/// \param ngroups Number of capture groups in the regular expression.
/// \param ignore_case Whether to ignore case during matching.
///
/// \return A new regex_matches object with the results of the match.
text::regex_matches
text::match_regex(const std::string& regex_, const std::string& str,
                  const std::size_t ngroups, const bool ignore_case)
{
    return regex::compile(regex_, ngroups, ignore_case).match(str);
}
