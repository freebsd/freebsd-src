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

#include "engine/filters.hpp"

#include <algorithm>
#include <stdexcept>

#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;

using utils::none;
using utils::optional;


/// Constructs a filter.
///
/// \param test_program_ The name of the test program or of the subdirectory to
///     match.
/// \param test_case_ The name of the test case to match.
engine::test_filter::test_filter(const fs::path& test_program_,
                                 const std::string& test_case_) :
    test_program(test_program_),
    test_case(test_case_)
{
}


/// Parses a user-provided test filter.
///
/// \param str The user-provided string representing a filter for tests.  Must
///     be of the form &lt;test_program%gt;[:&lt;test_case%gt;].
///
/// \return The parsed filter.
///
/// \throw std::runtime_error If the provided filter is invalid.
engine::test_filter
engine::test_filter::parse(const std::string& str)
{
    if (str.empty())
        throw std::runtime_error("Test filter cannot be empty");

    const std::string::size_type pos = str.find(':');
    if (pos == 0)
        throw std::runtime_error(F("Program name component in '%s' is empty")
                                 % str);
    if (pos == str.length() - 1)
        throw std::runtime_error(F("Test case component in '%s' is empty")
                                 % str);

    try {
        const fs::path test_program_(str.substr(0, pos));
        if (test_program_.is_absolute())
            throw std::runtime_error(F("Program name '%s' must be relative "
                                       "to the test suite, not absolute") %
                                       test_program_.str());
        if (pos == std::string::npos) {
            LD(F("Parsed user filter '%s': test program '%s', no test case") %
               str % test_program_.str());
            return test_filter(test_program_, "");
        } else {
            const std::string test_case_(str.substr(pos + 1));
            LD(F("Parsed user filter '%s': test program '%s', test case '%s'") %
               str % test_program_.str() % test_case_);
            return test_filter(test_program_, test_case_);
        }
    } catch (const fs::error& e) {
        throw std::runtime_error(F("Invalid path in filter '%s': %s") % str %
                                 e.what());
    }
}


/// Formats a filter for user presentation.
///
/// \return A user-friendly string representing the filter.  Note that this does
/// not necessarily match the string the user provided: in particular, the path
/// may have been internally normalized.
std::string
engine::test_filter::str(void) const
{
    if (!test_case.empty())
        return F("%s:%s") % test_program % test_case;
    else
        return test_program.str();
}


/// Checks if this filter contains another.
///
/// \param other The filter to compare to.
///
/// \return True if this filter contains the other filter or if they are equal.
bool
engine::test_filter::contains(const test_filter& other) const
{
    if (*this == other)
        return true;
    else
        return test_case.empty() && test_program.is_parent_of(
            other.test_program);
}


/// Checks if this filter matches a given test program name or subdirectory.
///
/// \param test_program_ The test program to compare to.
///
/// \return Whether the filter matches the test program.  This is a superset of
/// matches_test_case.
bool
engine::test_filter::matches_test_program(const fs::path& test_program_) const
{
    if (test_program == test_program_)
        return true;
    else {
        // Check if the filter matches a subdirectory of the test program.
        // The test case must be empty because we don't want foo:bar to match
        // foo/baz.
        return (test_case.empty() && test_program.is_parent_of(test_program_));
    }
}


/// Checks if this filter matches a given test case identifier.
///
/// \param test_program_ The test program to compare to.
/// \param test_case_ The test case to compare to.
///
/// \return Whether the filter matches the test case.
bool
engine::test_filter::matches_test_case(const fs::path& test_program_,
                                       const std::string& test_case_) const
{
    if (matches_test_program(test_program_)) {
        return test_case.empty() || test_case == test_case_;
    } else
        return false;
}


/// Less-than comparison for sorting purposes.
///
/// \param other The filter to compare to.
///
/// \return True if this filter sorts before the other filter.
bool
engine::test_filter::operator<(const test_filter& other) const
{
    return (
        test_program < other.test_program ||
        (test_program == other.test_program && test_case < other.test_case));
}


/// Equality comparison.
///
/// \param other The filter to compare to.
///
/// \return True if this filter is equal to the other filter.
bool
engine::test_filter::operator==(const test_filter& other) const
{
    return test_program == other.test_program && test_case == other.test_case;
}


/// Non-equality comparison.
///
/// \param other The filter to compare to.
///
/// \return True if this filter is different than the other filter.
bool
engine::test_filter::operator!=(const test_filter& other) const
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
engine::operator<<(std::ostream& output, const test_filter& object)
{
    if (object.test_case.empty()) {
        output << F("test_filter{test_program=%s}") % object.test_program;
    } else {
        output << F("test_filter{test_program=%s, test_case=%s}")
            % object.test_program % object.test_case;
    }
    return output;
}


