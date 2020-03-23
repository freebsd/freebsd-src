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

#include "utils/cmdline/options.hpp"

#include <stdexcept>
#include <vector>

#include "utils/cmdline/exceptions.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/path.hpp"
#include "utils/sanity.hpp"
#include "utils/text/operations.ipp"

namespace cmdline = utils::cmdline;
namespace text = utils::text;


/// Constructs a generic option with both a short and a long name.
///
/// \param short_name_ The short name for the option.
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ If not NULL, specifies that the option must receive an
///     argument and specifies the name of such argument for documentation
///     purposes.
/// \param default_value_ If not NULL, specifies that the option has a default
///     value for the mandatory argument.
cmdline::base_option::base_option(const char short_name_,
                                  const char* long_name_,
                                  const char* description_,
                                  const char* arg_name_,
                                  const char* default_value_) :
    _short_name(short_name_),
    _long_name(long_name_),
    _description(description_),
    _arg_name(arg_name_ == NULL ? "" : arg_name_),
    _has_default_value(default_value_ != NULL),
    _default_value(default_value_ == NULL ? "" : default_value_)
{
    INV(short_name_ != '\0');
}


/// Constructs a generic option with a long name only.
///
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ If not NULL, specifies that the option must receive an
///     argument and specifies the name of such argument for documentation
///     purposes.
/// \param default_value_ If not NULL, specifies that the option has a default
///     value for the mandatory argument.
cmdline::base_option::base_option(const char* long_name_,
                                  const char* description_,
                                  const char* arg_name_,
                                  const char* default_value_) :
    _short_name('\0'),
    _long_name(long_name_),
    _description(description_),
    _arg_name(arg_name_ == NULL ? "" : arg_name_),
    _has_default_value(default_value_ != NULL),
    _default_value(default_value_ == NULL ? "" : default_value_)
{
}


/// Destructor for the option.
cmdline::base_option::~base_option(void)
{
}


/// Checks whether the option has a short name or not.
///
/// \return True if the option has a short name, false otherwise.
bool
cmdline::base_option::has_short_name(void) const
{
    return _short_name != '\0';
}


/// Returns the short name of the option.
///
/// \pre has_short_name() must be true.
///
/// \return The short name.
char
cmdline::base_option::short_name(void) const
{
    PRE(has_short_name());
    return _short_name;
}


/// Returns the long name of the option.
///
/// \return The long name.
const std::string&
cmdline::base_option::long_name(void) const
{
    return _long_name;
}


/// Returns the description of the option.
///
/// \return The description.
const std::string&
cmdline::base_option::description(void) const
{
    return _description;
}


/// Checks whether the option needs an argument or not.
///
/// \return True if the option needs an argument, false otherwise.
bool
cmdline::base_option::needs_arg(void) const
{
    return !_arg_name.empty();
}


/// Returns the argument name of the option for documentation purposes.
///
/// \pre needs_arg() must be true.
///
/// \return The argument name.
const std::string&
cmdline::base_option::arg_name(void) const
{
    INV(needs_arg());
    return _arg_name;
}


/// Checks whether the option has a default value for its argument.
///
/// \pre needs_arg() must be true.
///
/// \return True if the option has a default value, false otherwise.
bool
cmdline::base_option::has_default_value(void) const
{
    PRE(needs_arg());
    return _has_default_value;
}


/// Returns the default value for the argument to the option.
///
/// \pre has_default_value() must be true.
///
/// \return The default value.
const std::string&
cmdline::base_option::default_value(void) const
{
    INV(has_default_value());
    return _default_value;;
}


/// Formats the short name of the option for documentation purposes.
///
/// \return A string describing the option's short name.
std::string
cmdline::base_option::format_short_name(void) const
{
    PRE(has_short_name());

    if (needs_arg()) {
        return F("-%s %s") % short_name() % arg_name();
    } else {
        return F("-%s") % short_name();
    }
}


/// Formats the long name of the option for documentation purposes.
///
/// \return A string describing the option's long name.
std::string
cmdline::base_option::format_long_name(void) const
{
    if (needs_arg()) {
        return F("--%s=%s") % long_name() % arg_name();
    } else {
        return F("--%s") % long_name();
    }
}



/// Ensures that an argument passed to the option is valid.
///
/// This must be reimplemented by subclasses that describe options with
/// arguments.
///
/// \throw cmdline::option_argument_value_error Subclasses must raise this
///     exception to indicate the cases in which str is invalid.
void
cmdline::base_option::validate(const std::string& /* str */) const
{
    UNREACHABLE_MSG("Option does not support an argument");
}


