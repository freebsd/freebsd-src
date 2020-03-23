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

#include "utils/cmdline/parser.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

extern "C" {
#include <getopt.h>
}

#include <cstdlib>
#include <cstring>
#include <limits>

#include "utils/auto_array.ipp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/format/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;

namespace {


/// Auxiliary data to call getopt_long(3).
struct getopt_data : utils::noncopyable {
    /// Plain-text representation of the short options.
    ///
    /// This string follows the syntax expected by getopt_long(3) in the
    /// argument to describe the short options.
    std::string short_options;

    /// Representation of the long options as expected by getopt_long(3).
    utils::auto_array< ::option > long_options;

    /// Auto-generated identifiers to be able to parse long options.
    std::map< int, const cmdline::base_option* > ids;
};


/// Converts a cmdline::options_vector to a getopt_data.
///
/// \param options The high-level definition of the options.
/// \param [out] data An object containing the necessary data to call
///     getopt_long(3) and interpret its results.
static void
options_to_getopt_data(const cmdline::options_vector& options,
                       getopt_data& data)
{
    data.short_options.clear();
    data.long_options.reset(new ::option[options.size() + 1]);

    int cur_id = 512;

    for (cmdline::options_vector::size_type i = 0; i < options.size(); i++) {
        const cmdline::base_option* option = options[i];
        ::option& long_option = data.long_options[i];

        long_option.name = option->long_name().c_str();
        if (option->needs_arg())
            long_option.has_arg = required_argument;
        else
            long_option.has_arg = no_argument;

        int id = -1;
        if (option->has_short_name()) {
            data.short_options += option->short_name();
            if (option->needs_arg())
                data.short_options += ':';
            id = option->short_name();
        } else {
            id = cur_id++;
        }
        long_option.flag = NULL;
        long_option.val = id;
        data.ids[id] = option;
    }

    ::option& last_long_option = data.long_options[options.size()];
    last_long_option.name = NULL;
    last_long_option.has_arg = 0;
    last_long_option.flag = NULL;
    last_long_option.val = 0;
}


/// Converts an argc/argv pair to an args_vector.
///
/// \param argc The value of argc as passed to main().
/// \param argv The value of argv as passed to main().
///
/// \return An args_vector with the same contents of argc/argv.
static cmdline::args_vector
argv_to_vector(int argc, const char* const argv[])
{
    PRE(argv[argc] == NULL);
    cmdline::args_vector args;
    for (int i = 0; i < argc; i++)
        args.push_back(argv[i]);
    return args;
}


/// Creates a mutable version of argv.
///
/// \param argc The value of argc as passed to main().
/// \param argv The value of argv as passed to main().
///
/// \return A new argv, with mutable buffers.  The returned array must be
/// released using the free_mutable_argv() function.
static char**
make_mutable_argv(const int argc, const char* const* argv)
{
    char** mutable_argv = new char*[argc + 1];
    for (int i = 0; i < argc; i++)
        mutable_argv[i] = ::strdup(argv[i]);
    mutable_argv[argc] = NULL;
    return mutable_argv;
}


/// Releases the object returned by make_mutable_argv().
///
/// \param argv A dynamically-allocated argv as returned by make_mutable_argv().
static void
free_mutable_argv(char** argv)
{
    char** ptr = argv;
    while (*ptr != NULL) {
        ::free(*ptr);
        ptr++;
    }
    delete [] argv;
}


/// Finds the name of the offending option after a getopt_long error.
///
/// \param data Our internal getopt data used for the call to getopt_long.
/// \param getopt_optopt The value of getopt(3)'s optopt after the error.
/// \param argv The argv passed to getopt_long.
/// \param getopt_optind The value of getopt(3)'s optind after the error.
///
/// \return A fully-specified option name (i.e. an option name prefixed by
///     either '-' or '--').
static std::string
find_option_name(const getopt_data& data, const int getopt_optopt,
                 char** argv, const int getopt_optind)
{
    PRE(getopt_optopt >= 0);

    if (getopt_optopt == 0) {
        return argv[getopt_optind - 1];
    } else if (getopt_optopt < std::numeric_limits< char >::max()) {
        INV(getopt_optopt > 0);
        const char ch = static_cast< char >(getopt_optopt);
        return F("-%s") % ch;
    } else {
        for (const ::option* opt = &data.long_options[0]; opt->name != NULL;
             opt++) {
            if (opt->val == getopt_optopt)
                return F("--%s") % opt->name;
        }
        UNREACHABLE;
    }
}


}  // anonymous namespace


/// Constructs a new parsed_cmdline.
///
/// Use the cmdline::parse() free functions to construct.
///
/// \param option_values_ A mapping of long option names to values.  This
///     contains a representation of the options provided by the user.  Note
///     that each value is actually a collection values: a user may specify a
///     flag multiple times, and depending on the case we want to honor one or
///     the other.  For those options that support no argument, the argument
///     value is the empty string.
/// \param arguments_ The list of non-option arguments in the command line.
cmdline::parsed_cmdline::parsed_cmdline(
    const std::map< std::string, std::vector< std::string > >& option_values_,
    const cmdline::args_vector& arguments_) :
    _option_values(option_values_),
    _arguments(arguments_)
{
}


