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

#include "model/test_program.hpp"

#include <map>
#include <sstream>

#include "model/exceptions.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_result.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"
#include "utils/text/operations.ipp"

namespace fs = utils::fs;
namespace text = utils::text;

using utils::none;


/// Internal implementation of a test_program.
struct model::test_program::impl : utils::noncopyable {
    /// Name of the test program interface.
    std::string interface_name;

    /// Name of the test program binary relative to root.
    fs::path binary;

    /// Root of the test suite containing the test program.
    fs::path root;

    /// Name of the test suite this program belongs to.
    std::string test_suite_name;

    /// Metadata of the test program.
    model::metadata md;

    /// List of test cases in the test program.
    ///
    /// Must be queried via the test_program::test_cases() method.
    model::test_cases_map test_cases;

    /// Constructor.
    ///
    /// \param interface_name_ Name of the test program interface.
    /// \param binary_ The name of the test program binary relative to root_.
    /// \param root_ The root of the test suite containing the test program.
    /// \param test_suite_name_ The name of the test suite this program
    ///     belongs to.
    /// \param md_ Metadata of the test program.
    /// \param test_cases_ The collection of test cases in the test program.
    impl(const std::string& interface_name_, const fs::path& binary_,
         const fs::path& root_, const std::string& test_suite_name_,
         const model::metadata& md_, const model::test_cases_map& test_cases_) :
        interface_name(interface_name_),
        binary(binary_),
        root(root_),
        test_suite_name(test_suite_name_),
        md(md_)
    {
        PRE_MSG(!binary.is_absolute(),
                F("The program '%s' must be relative to the root of the test "
                  "suite '%s'") % binary % root);

        set_test_cases(test_cases_);
    }

    /// Sets the list of test cases of the test program.
    ///
    /// \param test_cases_ The new list of test cases.
    void
    set_test_cases(const model::test_cases_map& test_cases_)
    {
        for (model::test_cases_map::const_iterator iter = test_cases_.begin();
             iter != test_cases_.end(); ++iter) {
            const std::string& name = (*iter).first;
            const model::test_case& test_case = (*iter).second;

            PRE_MSG(name == test_case.name(),
                    F("The test case '%s' has been registered with the "
                      "non-matching name '%s'") % name % test_case.name());

            test_cases.insert(model::test_cases_map::value_type(
                name, test_case.apply_metadata_defaults(&md)));
        }
        INV(test_cases.size() == test_cases_.size());
    }
};


/// Constructs a new test program.
///
/// \param interface_name_ Name of the test program interface.
/// \param binary_ The name of the test program binary relative to root_.
/// \param root_ The root of the test suite containing the test program.
/// \param test_suite_name_ The name of the test suite this program belongs to.
/// \param md_ Metadata of the test program.
/// \param test_cases_ The collection of test cases in the test program.
model::test_program::test_program(const std::string& interface_name_,
                                  const fs::path& binary_,
                                  const fs::path& root_,
                                  const std::string& test_suite_name_,
                                  const model::metadata& md_,
                                  const model::test_cases_map& test_cases_) :
    _pimpl(new impl(interface_name_, binary_, root_, test_suite_name_, md_,
                    test_cases_))
{
}


/// Destroys a test program.
model::test_program::~test_program(void)
{
}


/// Gets the name of the test program interface.
///
/// \return An interface name.
const std::string&
model::test_program::interface_name(void) const
{
    return _pimpl->interface_name;
}


/// Gets the path to the test program relative to the root of the test suite.
///
/// \return The relative path to the test program binary.
const fs::path&
model::test_program::relative_path(void) const
{
    return _pimpl->binary;
}


/// Gets the absolute path to the test program.
///
/// \return The absolute path to the test program binary.
const fs::path
model::test_program::absolute_path(void) const
{
    const fs::path full_path = _pimpl->root / _pimpl->binary;
    return full_path.is_absolute() ? full_path : full_path.to_absolute();
}


/// Gets the root of the test suite containing this test program.
///
/// \return The path to the root of the test suite.
const fs::path&
model::test_program::root(void) const
{
    return _pimpl->root;
}


/// Gets the name of the test suite containing this test program.
///
/// \return The name of the test suite.
const std::string&
model::test_program::test_suite_name(void) const
{
    return _pimpl->test_suite_name;
}


/// Gets the metadata of the test program.
///
/// \return The metadata.
const model::metadata&
model::test_program::get_metadata(void) const
{
    return _pimpl->md;
}


/// Gets a test case by its name.
///
/// \param name The name of the test case to locate.
///
/// \return The requested test case.
///
/// \throw not_found_error If the specified test case is not in the test
///     program.
const model::test_case&
model::test_program::find(const std::string& name) const
{
    const test_cases_map& tcs = test_cases();

    const test_cases_map::const_iterator iter = tcs.find(name);
    if (iter == tcs.end())
        throw not_found_error(F("Unknown test case %s in test program %s") %
                              name % relative_path());
    return (*iter).second;
}


/// Gets the list of test cases from the test program.
///
/// \return The list of test cases provided by the test program.
const model::test_cases_map&
model::test_program::test_cases(void) const
{
    return _pimpl->test_cases;
}


