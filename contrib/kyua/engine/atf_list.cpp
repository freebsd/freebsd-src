// Copyright 2015 The Kyua Authors.
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

#include "engine/atf_list.hpp"

#include <fstream>
#include <string>
#include <utility>

#include "engine/exceptions.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "utils/config/exceptions.hpp"
#include "utils/format/macros.hpp"

namespace config = utils::config;
namespace fs = utils::fs;


namespace {


/// Splits a property line of the form "name: word1 [... wordN]".
///
/// \param line The line to parse.
///
/// \return A (property_name, property_value) pair.
///
/// \throw format_error If the value of line is invalid.
static std::pair< std::string, std::string >
split_prop_line(const std::string& line)
{
    const std::string::size_type pos = line.find(": ");
    if (pos == std::string::npos)
        throw engine::format_error("Invalid property line; expecting line of "
                                   "the form 'name: value'");
    return std::make_pair(line.substr(0, pos), line.substr(pos + 2));
}


/// Parses a set of consecutive property lines.
///
/// Processing stops when an empty line or the end of file is reached.  None of
/// these conditions indicate errors.
///
/// \param input The stream to read the lines from.
///
/// \return The parsed property lines.
///
/// throw format_error If the input stream has an invalid format.
static model::properties_map
parse_properties(std::istream& input)
{
    model::properties_map properties;

    std::string line;
    while (std::getline(input, line).good() && !line.empty()) {
        const std::pair< std::string, std::string > property = split_prop_line(
            line);
        if (properties.find(property.first) != properties.end())
            throw engine::format_error("Duplicate value for property " +
                                       property.first);
        properties.insert(property);
    }

    return properties;
}


}  // anonymous namespace


/// Parses the metadata of an ATF test case.
///
/// \param props The properties (name/value string pairs) as provided by the
///     ATF test program.
///
/// \return A parsed metadata object.
///
/// \throw engine::format_error If the syntax of any of the properties is
///     invalid.
model::metadata
engine::parse_atf_metadata(const model::properties_map& props)
{
    model::metadata_builder mdbuilder;

    try {
        for (model::properties_map::const_iterator iter = props.begin();
             iter != props.end(); iter++) {
            const std::string& name = (*iter).first;
            const std::string& value = (*iter).second;

            if (name == "descr") {
                mdbuilder.set_string("description", value);
            } else if (name == "has.cleanup") {
                mdbuilder.set_string("has_cleanup", value);
            } else if (name == "require.arch") {
                mdbuilder.set_string("allowed_architectures", value);
            } else if (name == "execenv") {
                mdbuilder.set_string("execenv", value);
            } else if (name == "execenv.jail.params") {
                mdbuilder.set_string("execenv_jail_params", value);
            } else if (name == "is.exclusive") {
                mdbuilder.set_string("is_exclusive", value);
            } else if (name == "require.config") {
                mdbuilder.set_string("required_configs", value);
            } else if (name == "require.diskspace") {
                mdbuilder.set_string("required_disk_space", value);
            } else if (name == "require.files") {
                mdbuilder.set_string("required_files", value);
            } else if (name == "require.kmods") {
                mdbuilder.set_string("required_kmods", value);
            } else if (name == "require.machine") {
                mdbuilder.set_string("allowed_platforms", value);
            } else if (name == "require.memory") {
                mdbuilder.set_string("required_memory", value);
            } else if (name == "require.progs") {
                mdbuilder.set_string("required_programs", value);
            } else if (name == "require.user") {
                mdbuilder.set_string("required_user", value);
            } else if (name == "timeout") {
                mdbuilder.set_string("timeout", value);
            } else if (name.length() > 2 && name.substr(0, 2) == "X-") {
                mdbuilder.add_custom(name.substr(2), value);
            } else {
                throw engine::format_error(F("Unknown test case metadata "
                                             "property '%s'") % name);
            }
        }
    } catch (const config::error& e) {
        throw engine::format_error(e.what());
    }

    return mdbuilder.build();
}


/// Parses the ATF list of test cases from an open stream.
///
/// \param input The stream to read from.
///
/// \return The collection of parsed test cases.
///
/// \throw format_error If there is any problem in the input data.
model::test_cases_map
engine::parse_atf_list(std::istream& input)
{
    std::string line;

    std::getline(input, line);
    if (line != "Content-Type: application/X-atf-tp; version=\"1\""
        || !input.good())
        throw format_error(F("Invalid header for test case list; expecting "
                             "Content-Type for application/X-atf-tp version 1, "
                             "got '%s'") % line);

    std::getline(input, line);
    if (!line.empty() || !input.good())
        throw format_error(F("Invalid header for test case list; expecting "
                             "a blank line, got '%s'") % line);

    model::test_cases_map_builder test_cases_builder;
    while (std::getline(input, line).good()) {
        const std::pair< std::string, std::string > ident = split_prop_line(
            line);
        if (ident.first != "ident" or ident.second.empty())
            throw format_error("Invalid test case definition; must be "
                               "preceeded by the identifier");

        const model::properties_map props = parse_properties(input);
        test_cases_builder.add(ident.second, parse_atf_metadata(props));
    }
    const model::test_cases_map test_cases = test_cases_builder.build();
    if (test_cases.empty()) {
        // The scheduler interface also checks for the presence of at least one
        // test case.  However, because the atf format itself requires one test
        // case to be always present, we check for this condition here as well.
        throw format_error("No test cases");
    }
    return test_cases;
}
