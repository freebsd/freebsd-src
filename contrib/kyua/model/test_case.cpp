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

#include "model/test_case.hpp"

#include "model/metadata.hpp"
#include "model/test_result.hpp"
#include "utils/format/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/text/operations.ipp"

namespace text = utils::text;

using utils::none;
using utils::optional;


/// Internal implementation for a test_case.
struct model::test_case::impl : utils::noncopyable {
    /// Name of the test case; must be unique within the test program.
    std::string name;

    /// Metadata of the container test program.
    ///
    /// Yes, this is a pointer.  Yes, we do not own the object pointed to.
    /// However, because this is only intended to point to the metadata object
    /// of test programs _containing_ this test case, we can assume that the
    /// referenced object will be alive for the lifetime of this test case.
    const model::metadata* md_defaults;

    /// Test case metadata.
    model::metadata md;

    /// Fake result to return instead of running the test case.
    optional< model::test_result > fake_result;

    /// Optional pointer to a debugger attached.
    engine::debugger_ptr debugger;

    /// Constructor.
    ///
    /// \param name_ The name of the test case within the test program.
    /// \param md_defaults_ Metadata of the container test program.
    /// \param md_ Metadata of the test case.
    /// \param fake_result_ Fake result to return instead of running the test
    ///     case.
    impl(const std::string& name_,
         const model::metadata* md_defaults_,
         const model::metadata& md_,
         const optional< model::test_result >& fake_result_) :
        name(name_),
        md_defaults(md_defaults_),
        md(md_),
        fake_result(fake_result_)
    {
    }

    /// Gets the test case metadata.
    ///
    /// This combines the test case's metadata with any possible test program
    /// metadata, using the latter as defaults.
    ///
    /// \return The test case metadata.
    model::metadata
    get_metadata(void) const
    {
        if (md_defaults != NULL) {
            return md_defaults->apply_overrides(md);
        } else {
            return md;
        }
    }

    /// Equality comparator.
    ///
    /// \param other The other object to compare this one to.
    ///
    /// \return True if this object and other are equal; false otherwise.
    bool
    operator==(const impl& other) const
    {
        return (name == other.name &&
                get_metadata() == other.get_metadata() &&
                fake_result == other.fake_result);
    }
};


/// Constructs a new test case from an already-built impl oject.
///
/// \param pimpl_ The internal representation of the test case.
model::test_case::test_case(std::shared_ptr< impl > pimpl_) :
    _pimpl(pimpl_)
{
}


/// Constructs a new test case.
///
/// \param name_ The name of the test case within the test program.
/// \param md_ Metadata of the test case.
model::test_case::test_case(const std::string& name_,
                            const model::metadata& md_) :
    _pimpl(new impl(name_, NULL, md_, none))
{
}



/// Constructs a new fake test case.
///
/// A fake test case is a test case that is not really defined by the test
/// program.  Such test cases have a name surrounded by '__' and, when executed,
/// they return a fixed, pre-recorded result.
///
/// This is necessary for the cases where listing the test cases of a test
/// program fails.  In this scenario, we generate a single test case within
/// the test program that unconditionally returns a failure.
///
/// TODO(jmmv): Need to get rid of this.  We should be able to report the
/// status of test programs independently of test cases, as some interfaces
/// don't know about the latter at all.
///
/// \param name_ The name to give to this fake test case.  This name has to be
///     prefixed and suffixed by '__' to clearly denote that this is internal.
/// \param description_ The description of the test case, if any.
/// \param test_result_ The fake result to return when this test case is run.
model::test_case::test_case(
    const std::string& name_,
    const std::string& description_,
    const model::test_result& test_result_) :
    _pimpl(new impl(
        name_,
        NULL,
        model::metadata_builder().set_description(description_).build(),
        utils::make_optional(test_result_)))
{
    PRE_MSG(name_.length() > 4 && name_.substr(0, 2) == "__" &&
            name_.substr(name_.length() - 2) == "__",
            "Invalid fake name provided to fake test case");
}


/// Destroys a test case.
model::test_case::~test_case(void)
{
}