/// Sets the list of test cases of the test program.
///
/// This can only be called once and it may only be called from within
/// overridden test_cases() before that method ever returns a value for the
/// first time.  Any other invocations will result in inconsistent program
/// state.
///
/// \param test_cases_ The new list of test cases.
void
model::test_program::set_test_cases(const model::test_cases_map& test_cases_)
{
    PRE(_pimpl->test_cases.empty());
    _pimpl->set_test_cases(test_cases_);
}


/// Equality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are equal; false otherwise.
bool
model::test_program::operator==(const test_program& other) const
{
    return _pimpl == other._pimpl || (
        _pimpl->interface_name == other._pimpl->interface_name &&
        _pimpl->binary == other._pimpl->binary &&
        _pimpl->root == other._pimpl->root &&
        _pimpl->test_suite_name == other._pimpl->test_suite_name &&
        _pimpl->md == other._pimpl->md &&
        test_cases() == other.test_cases());
}


/// Inequality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are different; false otherwise.
bool
model::test_program::operator!=(const test_program& other) const
{
    return !(*this == other);
}


/// Less-than comparator.
///
/// A test program is considered to be less than another if and only if the
/// former's absolute path is less than the absolute path of the latter.  In
/// other words, the absolute path is used here as the test program's
/// identifier.
///
/// This simplistic less-than operator overload is provided so that test
/// programs can be held in sets and other containers.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object sorts before the other object; false otherwise.
bool
model::test_program::operator<(const test_program& other) const
{
    return absolute_path() < other.absolute_path();
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
std::ostream&
model::operator<<(std::ostream& output, const test_program& object)
{
    output << F("test_program{interface=%s, binary=%s, root=%s, test_suite=%s, "
                "metadata=%s, test_cases=%s}")
        % text::quote(object.interface_name(), '\'')
        % text::quote(object.relative_path().str(), '\'')
        % text::quote(object.root().str(), '\'')
        % text::quote(object.test_suite_name(), '\'')
        % object.get_metadata()
        % object.test_cases();
    return output;
}


/// Internal implementation of the test_program_builder class.
struct model::test_program_builder::impl : utils::noncopyable {
    /// Partially-constructed program with only the required properties.
    model::test_program prototype;

    /// Optional metadata for the test program.
    model::metadata metadata;

    /// Collection of test cases.
    model::test_cases_map test_cases;

    /// Whether we have created a test_program object or not.
    bool built;

    /// Constructor.
    ///
    /// \param prototype_ The partially constructed program with only the
    ///     required properties.
    impl(const model::test_program& prototype_) :
        prototype(prototype_),
        metadata(model::metadata_builder().build()),
        built(false)
    {
    }
};


/// Constructs a new builder with non-optional values.
///
/// \param interface_name_ Name of the test program interface.
/// \param binary_ The name of the test program binary relative to root_.
/// \param root_ The root of the test suite containing the test program.
/// \param test_suite_name_ The name of the test suite this program belongs to.
model::test_program_builder::test_program_builder(
    const std::string& interface_name_, const fs::path& binary_,
    const fs::path& root_, const std::string& test_suite_name_) :
    _pimpl(new impl(model::test_program(interface_name_, binary_, root_,
                                        test_suite_name_,
                                        model::metadata_builder().build(),
                                        model::test_cases_map())))
{
}


/// Destructor.
model::test_program_builder::~test_program_builder(void)
{
}


/// Accumulates an additional test case with default metadata.
///
/// \param test_case_name The name of the test case.
///
/// \return A reference to this builder.
model::test_program_builder&
model::test_program_builder::add_test_case(const std::string& test_case_name)
{
    return add_test_case(test_case_name, model::metadata_builder().build());
}


/// Accumulates an additional test case.
///
/// \param test_case_name The name of the test case.
/// \param metadata The test case metadata.
///
/// \return A reference to this builder.
model::test_program_builder&
model::test_program_builder::add_test_case(const std::string& test_case_name,
                                           const model::metadata& metadata)
{
    const model::test_case test_case(test_case_name, metadata);
    PRE_MSG(_pimpl->test_cases.find(test_case_name) == _pimpl->test_cases.end(),
            F("Attempted to re-register test case '%s'") % test_case_name);
    _pimpl->test_cases.insert(model::test_cases_map::value_type(
        test_case_name, test_case));
    return *this;
}


/// Sets the test program metadata.
///
/// \return metadata The metadata for the test program.
///
/// \return A reference to this builder.
model::test_program_builder&
model::test_program_builder::set_metadata(const model::metadata& metadata)
{
    _pimpl->metadata = metadata;
    return *this;
}


/// Creates a new test_program object.
///
/// \pre This has not yet been called.  We only support calling this function
/// once.
///
/// \return The constructed test_program object.
model::test_program
model::test_program_builder::build(void) const
{
    PRE(!_pimpl->built);
    _pimpl->built = true;

    return test_program(_pimpl->prototype.interface_name(),
                        _pimpl->prototype.relative_path(),
                        _pimpl->prototype.root(),
                        _pimpl->prototype.test_suite_name(),
                        _pimpl->metadata,
                        _pimpl->test_cases);
}


/// Creates a new dynamically-allocated test_program object.
///
/// \pre This has not yet been called.  We only support calling this function
/// once.
///
/// \return The constructed test_program object.
model::test_program_ptr
model::test_program_builder::build_ptr(void) const
{
    const test_program result = build();
    return test_program_ptr(new test_program(result));
}