/// Constructs a boolean option with both a short and a long name.
///
/// \param short_name_ The short name for the option.
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
cmdline::bool_option::bool_option(const char short_name_,
                                  const char* long_name_,
                                  const char* description_) :
    base_option(short_name_, long_name_, description_)
{
}


/// Constructs a boolean option with a long name only.
///
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
cmdline::bool_option::bool_option(const char* long_name_,
                                  const char* description_) :
    base_option(long_name_, description_)
{
}


/// Constructs an integer option with both a short and a long name.
///
/// \param short_name_ The short name for the option.
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ The name of the mandatory argument, for documentation
///     purposes.
/// \param default_value_ If not NULL, the default value for the mandatory
///     argument.
cmdline::int_option::int_option(const char short_name_,
                                const char* long_name_,
                                const char* description_,
                                const char* arg_name_,
                                const char* default_value_) :
    base_option(short_name_, long_name_, description_, arg_name_,
                default_value_)
{
}


/// Constructs an integer option with a long name only.
///
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ The name of the mandatory argument, for documentation
///     purposes.
/// \param default_value_ If not NULL, the default value for the mandatory
///     argument.
cmdline::int_option::int_option(const char* long_name_,
                                const char* description_,
                                const char* arg_name_,
                                const char* default_value_) :
    base_option(long_name_, description_, arg_name_, default_value_)
{
}


/// Ensures that an integer argument passed to the int_option is valid.
///
/// \param raw_value The argument representing an integer as provided by the
///     user.
///
/// \throw cmdline::option_argument_value_error If the integer provided in
///     raw_value is invalid.
void
cmdline::int_option::validate(const std::string& raw_value) const
{
    try {
        (void)text::to_type< int >(raw_value);
    } catch (const std::runtime_error& e) {
        throw cmdline::option_argument_value_error(
            F("--%s") % long_name(), raw_value, "Not a valid integer");
    }
}


/// Converts an integer argument to a native integer.
///
/// \param raw_value The argument representing an integer as provided by the
///     user.
///
/// \return The integer.
///
/// \pre validate(raw_value) must be true.
int
cmdline::int_option::convert(const std::string& raw_value)
{
    try {
        return text::to_type< int >(raw_value);
    } catch (const std::runtime_error& e) {
        PRE_MSG(false, F("Raw value '%s' for int option not properly "
                         "validated: %s") % raw_value % e.what());
    }
}


/// Constructs a list option with both a short and a long name.
///
/// \param short_name_ The short name for the option.
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ The name of the mandatory argument, for documentation
///     purposes.
/// \param default_value_ If not NULL, the default value for the mandatory
///     argument.
cmdline::list_option::list_option(const char short_name_,
                                  const char* long_name_,
                                  const char* description_,
                                  const char* arg_name_,
                                  const char* default_value_) :
    base_option(short_name_, long_name_, description_, arg_name_,
                default_value_)
{
}


/// Constructs a list option with a long name only.
///
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ The name of the mandatory argument, for documentation
///     purposes.
/// \param default_value_ If not NULL, the default value for the mandatory
///     argument.
cmdline::list_option::list_option(const char* long_name_,
                                  const char* description_,
                                  const char* arg_name_,
                                  const char* default_value_) :
    base_option(long_name_, description_, arg_name_, default_value_)
{
}


/// Ensures that a lisstring argument passed to the list_option is valid.
void
cmdline::list_option::validate(
    const std::string& /* raw_value */) const
{
    // Any list is potentially valid; the caller must check for semantics.
}


/// Converts a string argument to a vector.
///
/// \param raw_value The argument representing a list as provided by the user.
///
/// \return The list.
///
/// \pre validate(raw_value) must be true.
cmdline::list_option::option_type
cmdline::list_option::convert(const std::string& raw_value)
{
    try {
        return text::split(raw_value, ',');
    } catch (const std::runtime_error& e) {
        PRE_MSG(false, F("Raw value '%s' for list option not properly "
                         "validated: %s") % raw_value % e.what());
    }
}


/// Constructs a path option with both a short and a long name.
///
/// \param short_name_ The short name for the option.
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ The name of the mandatory argument, for documentation
///     purposes.
/// \param default_value_ If not NULL, the default value for the mandatory
///     argument.
cmdline::path_option::path_option(const char short_name_,
                                  const char* long_name_,
                                  const char* description_,
                                  const char* arg_name_,
                                  const char* default_value_) :
    base_option(short_name_, long_name_, description_, arg_name_,
                default_value_)
{
}


/// Constructs a path option with a long name only.
///
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ The name of the mandatory argument, for documentation
///     purposes.
/// \param default_value_ If not NULL, the default value for the mandatory
///     argument.
cmdline::path_option::path_option(const char* long_name_,
                                  const char* description_,
                                  const char* arg_name_,
                                  const char* default_value_) :
    base_option(long_name_, description_, arg_name_, default_value_)
{
}


