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

#include "engine/scanner.hpp"

#include <deque>
#include <string>

#include "engine/filters.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

using utils::none;
using utils::optional;


namespace {


/// Extracts the keys of a map as a deque.
///
/// \tparam KeyType The type of the map keys.
/// \tparam ValueType The type of the map values.
/// \param map The input map.
///
/// \return A deque with the keys of the map.
template< typename KeyType, typename ValueType >
static std::deque< KeyType >
map_keys(const std::map< KeyType, ValueType >& map)
{
    std::deque< KeyType > keys;
    for (typename std::map< KeyType, ValueType >::const_iterator iter =
             map.begin(); iter != map.end(); ++iter) {
        keys.push_back((*iter).first);
    }
    return keys;
}


}  // anonymous namespace


/// Internal implementation for the scanner class.
struct engine::scanner::impl : utils::noncopyable {
    /// Collection of test programs not yet scanned.
    ///
    /// The first element in this deque is the "active" test program when
    /// first_test_cases is defined.
    std::deque< model::test_program_ptr > pending_test_programs;

    /// Current state of the provided filters.
    engine::filters_state filters;

    /// Collection of test cases not yet scanned.
    ///
    /// These are the test cases for the first test program in
    /// pending_test_programs when such test program is active.
    optional< std::deque< std::string > > first_test_cases;

    /// Constructor.
    ///
    /// \param test_programs_ Collection of test programs to scan through.
    /// \param filters_ List of scan filters as provided by the user.
    impl(const model::test_programs_vector& test_programs_,
         const std::set< engine::test_filter >& filters_) :
        pending_test_programs(test_programs_.begin(), test_programs_.end()),
        filters(filters_)
    {
    }

    /// Positions the internal state to return the next element if any.
    ///
    /// \post If there are more elements to read, returns true and
    /// pending_test_programs[0] points to the active test program and
    /// first_test_cases[0] has the test case to be returned.
    ///
    /// \return True if there is one more result available.
    bool
    advance(void)
    {
        for (;;) {
            if (first_test_cases) {
                if (first_test_cases.get().empty()) {
                    pending_test_programs.pop_front();
                    first_test_cases = none;
                }
            }
            if (pending_test_programs.empty()) {
                break;
            }

            model::test_program_ptr test_program = pending_test_programs[0];
            if (!first_test_cases) {
                if (!filters.match_test_program(
                        test_program->relative_path())) {
                    pending_test_programs.pop_front();
                    continue;
                }

                first_test_cases = utils::make_optional(
                    map_keys(test_program->test_cases()));
            }

            if (!first_test_cases.get().empty()) {
                std::deque< std::string >::iterator iter =
                    first_test_cases.get().begin();
                const std::string test_case_name = *iter;
                if (!filters.match_test_case(test_program->relative_path(),
                                             test_case_name)) {
                    first_test_cases.get().erase(iter);
                    continue;
                }
                return true;
            } else {
                pending_test_programs.pop_front();
                first_test_cases = none;
            }
        }
        return false;
    }

    /// Extracts the current element.
    ///
    /// \pre Must be called only if advance() returns true, and immediately
    /// afterwards.
    ///
    /// \return The current scan result.
    engine::scan_result
    consume(void)
    {
        const std::string test_case_name = first_test_cases.get()[0];
        first_test_cases.get().pop_front();
        return scan_result(pending_test_programs[0], test_case_name);
    }
};


/// Constructor.
///
/// \param test_programs Collection of test programs to scan through.
/// \param filters List of scan filters as provided by the user.
engine::scanner::scanner(const model::test_programs_vector& test_programs,
                         const std::set< engine::test_filter >& filters) :
    _pimpl(new impl(test_programs, filters))
{
}


/// Destructor.
engine::scanner::~scanner(void)
{
}


/// Returns the next scan result.
///
/// \return A scan result if there are still pending test cases to be processed,
/// or none otherwise.
optional< engine::scan_result >
engine::scanner::yield(void)
{
    if (_pimpl->advance()) {
        return utils::make_optional(_pimpl->consume());
    } else {
        return none;
    }
}


/// Checks whether the scan is finished.
///
/// \return True if the scan is finished, in which case yield() will return
/// none; false otherwise.
bool
engine::scanner::done(void)
{
    return !_pimpl->advance();
}


/// Returns the list of test filters that did not match any test case.
///
/// \return The collection of unmatched test filters.
std::set< engine::test_filter >
engine::scanner::unused_filters(void) const
{
    return _pimpl->filters.unused();
}
