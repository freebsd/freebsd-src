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

#include "utils/logging/operations.hpp"

extern "C" {
#include <unistd.h>
}

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/stream.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;

using utils::none;
using utils::optional;


/// The general idea for the application-wide logging goes like this:
///
/// 1. The application starts.  Logging is initialized to capture _all_ log
/// messages into memory regardless of their level by issuing a call to the
/// set_inmemory() function.
///
/// 2. The application offers the user a way to select the logging level and a
/// file into which to store the log.
///
/// 3. The application calls set_persistency providing a new log level and a log
/// file.  This must be done as early as possible, to minimize the chances of an
/// early crash not capturing any logs.
///
/// 4. At this point, any log messages stored into memory are flushed to disk
/// respecting the provided log level.
///
/// 5. The internal state of the logging module is updated to only capture
/// messages that are of the provided log level (or below) and is configured to
/// directly send messages to disk.
///
/// 6. The user may choose to call set_inmemory() again at a later stage, which
/// will cause the log to be flushed and messages to be recorded in memory
/// again.  This is useful in case the logs are being sent to either stdout or
/// stderr and the process forks and wants to keep those child channels
/// unpolluted.
///
/// The call to set_inmemory() should only be performed by the user-facing
/// application.  Tests should skip this call so that the logging messages go to
/// stderr by default, thus generating a useful log to debug the tests.


namespace {


/// Constant string to strftime to format timestamps.
static const char* timestamp_format = "%Y%m%d-%H%M%S";


/// Mutable global state.
struct global_state {
    /// Current log level.
    logging::level log_level;

    /// Indicates whether set_persistency() will be called automatically or not.
    bool auto_set_persistency;

    /// First time recorded by the logging module.
    optional< datetime::timestamp > first_timestamp;

    /// In-memory record of log entries before persistency is enabled.
    std::vector< std::pair< logging::level, std::string > > backlog;

    /// Stream to the currently open log file.
    std::unique_ptr< std::ostream > logfile;

    global_state() :
        log_level(logging::level_debug),
        auto_set_persistency(true)
    {
    }
};


/// Single instance of the mutable global state.
///
/// Note that this is a raw pointer that we intentionally leak.  We must do
/// this, instead of making all of the singleton's members static values,
/// because we want other destructors in the program to be able to log critical
/// conditions.  If we use complex types in this translation unit, they may be
/// destroyed before the logging methods in the destructors get a chance to run
/// thus resulting in a premature crash.  By using a plain pointer, we ensure
/// this state never gets cleaned up.
static struct global_state* globals_singleton = NULL;


/// Gets the singleton instance of global_state.
///
/// \return A pointer to the unique global_state instance.
static struct global_state*
get_globals(void)
{
    if (globals_singleton == NULL) {
        globals_singleton = new global_state();
    }
    return globals_singleton;
}


/// Converts a level to a printable character.
///
/// \param level The level to convert.
///
/// \return The printable character, to be used in log messages.
static char
level_to_char(const logging::level level)
{
    switch (level) {
    case logging::level_error: return 'E';
    case logging::level_warning: return 'W';
    case logging::level_info: return 'I';
    case logging::level_debug: return 'D';
    default: UNREACHABLE;
    }
}


}  // anonymous namespace


/// Generates a standard log name.
///
/// This always adds the same timestamp to the log name for a particular run.
/// Also, the timestamp added to the file name corresponds to the first
/// timestamp recorded by the module; it does not necessarily contain the
/// current value of "now".
///
/// \param logdir The path to the directory in which to place the log.
/// \param progname The name of the program that is generating the log.
///
/// \return A string representation of the log name based on \p logdir and
///     \p progname.
fs::path
logging::generate_log_name(const fs::path& logdir, const std::string& progname)
{
    struct global_state* globals = get_globals();

    if (!globals->first_timestamp)
        globals->first_timestamp = datetime::timestamp::now();
    // Update kyua(1) if you change the name format.
    return logdir / (F("%s.%s.log") % progname %
                     globals->first_timestamp.get().strftime(timestamp_format));
}


/// Logs an entry to the log file.
///
/// If the log is not yet set to persistent mode, the entry is recorded in the
/// in-memory backlog.  Otherwise, it is just written to disk.
///
/// \param message_level The level of the entry.
/// \param file The file from which the log message is generated.
/// \param line The line from which the log message is generated.
/// \param user_message The raw message to store.
void
logging::log(const level message_level, const char* file, const int line,
             const std::string& user_message)
{
    struct global_state* globals = get_globals();

    const datetime::timestamp now = datetime::timestamp::now();
    if (!globals->first_timestamp)
        globals->first_timestamp = now;

    if (globals->auto_set_persistency) {
        // These values are hardcoded here for testing purposes.  The
        // application should call set_inmemory() by itself during
        // initialization to avoid this, so that it has explicit control on how
        // the call to set_persistency() happens.
        set_persistency("debug", fs::path("/dev/stderr"));
        globals->auto_set_persistency = false;
    }

    if (message_level > globals->log_level)
        return;

    // Update doc/troubleshooting.texi if you change the log format.
    const std::string message = F("%s %s %s %s:%s: %s") %
        now.strftime(timestamp_format) % level_to_char(message_level) %
        ::getpid() % file % line % user_message;
    if (globals->logfile.get() == NULL)
        globals->backlog.push_back(std::make_pair(message_level, message));
    else {
        INV(globals->backlog.empty());
        (*globals->logfile) << message << '\n';
        globals->logfile->flush();
    }
}


/// Sets the logging to record messages in memory for later flushing.
///
/// Can be called after set_persistency to flush logs and set recording to be
/// in-memory again.
void
logging::set_inmemory(void)
{
    struct global_state* globals = get_globals();

    globals->auto_set_persistency = false;

    if (globals->logfile.get() != NULL) {
        INV(globals->backlog.empty());
        globals->logfile->flush();
        globals->logfile.reset();
    }
}


/// Makes the log persistent.
///
/// Calling this function flushes the in-memory log, if any, to disk and sets
/// the logging module to send log entries to disk from this point onwards.
/// There is no way back, and the caller program should execute this function as
/// early as possible to ensure that a crash at startup does not discard too
/// many useful log entries.
///
/// Any log entries above the provided new_level are discarded.
///
/// \param new_level The new log level.
/// \param path The file to write the logs to.
///
/// \throw std::range_error If the given log level is invalid.
/// \throw std::runtime_error If the given file cannot be created.
void
logging::set_persistency(const std::string& new_level, const fs::path& path)
{
    struct global_state* globals = get_globals();

    globals->auto_set_persistency = false;

    PRE(globals->logfile.get() == NULL);

    // Update doc/troubleshooting.info if you change the log levels.
    if (new_level == "debug")
        globals->log_level = level_debug;
    else if (new_level == "error")
        globals->log_level = level_error;
    else if (new_level == "info")
        globals->log_level = level_info;
    else if (new_level == "warning")
        globals->log_level = level_warning;
    else
        throw std::range_error(F("Unrecognized log level '%s'") % new_level);

    try {
        globals->logfile = utils::open_ostream(path);
    } catch (const std::runtime_error& unused_error) {
        throw std::runtime_error(F("Failed to create log file %s") % path);
    }

    for (std::vector< std::pair< logging::level, std::string > >::const_iterator
         iter = globals->backlog.begin(); iter != globals->backlog.end();
         ++iter) {
        if ((*iter).first <= globals->log_level)
            (*globals->logfile) << (*iter).second << '\n';
    }
    globals->logfile->flush();
    globals->backlog.clear();
}