/// Ensures that a path argument passed to the path_option is valid.
///
/// \param raw_value The argument representing a path as provided by the user.
///
/// \throw cmdline::option_argument_value_error If the path provided in
///     raw_value is invalid.
void
cmdline::path_option::validate(const std::string& raw_value) const
{
    try {
        (void)utils::fs::path(raw_value);
    } catch (const utils::fs::error& e) {
        throw cmdline::option_argument_value_error(F("--%s") % long_name(),
                                                   raw_value, e.what());
    }
}


/// Converts a path argument to a utils::fs::path.
///
/// \param raw_value The argument representing a path as provided by the user.
///
/// \return The path.
///
/// \pre validate(raw_value) must be true.
utils::fs::path
cmdline::path_option::convert(const std::string& raw_value)
{
    try {
        return utils::fs::path(raw_value);
    } catch (const std::runtime_error& e) {
        PRE_MSG(false, F("Raw value '%s' for path option not properly "
                         "validated: %s") % raw_value % e.what());
    }
}


/// Constructs a property option with both a short and a long name.
///
/// \param short_name_ The short name for the option.
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ The name of the mandatory argument, for documentation
///     purposes.  Must include the '=' delimiter.
cmdline::property_option::property_option(const char short_name_,
                                          const char* long_name_,
                                          const char* description_,
                                          const char* arg_name_) :
    base_option(short_name_, long_name_, description_, arg_name_)
{
    PRE(arg_name().find('=') != std::string::npos);
}


/// Constructs a property option with a long name only.
///
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ The name of the mandatory argument, for documentation
///     purposes.  Must include the '=' delimiter.
cmdline::property_option::property_option(const char* long_name_,
                                          const char* description_,
                                          const char* arg_name_) :
    base_option(long_name_, description_, arg_name_)
{
    PRE(arg_name().find('=') != std::string::npos);
}


/// Validates the argument to a property option.
///
/// \param raw_value The argument provided by the user.
void
cmdline::property_option::validate(const std::string& raw_value) const
{
    const std::string::size_type pos = raw_value.find('=');
    if (pos == std::string::npos)
        throw cmdline::option_argument_value_error(
            F("--%s") % long_name(), raw_value,
            F("Argument does not have the form '%s'") % arg_name());

    const std::string key = raw_value.substr(0, pos);
    if (key.empty())
        throw cmdline::option_argument_value_error(
            F("--%s") % long_name(), raw_value, "Empty property name");

    const std::string value = raw_value.substr(pos + 1);
    if (value.empty())
        throw cmdline::option_argument_value_error(
            F("--%s") % long_name(), raw_value, "Empty value");
}


/// Returns the property option in a key/value pair form.
///
/// \param raw_value The argument provided by the user.
///
/// \return raw_value The key/value pair representation of the property.
///
/// \pre validate(raw_value) must be true.
cmdline::property_option::option_type
cmdline::property_option::convert(const std::string& raw_value)
{
    const std::string::size_type pos = raw_value.find('=');
    return std::make_pair(raw_value.substr(0, pos), raw_value.substr(pos + 1));
}


/// Constructs a string option with both a short and a long name.
///
/// \param short_name_ The short name for the option.
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ The name of the mandatory argument, for documentation
///     purposes.
/// \param default_value_ If not NULL, the default value for the mandatory
///     argument.
cmdline::string_option::string_option(const char short_name_,
                                      const char* long_name_,
                                      const char* description_,
                                      const char* arg_name_,
                                      const char* default_value_) :
    base_option(short_name_, long_name_, description_, arg_name_,
                default_value_)
{
}


/// Constructs a string option with a long name only.
///
/// \param long_name_ The long name for the option.
/// \param description_ A user-friendly description for the option.
/// \param arg_name_ The name of the mandatory argument, for documentation
///     purposes.
/// \param default_value_ If not NULL, the default value for the mandatory
///     argument.
cmdline::string_option::string_option(const char* long_name_,
                                      const char* description_,
                                      const char* arg_name_,
                                      const char* default_value_) :
    base_option(long_name_, description_, arg_name_, default_value_)
{
}


/// Does nothing; all string values are valid arguments to a string_option.
void
cmdline::string_option::validate(
    const std::string& /* raw_value */) const
{
    // Nothing to do.
}


/// Returns the string unmodified.
///
/// \param raw_value The argument provided by the user.
///
/// \return raw_value
///
/// \pre validate(raw_value) must be true.
std::string
cmdline::string_option::convert(const std::string& raw_value)
{
    return raw_value;
}