/// Constructs a new set of filters.
///
/// \param filters_ The filters themselves; if empty, no filters are applied.
engine::test_filters::test_filters(const std::set< test_filter >& filters_) :
    _filters(filters_)
{
}


/// Checks if a given test program matches the set of filters.
///
/// This is provided as an optimization only, and the results of this function
/// are less specific than those of match_test_case.  Checking for the matching
/// of a test program should be done before loading the list of test cases from
/// a program, so as to avoid the delay in executing the test program, but
/// match_test_case must still be called afterwards.
///
/// \param name The test program to check against the filters.
///
/// \return True if the provided identifier matches any filter.
bool
engine::test_filters::match_test_program(const fs::path& name) const
{
    if (_filters.empty())
        return true;

    bool matches = false;
    for (std::set< test_filter >::const_iterator iter = _filters.begin();
         !matches && iter != _filters.end(); iter++) {
        matches = (*iter).matches_test_program(name);
    }
    return matches;
}


/// Checks if a given test case identifier matches the set of filters.
///
/// \param test_program The test program to check against the filters.
/// \param test_case The test case to check against the filters.
///
/// \return A boolean indicating if the test case is matched by any filter and,
/// if true, a string containing the filter name.  The string is empty when
/// there are no filters defined.
engine::test_filters::match
engine::test_filters::match_test_case(const fs::path& test_program,
                                      const std::string& test_case) const
{
    if (_filters.empty()) {
        INV(match_test_program(test_program));
        return match(true, none);
    }

    optional< test_filter > found = none;
    for (std::set< test_filter >::const_iterator iter = _filters.begin();
         !found && iter != _filters.end(); iter++) {
        if ((*iter).matches_test_case(test_program, test_case))
            found = *iter;
    }
    INV(!found || match_test_program(test_program));
    return match(static_cast< bool >(found), found);
}


/// Calculates the filters that have not matched any tests.
///
/// \param matched The filters that did match some tests.  This must be a subset
///     of the filters held by this object.
///
/// \return The set of filters that have not been used.
std::set< engine::test_filter >
engine::test_filters::difference(const std::set< test_filter >& matched) const
{
    PRE(std::includes(_filters.begin(), _filters.end(),
                      matched.begin(), matched.end()));

    std::set< test_filter > filters;
    std::set_difference(_filters.begin(), _filters.end(),
                        matched.begin(), matched.end(),
                        std::inserter(filters, filters.begin()));
    return filters;
}


/// Checks if a collection of filters is disjoint.
///
/// \param filters The filters to check.
///
/// \throw std::runtime_error If the filters are not disjoint.
void
engine::check_disjoint_filters(const std::set< engine::test_filter >& filters)
{
    // Yes, this is an O(n^2) algorithm.  However, we can assume that the number
    // of test filters (which are provided by the user on the command line) on a
    // particular run is in the order of tens, and thus this should not cause
    // any serious performance trouble.
    for (std::set< test_filter >::const_iterator i1 = filters.begin();
         i1 != filters.end(); i1++) {
        for (std::set< test_filter >::const_iterator i2 = filters.begin();
             i2 != filters.end(); i2++) {
            const test_filter& filter1 = *i1;
            const test_filter& filter2 = *i2;

            if (i1 != i2 && filter1.contains(filter2)) {
                throw std::runtime_error(
                    F("Filters '%s' and '%s' are not disjoint") %
                    filter1.str() % filter2.str());
            }
        }
    }
}


/// Constructs a filters_state instance.
///
/// \param filters_ The set of filters to track.
engine::filters_state::filters_state(
    const std::set< engine::test_filter >& filters_) :
    _filters(test_filters(filters_))
{
}


/// Checks whether these filters match the given test program.
///
/// \param test_program The test program to match against.
///
/// \return True if these filters match the given test program name.
bool
engine::filters_state::match_test_program(const fs::path& test_program) const
{
    return _filters.match_test_program(test_program);
}


/// Checks whether these filters match the given test case.
///
/// \param test_program The test program to match against.
/// \param test_case The test case to match against.
///
/// \return True if these filters match the given test case identifier.
bool
engine::filters_state::match_test_case(const fs::path& test_program,
                                       const std::string& test_case)
{
    engine::test_filters::match match = _filters.match_test_case(
        test_program, test_case);
    if (match.first && match.second)
        _used_filters.insert(match.second.get());
    return match.first;
}


/// Calculates the unused filters in this set.
///
/// \return Returns the set of filters that have not matched any tests.  This
/// information is useful to report usage errors to the user.
std::set< engine::test_filter >
engine::filters_state::unused(void) const
{
    return _filters.difference(_used_filters);
}
