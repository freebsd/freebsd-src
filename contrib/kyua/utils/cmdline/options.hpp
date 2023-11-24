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

/// \file utils/cmdline/options.hpp
/// Definitions of command-line options.

#if !defined(UTILS_CMDLINE_OPTIONS_HPP)
#define UTILS_CMDLINE_OPTIONS_HPP

#include "utils/cmdline/options_fwd.hpp"

#include <string>
#include <utility>
#include <vector>

#include "utils/fs/path_fwd.hpp"

namespace utils {
namespace cmdline {


/// Type-less base option class.
///
/// This abstract class provides the most generic representation of options.  It
/// allows defining options with both short and long names, with and without
/// arguments and with and without optional values.  These are all the possible
/// combinations supported by the getopt_long(3) function, on which this is
/// built.
///
/// The internal values (e.g. the default value) of a generic option are all
/// represented as strings.  However, from the caller's perspective, this is
/// suboptimal.  Hence why this class must be specialized: the subclasses
/// provide type-specific accessors and provide automatic validation of the
/// types (e.g. a string '3foo' is not passed to an integer option).
///
/// Given that subclasses are used through templatized code, they must provide:
///
/// <ul>
///     <li>A public option_type typedef that defines the type of the
///     option.</li>
///
///     <li>A convert() method that takes a string and converts it to
///     option_type.  The string can be assumed to be convertible to the
///     destination type.  Should not raise exceptions.</li>
///
///     <li>A validate() method that matches the implementation of convert().
///     This method can throw option_argument_value_error if the string cannot
///     be converted appropriately.  If validate() does not throw, then
///     convert() must execute successfully.</li>
/// </ul>
///
/// TODO(jmmv): Many methods in this class are split into two parts: has_foo()
/// and foo(), the former to query if the foo is available and the latter to get
/// the foo.  It'd be very nice if we'd use something similar Boost.Optional to
/// simplify this interface altogether.
class base_option {
    /// Short name of the option; 0 to indicate that none is available.
    char _short_name;

    /// Long name of the option.
    std::string _long_name;

    /// Textual description of the purpose of the option.
    std::string _description;

    /// Descriptive name of the required argument; empty if not allowed.
    std::string _arg_name;

    /// Whether the option has a default value or not.
    ///
    /// \todo We should probably be using the optional class here.
    bool _has_default_value;

    /// If _has_default_value is true, the default value.
    std::string _default_value;

public:
    base_option(const char, const char*, const char*, const char* = NULL,
                const char* = NULL);
    base_option(const char*, const char*, const char* = NULL,
                const char* = NULL);
    virtual ~base_option(void);

    bool has_short_name(void) const;
    char short_name(void) const;
    const std::string& long_name(void) const;
    const std::string& description(void) const;

    bool needs_arg(void) const;
    const std::string& arg_name(void) const;

    bool has_default_value(void) const;
    const std::string& default_value(void) const;

    std::string format_short_name(void) const;
    std::string format_long_name(void) const;

    virtual void validate(const std::string&) const;
};


/// Definition of a boolean option.
///
/// A boolean option can be specified once in the command line, at which point
/// is set to true.  Such an option cannot carry optional arguments.
class bool_option : public base_option {
public:
    bool_option(const char, const char*, const char*);
    bool_option(const char*, const char*);
    virtual ~bool_option(void) {}

    /// The data type of this option.
    typedef bool option_type;
};


/// Definition of an integer option.
class int_option : public base_option {
public:
    int_option(const char, const char*, const char*, const char*,
               const char* = NULL);
    int_option(const char*, const char*, const char*, const char* = NULL);
    virtual ~int_option(void) {}

    /// The data type of this option.
    typedef int option_type;

    virtual void validate(const std::string& str) const;
    static int convert(const std::string& str);
};


/// Definition of a comma-separated list of strings.
class list_option : public base_option {
public:
    list_option(const char, const char*, const char*, const char*,
                const char* = NULL);
    list_option(const char*, const char*, const char*, const char* = NULL);
    virtual ~list_option(void) {}

    /// The data type of this option.
    typedef std::vector< std::string > option_type;

    virtual void validate(const std::string&) const;
    static option_type convert(const std::string&);
};


/// Definition of an option representing a path.
///
/// The path pointed to by the option may not exist, but it must be
/// syntactically valid.
class path_option : public base_option {
public:
    path_option(const char, const char*, const char*, const char*,
                const char* = NULL);
    path_option(const char*, const char*, const char*, const char* = NULL);
    virtual ~path_option(void) {}

    /// The data type of this option.
    typedef utils::fs::path option_type;

    virtual void validate(const std::string&) const;
    static utils::fs::path convert(const std::string&);
};


/// Definition of a property option.
///
/// A property option is an option whose required arguments are of the form
/// 'name=value'.  Both components of the property are treated as free-form
/// non-empty strings; any other validation must happen on the caller side.
///
/// \todo Would be nice if the delimiter was parametrizable.  With the current
///     parser interface (convert() being a static method), the only way to do
///     this would be to templatize this class.
class property_option : public base_option {
public:
    property_option(const char, const char*, const char*, const char*);
    property_option(const char*, const char*, const char*);
    virtual ~property_option(void) {}

    /// The data type of this option.
    typedef std::pair< std::string, std::string > option_type;

    virtual void validate(const std::string& str) const;
    static option_type convert(const std::string& str);
};


/// Definition of a free-form string option.
///
/// This class provides no restrictions on the argument passed to the option.
class string_option : public base_option {
public:
    string_option(const char, const char*, const char*, const char*,
                  const char* = NULL);
    string_option(const char*, const char*, const char*, const char* = NULL);
    virtual ~string_option(void) {}

    /// The data type of this option.
    typedef std::string option_type;

    virtual void validate(const std::string& str) const;
    static std::string convert(const std::string& str);
};


}  // namespace cmdline
}  // namespace utils

#endif  // !defined(UTILS_CMDLINE_OPTIONS_HPP)