/// Checks if the given option has been given in the command line.
///
/// \param name The long option name to check for presence.
///
/// \return True if the option has been given; false otherwise.
bool
cmdline::parsed_cmdline::has_option(const std::string& name) const
{
    return _option_values.find(name) != _option_values.end();
}


/// Gets the raw value of an option.
///
/// The raw value of an option is a collection of strings that represent all the
/// values passed to the option on the command line.  It is up to the consumer
/// if he wants to honor only the last value or all of them.
///
/// The caller has to use get_option() instead; this function is internal.
///
/// \pre has_option(name) must be true.
///
/// \param name The option to query.
///
/// \return The value of the option as a plain string.
const std::vector< std::string >&
cmdline::parsed_cmdline::get_option_raw(const std::string& name) const
{
    std::map< std::string, std::vector< std::string > >::const_iterator iter =
        _option_values.find(name);
    INV_MSG(iter != _option_values.end(), F("Undefined option --%s") % name);
    return (*iter).second;
}


/// Returns the non-option arguments found in the command line.
///
/// \return The arguments, if any.
const cmdline::args_vector&
cmdline::parsed_cmdline::arguments(void) const
{
    return _arguments;
}


/// Parses a command line.
///
/// \param args The command line to parse, broken down by words.
/// \param options The description of the supported options.
///
/// \return The parsed command line.
///
/// \pre args[0] must be the program or command name.
///
/// \throw cmdline::error See the description of parse(argc, argv, options) for
///     more details on the raised errors.
cmdline::parsed_cmdline
cmdline::parse(const cmdline::args_vector& args,
               const cmdline::options_vector& options)
{
    PRE_MSG(args.size() >= 1, "No progname or command name found");

    utils::auto_array< const char* > argv(new const char*[args.size() + 1]);
    for (args_vector::size_type i = 0; i < args.size(); i++)
        argv[i] = args[i].c_str();
    argv[args.size()] = NULL;
    return parse(static_cast< int >(args.size()), argv.get(), options);
}


/// Parses a command line.
///
/// \param argc The number of arguments in argv, without counting the
///     terminating NULL.
/// \param argv The arguments to parse.  The array is NULL-terminated.
/// \param options The description of the supported options.
///
/// \return The parsed command line.
///
/// \pre args[0] must be the program or command name.
///
/// \throw cmdline::missing_option_argument_error If the user specified an
///     option that requires an argument, but no argument was provided.
/// \throw cmdline::unknown_option_error If the user specified an unknown
///     option (i.e. an option not defined in options).
/// \throw cmdline::option_argument_value_error If the user passed an invalid
///     argument to a supported option.
cmdline::parsed_cmdline
cmdline::parse(const int argc, const char* const* argv,
               const cmdline::options_vector& options)
{
    PRE_MSG(argc >= 1, "No progname or command name found");

    getopt_data data;
    options_to_getopt_data(options, data);

    std::map< std::string, std::vector< std::string > > option_values;

    for (cmdline::options_vector::const_iterator iter = options.begin();
         iter != options.end(); iter++) {
        const cmdline::base_option* option = *iter;
        if (option->needs_arg() && option->has_default_value())
            option_values[option->long_name()].push_back(
                option->default_value());
    }

    args_vector args;

    int mutable_argc = argc;
    char** mutable_argv = make_mutable_argv(argc, argv);
    const int old_opterr = ::opterr;
    try {
        int ch;

        ::opterr = 0;

        while ((ch = ::getopt_long(mutable_argc, mutable_argv,
                                   ("+:" + data.short_options).c_str(),
                                   data.long_options.get(), NULL)) != -1) {
            if (ch == ':' ) {
                const std::string name = find_option_name(
                    data, ::optopt, mutable_argv, ::optind);
                throw cmdline::missing_option_argument_error(name);
            } else if (ch == '?') {
                const std::string name = find_option_name(
                    data, ::optopt, mutable_argv, ::optind);
                throw cmdline::unknown_option_error(name);
            }

            const std::map< int, const cmdline::base_option* >::const_iterator
                id = data.ids.find(ch);
            INV(id != data.ids.end());
            const cmdline::base_option* option = (*id).second;

            if (option->needs_arg()) {
                if (::optarg != NULL) {
                    option->validate(::optarg);
                    option_values[option->long_name()].push_back(::optarg);
                } else
                    INV(option->has_default_value());
            } else {
                option_values[option->long_name()].push_back("");
            }
        }
        args = argv_to_vector(mutable_argc - optind, mutable_argv + optind);

        ::opterr = old_opterr;
        ::optind = GETOPT_OPTIND_RESET_VALUE;
#if defined(HAVE_GETOPT_WITH_OPTRESET)
        ::optreset = 1;
#endif
    } catch (...) {
        free_mutable_argv(mutable_argv);
        ::opterr = old_opterr;
        ::optind = GETOPT_OPTIND_RESET_VALUE;
#if defined(HAVE_GETOPT_WITH_OPTRESET)
        ::optreset = 1;
#endif
        throw;
    }
    free_mutable_argv(mutable_argv);

    return parsed_cmdline(option_values, args);
}