/// Constructs a new test case applying metadata defaults.
///
/// This method is intended to be used by the container test program when
/// ownership of the test is given to it.  At that point, the test case receives
/// the default metadata properties of the test program, not the global
/// defaults.
///
/// \param defaults The metadata properties to use as defaults.  The provided
///     object's lifetime MUST extend the lifetime of the test case.  Because
///     this is only intended to point at the metadata of the test program
///     containing this test case, this assumption should hold.
///
/// \return A new test case.
model::test_case
model::test_case::apply_metadata_defaults(const metadata* defaults) const
{
    return test_case(std::shared_ptr< impl >(new impl(
        _pimpl->name,
        defaults,
        _pimpl->md,
        _pimpl->fake_result)));
}


/// Gets the test case name.
///
/// \return The test case name, relative to the test program.
const std::string&
model::test_case::name(void) const
{
    return _pimpl->name;
}


/// Gets the test case metadata.
///
/// This combines the test case's metadata with any possible test program
/// metadata, using the latter as defaults.  You should use this method in
/// generaland not get_raw_metadata().
///
/// \return The test case metadata.
model::metadata
model::test_case::get_metadata(void) const
{
    return _pimpl->get_metadata();
}


/// Gets the original test case metadata without test program overrides.
///
/// This method should be used for storage purposes as serialized test cases
/// should record exactly whatever the test case reported and not what the test
/// program may have provided.  The final values will be reconstructed at load
/// time.
///
/// \return The test case metadata.
const model::metadata&
model::test_case::get_raw_metadata(void) const
{
    return _pimpl->md;
}


/// Attach a debugger to the test case.
void
model::test_case::attach_debugger(engine::debugger_ptr debugger) const
{
    _pimpl->debugger = debugger;
}


/// Gets the optional pointer to a debugger.
///
/// \return An optional pointer to a debugger.
engine::debugger_ptr
model::test_case::get_debugger() const
{
    return _pimpl->debugger;
}


/// Gets the fake result pre-stored for this test case.
///
/// \return A fake result, or none if not defined.
optional< model::test_result >
model::test_case::fake_result(void) const
{
    return _pimpl->fake_result;
}


/// Equality comparator.
///
/// \warning Because test cases reference their container test programs, and
/// test programs include test cases, we cannot perform a full comparison here:
/// otherwise, we'd enter an inifinte loop.  Therefore, out of necessity, this
/// does NOT compare whether the container test programs of the affected test
/// cases are the same.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are equal; false otherwise.
bool
model::test_case::operator==(const test_case& other) const
{
    return _pimpl == other._pimpl || *_pimpl == *other._pimpl;
}


/// Inequality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are different; false otherwise.
bool
model::test_case::operator!=(const test_case& other) const
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
model::operator<<(std::ostream& output, const test_case& object)
{
    output << F("test_case{name=%s, metadata=%s}")
        % text::quote(object.name(), '\'')
        % object.get_metadata();
    return output;
}


/// Adds an already-constructed test case.
///
/// \param test_case The test case to add.
///
/// \return A reference to this builder.
model::test_cases_map_builder&
model::test_cases_map_builder::add(const test_case& test_case)
{
    _test_cases.insert(
        test_cases_map::value_type(test_case.name(), test_case));
    return *this;
}


/// Constructs and adds a new test case with default metadata.
///
/// \param test_case_name The name of the test case to add.
///
/// \return A reference to this builder.
model::test_cases_map_builder&
model::test_cases_map_builder::add(const std::string& test_case_name)
{
    return add(test_case(test_case_name, metadata_builder().build()));
}


/// Constructs and adds a new test case with explicit metadata.
///
/// \param test_case_name The name of the test case to add.
/// \param metadata The metadata of the test case.
///
/// \return A reference to this builder.
model::test_cases_map_builder&
model::test_cases_map_builder::add(const std::string& test_case_name,
                                   const metadata& metadata)
{
    return add(test_case(test_case_name, metadata));
}


/// Creates a new test_cases_map.
///
/// \return The constructed test_cases_map.
model::test_cases_map
model::test_cases_map_builder::build(void) const
{
    return _test_cases;
}
