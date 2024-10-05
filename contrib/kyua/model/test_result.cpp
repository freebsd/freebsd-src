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

#include "model/test_result.hpp"

#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/text/operations.ipp"

namespace text = utils::text;


const std::map<enum model::test_result_type,
               const struct model::test_result_type_desc>
    model::test_result_types =
{
    { test_result_broken,
        { .id =      test_result_broken,
          .name =    "broken",
          .is_run =  true,
          .is_good = false, } },

    { test_result_expected_failure,
        { .id =      test_result_expected_failure,
          .name =    "xfail",
          .is_run =  true,
          .is_good = true, } },

    { test_result_failed,
        { .id =      test_result_failed,
          .name =    "failed",
          .is_run =  true,
          .is_good = false, } },

    { test_result_passed,
        { .id =      test_result_passed,
          .name =    "passed",
          .is_run =  true,
          .is_good = true, } },

    { test_result_skipped,
        { .id =      test_result_skipped,
          .name =    "skipped",
          .is_run =  false,
          .is_good = true, } },
};


/// Constructs a base result.
///
/// \param type_ The type of the result.
/// \param reason_ The reason explaining the result, if any.  It is OK for this
///     to be empty, which is actually the default.
model::test_result::test_result(const test_result_type type_,
                                const std::string& reason_) :
    _type(type_),
    _reason(reason_)
{
}


/// Returns the type of the result.
///
/// \return A result type.
model::test_result_type
model::test_result::type(void) const
{
    return _type;
}


/// Returns the reason explaining the result.
///
/// \return A textual reason, possibly empty.
const std::string&
model::test_result::reason(void) const
{
    return _reason;
}


/// True if the test case result has a positive connotation.
///
/// \return Whether the test case is good or not.
bool
model::test_result::good(void) const
{
    return test_result_types.at(_type).is_good;
}


/// Equality comparator.
///
/// \param other The test result to compare to.
///
/// \return True if the other object is equal to this one, false otherwise.
bool
model::test_result::operator==(const test_result& other) const
{
    return _type == other._type && _reason == other._reason;
}


/// Inequality comparator.
///
/// \param other The test result to compare to.
///
/// \return True if the other object is different from this one, false
/// otherwise.
bool
model::test_result::operator!=(const test_result& other) const
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
model::operator<<(std::ostream& output, const test_result& object)
{
    std::string result_name;
    switch (object.type()) {
    case test_result_broken: result_name = "broken"; break;
    case test_result_expected_failure: result_name = "expected_failure"; break;
    case test_result_failed: result_name = "failed"; break;
    case test_result_passed: result_name = "passed"; break;
    case test_result_skipped: result_name = "skipped"; break;
    }
    const std::string& reason = object.reason();
    if (reason.empty()) {
        output << F("model::test_result{type=%s}")
            % text::quote(result_name, '\'');
    } else {
        output << F("model::test_result{type=%s, reason=%s}")
            % text::quote(result_name, '\'') % text::quote(reason, '\'');
    }
    return output;
}
