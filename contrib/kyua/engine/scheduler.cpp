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

#include "engine/scheduler.hpp"

extern "C" {
#include <unistd.h>
}

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "engine/config.hpp"
#include "engine/debugger.hpp"
#include "engine/exceptions.hpp"
#include "engine/execenv/execenv.hpp"
#include "engine/requirements.hpp"
#include "model/context.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/directory.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/executor.ipp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/stacktrace.hpp"
#include "utils/stream.hpp"
#include "utils/text/operations.ipp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace execenv = engine::execenv;
namespace executor = utils::process::executor;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace scheduler = engine::scheduler;
namespace text = utils::text;

using utils::none;
using utils::optional;


/// Timeout for the test case cleanup operation.
///
/// TODO(jmmv): This is here only for testing purposes.  Maybe we should expose
/// this setting as part of the user_config.
datetime::delta scheduler::cleanup_timeout(300, 0);


/// Timeout for the test case execenv cleanup operation.
datetime::delta scheduler::execenv_cleanup_timeout(300, 0);


/// Timeout for the test case listing operation.
///
/// TODO(jmmv): This is here only for testing purposes.  Maybe we should expose
/// this setting as part of the user_config.
datetime::delta scheduler::list_timeout(300, 0);


namespace {


/// Magic exit status to indicate that the test case was probably skipped.
///
/// The test case was only skipped if and only if we return this exit code and
/// we find the skipped_cookie file on disk.
static const int exit_skipped = 84;


/// Text file containing the skip reason for the test case.
///
/// This will only be present within unique_work_directory if the test case
/// exited with the exit_skipped code.  However, there is no guarantee that the
/// file is there (say if the test really decided to exit with code exit_skipped
/// on its own).
static const char* skipped_cookie = "skipped.txt";


/// Mapping of interface names to interface definitions.
typedef std::map< std::string, std::shared_ptr< scheduler::interface > >
    interfaces_map;


/// Mapping of interface names to interface definitions.
///
/// Use register_interface() to add an entry to this global table.
static interfaces_map interfaces;


/// Scans the contents of a directory and appends the file listing to a file.
///
/// \param dir_path The directory to scan.
/// \param output_file The file to which to append the listing.
///
/// \throw engine::error If there are problems listing the files.
static void
append_files_listing(const fs::path& dir_path, const fs::path& output_file)
{
    std::ofstream output(output_file.c_str(), std::ios::app);
    if (!output)
        throw engine::error(F("Failed to open output file %s for append")
                            % output_file);
    try {
        std::set < std::string > names;

        const fs::directory dir(dir_path);
        for (fs::directory::const_iterator iter = dir.begin();
             iter != dir.end(); ++iter) {
            if (iter->name != "." && iter->name != "..")
                names.insert(iter->name);
        }

        if (!names.empty()) {
            output << "Files left in work directory after failure: "
                   << text::join(names, ", ") << '\n';
        }
    } catch (const fs::error& e) {
        throw engine::error(F("Cannot append files listing to %s: %s")
                            % output_file % e.what());
    }
}


/// Maintenance data held while a test is being executed.
///
/// This data structure exists from the moment when a test is executed via
/// scheduler::spawn_test() or scheduler::impl::spawn_cleanup() to when it is
/// cleaned up with result_handle::cleanup().
///
/// This is a base data type intended to be extended for the test and cleanup
/// cases so that each contains only the relevant data.
struct exec_data : utils::noncopyable {
    /// Test program data for this test case.
    const model::test_program_ptr test_program;

    /// Name of the test case.
    const std::string test_case_name;

    /// Constructor.
    ///
    /// \param test_program_ Test program data for this test case.
    /// \param test_case_name_ Name of the test case.
    exec_data(const model::test_program_ptr test_program_,
              const std::string& test_case_name_) :
        test_program(test_program_), test_case_name(test_case_name_)
    {
    }

    /// Destructor.
    virtual ~exec_data(void)
    {
    }
};


/// Maintenance data held while a test is being executed.
struct test_exec_data : public exec_data {
    /// Test program-specific execution interface.
    const std::shared_ptr< scheduler::interface > interface;

    /// User configuration passed to the execution of the test.  We need this
    /// here to recover it later when chaining the execution of a cleanup
    /// routine (if any).
    const config::tree user_config;

    /// Whether this test case still needs to have its cleanup routine executed.
    ///
    /// This is set externally when the cleanup routine is actually invoked to
    /// denote that no further attempts shall be made at cleaning this up.
    bool needs_cleanup;

    /// Whether this test case still needs to have its execenv cleanup executed.
    ///
    /// This is set externally when the cleanup routine is actually invoked to
    /// denote that no further attempts shall be made at cleaning this up.
    bool needs_execenv_cleanup;

    /// Original PID of the test case subprocess.
    ///
    /// This is used for the cleanup upon termination by a signal, to reap the
    /// leftovers and form missing exit_handle.
    pid_t pid;

    /// The exit_handle for this test once it has completed.
    ///
    /// This is set externally when the test case has finished, as we need this
    /// information to invoke the followup cleanup routine in the right context,
    /// as indicated by needs_cleanup.
    optional< executor::exit_handle > exit_handle;

    /// Constructor.
    ///
    /// \param test_program_ Test program data for this test case.
    /// \param test_case_name_ Name of the test case.
    /// \param interface_ Test program-specific execution interface.
    /// \param user_config_ User configuration passed to the test.
    test_exec_data(const model::test_program_ptr test_program_,
                   const std::string& test_case_name_,
                   const std::shared_ptr< scheduler::interface > interface_,
                   const config::tree& user_config_,
                   const pid_t pid_) :
        exec_data(test_program_, test_case_name_),
        interface(interface_), user_config(user_config_), pid(pid_)
    {
        const model::test_case& test_case = test_program->find(test_case_name);
        needs_cleanup = test_case.get_metadata().has_cleanup();
        needs_execenv_cleanup = test_case.get_metadata().has_execenv();
    }
};


/// Maintenance data held while a test cleanup routine is being executed.
///
/// Instances of this object are related to a previous test_exec_data, as
/// cleanup routines can only exist once the test has been run.
struct cleanup_exec_data : public exec_data {
    /// The exit handle of the test.  This is necessary so that we can return
    /// the correct exit_handle to the user of the scheduler.
    executor::exit_handle body_exit_handle;

    /// The final result of the test's body.  This is necessary to compute the
    /// right return value for a test with a cleanup routine: the body result is
    /// respected if it is a "bad" result; else the result of the cleanup
    /// routine is used if it has failed.
    model::test_result body_result;

    /// Constructor.
    ///
    /// \param test_program_ Test program data for this test case.
    /// \param test_case_name_ Name of the test case.
    /// \param body_exit_handle_ If not none, exit handle of the body
    ///     corresponding to the cleanup routine represented by this exec_data.
    /// \param body_result_ If not none, result of the body corresponding to the
    ///     cleanup routine represented by this exec_data.
    cleanup_exec_data(const model::test_program_ptr test_program_,
                      const std::string& test_case_name_,
                      const executor::exit_handle& body_exit_handle_,
                      const model::test_result& body_result_) :
        exec_data(test_program_, test_case_name_),
        body_exit_handle(body_exit_handle_), body_result(body_result_)
    {
    }
};


/// Maintenance data held while a test execenv cleanup is being executed.
///
/// Instances of this object are related to a previous test_exec_data, as
/// cleanup routines can only exist once the test has been run.
struct execenv_exec_data : public exec_data {
    /// The exit handle of the test.  This is necessary so that we can return
    /// the correct exit_handle to the user of the scheduler.
    executor::exit_handle body_exit_handle;

    /// The final result of the test's body.  This is necessary to compute the
    /// right return value for a test with a cleanup routine: the body result is
    /// respected if it is a "bad" result; else the result of the cleanup
    /// routine is used if it has failed.
    model::test_result body_result;

    /// Constructor.
    ///
    /// \param test_program_ Test program data for this test case.
    /// \param test_case_name_ Name of the test case.
    /// \param body_exit_handle_ If not none, exit handle of the body
    ///     corresponding to the cleanup routine represented by this exec_data.
    /// \param body_result_ If not none, result of the body corresponding to the
    ///     cleanup routine represented by this exec_data.
    execenv_exec_data(const model::test_program_ptr test_program_,
                              const std::string& test_case_name_,
                              const executor::exit_handle& body_exit_handle_,
                              const model::test_result& body_result_) :
        exec_data(test_program_, test_case_name_),
        body_exit_handle(body_exit_handle_), body_result(body_result_)
    {
    }
};


/// Shared pointer to exec_data.
///
/// We require this because we want exec_data to not be copyable, and thus we
/// cannot just store it in the map without move constructors.
typedef std::shared_ptr< exec_data > exec_data_ptr;


/// Mapping of active PIDs to their maintenance data.
typedef std::map< int, exec_data_ptr > exec_data_map;


/// Enforces a test program to hold an absolute path.
///
/// TODO(jmmv): This function (which is a pretty ugly hack) exists because we
/// want the interface hooks to receive a test_program as their argument.
/// However, those hooks run after the test program has been isolated, which
/// means that the current directory has changed since when the test_program
/// objects were created.  This causes the absolute_path() method of
/// test_program to return bogus values if the internal representation of their
/// path is relative.  We should fix somehow: maybe making the fs module grab
/// its "current_path" view at program startup time; or maybe by grabbing the
/// current path at test_program creation time; or maybe something else.
///
/// \param program The test program to modify.
///
/// \return A new test program whose internal paths are absolute.
static model::test_program
force_absolute_paths(const model::test_program program)
{
    const std::string& relative = program.relative_path().str();
    const std::string absolute = program.absolute_path().str();

    const std::string root = absolute.substr(
        0, absolute.length() - relative.length());

    return model::test_program(
        program.interface_name(),
        program.relative_path(), fs::path(root),
        program.test_suite_name(),
        program.get_metadata(), program.test_cases());
}


/// Functor to list the test cases of a test program.
class list_test_cases {
    /// Interface of the test program to execute.
    std::shared_ptr< scheduler::interface > _interface;

    /// Test program to execute.
    const model::test_program _test_program;

    /// User-provided configuration variables.
    const config::tree& _user_config;

public:
    /// Constructor.
    ///
    /// \param interface Interface of the test program to execute.
    /// \param test_program Test program to execute.
    /// \param user_config User-provided configuration variables.
    list_test_cases(
        const std::shared_ptr< scheduler::interface > interface,
        const model::test_program* test_program,
        const config::tree& user_config) :
        _interface(interface),
        _test_program(force_absolute_paths(*test_program)),
        _user_config(user_config)
    {
    }

    /// Body of the subprocess.
    void
    operator()(const fs::path& /* control_directory */)
    {
        const config::properties_map vars = scheduler::generate_config(
            _user_config, _test_program.test_suite_name());
        _interface->exec_list(_test_program, vars);
    }
};


/// Functor to execute a test program in a child process.
class run_test_program {
    /// Interface of the test program to execute.
    std::shared_ptr< scheduler::interface > _interface;

    /// Test program to execute.
    const model::test_program _test_program;

    /// Name of the test case to execute.
    const std::string& _test_case_name;

    /// User-provided configuration variables.
    const config::tree& _user_config;

    /// Verifies if the test case needs to be skipped or not.
    ///
    /// We could very well run this on the scheduler parent process before
    /// issuing the fork.  However, doing this here in the child process is
    /// better for two reasons: first, it allows us to continue using the simple
    /// spawn/wait abstraction of the scheduler; and, second, we parallelize the
    /// requirements checks among tests.
    ///
    /// \post If the test's preconditions are not met, the caller process is
    /// terminated with a special exit code and a "skipped cookie" is written to
    /// the disk with the reason for the failure.
    ///
    /// \param skipped_cookie_path File to create with the skip reason details
    ///     if this test is skipped.
    void
    do_requirements_check(const fs::path& skipped_cookie_path)
    {
        const model::test_case& test_case = _test_program.find(
            _test_case_name);

        const std::string skip_reason = engine::check_reqs(
            test_case.get_metadata(), _user_config,
            _test_program.test_suite_name(),
            fs::current_path());
        if (skip_reason.empty())
            return;

        std::ofstream output(skipped_cookie_path.c_str());
        if (!output) {
            std::perror((F("Failed to open %s for write") %
                         skipped_cookie_path).str().c_str());
            std::abort();
        }
        output << skip_reason;
        output.close();

        // Abruptly terminate the process.  We don't want to run any destructors
        // inherited from the parent process by mistake, which could, for
        // example, delete our own control files!
        ::_exit(exit_skipped);
    }

public:
    /// Constructor.
    ///
    /// \param interface Interface of the test program to execute.
    /// \param test_program Test program to execute.
    /// \param test_case_name Name of the test case to execute.
    /// \param user_config User-provided configuration variables.
    run_test_program(
        const std::shared_ptr< scheduler::interface > interface,
        const model::test_program_ptr test_program,
        const std::string& test_case_name,
        const config::tree& user_config) :
        _interface(interface),
        _test_program(force_absolute_paths(*test_program)),
        _test_case_name(test_case_name),
        _user_config(user_config)
    {
    }

    /// Body of the subprocess.
    ///
    /// \param control_directory The testcase directory where files will be
    ///     read from.
    void
    operator()(const fs::path& control_directory)
    {
        const model::test_case& test_case = _test_program.find(
            _test_case_name);
        if (test_case.fake_result())
            ::_exit(EXIT_SUCCESS);

        do_requirements_check(control_directory / skipped_cookie);

        const config::properties_map vars = scheduler::generate_config(
            _user_config, _test_program.test_suite_name());
        _interface->exec_test(_test_program, _test_case_name, vars,
                              control_directory);
    }
};


/// Functor to execute a test program in a child process.
class run_test_cleanup {
    /// Interface of the test program to execute.
    std::shared_ptr< scheduler::interface > _interface;

    /// Test program to execute.
    const model::test_program _test_program;

    /// Name of the test case to execute.
    const std::string& _test_case_name;

    /// User-provided configuration variables.
    const config::tree& _user_config;

public:
    /// Constructor.
    ///
    /// \param interface Interface of the test program to execute.
    /// \param test_program Test program to execute.
    /// \param test_case_name Name of the test case to execute.
    /// \param user_config User-provided configuration variables.
    run_test_cleanup(
        const std::shared_ptr< scheduler::interface > interface,
        const model::test_program_ptr test_program,
        const std::string& test_case_name,
        const config::tree& user_config) :
        _interface(interface),
        _test_program(force_absolute_paths(*test_program)),
        _test_case_name(test_case_name),
        _user_config(user_config)
    {
    }

    /// Body of the subprocess.
    ///
    /// \param control_directory The testcase directory where cleanup will be
    ///     run from.
    void
    operator()(const fs::path& control_directory)
    {
        const config::properties_map vars = scheduler::generate_config(
            _user_config, _test_program.test_suite_name());
        _interface->exec_cleanup(_test_program, _test_case_name, vars,
                                 control_directory);
    }
};


/// Functor to execute a test execenv cleanup in a child process.
class run_execenv_cleanup {
    /// Test program to execute.
    const model::test_program _test_program;

    /// Name of the test case to execute.
    const std::string& _test_case_name;

public:
    /// Constructor.
    ///
    /// \param test_program Test program to execute.
    /// \param test_case_name Name of the test case to execute.
    run_execenv_cleanup(
        const model::test_program_ptr test_program,
        const std::string& test_case_name) :
        _test_program(force_absolute_paths(*test_program)),
        _test_case_name(test_case_name)
    {
    }

    /// Body of the subprocess.
    ///
    /// \param control_directory The testcase directory where cleanup will be
    ///     run from.
    void
    operator()(const fs::path& /* control_directory */)
    {
        auto e = execenv::get(_test_program, _test_case_name);
        e->cleanup();
    }
};


/// Obtains the right scheduler interface for a given test program.
///
/// \param name The name of the interface of the test program.
///
/// \return An scheduler interface.
std::shared_ptr< scheduler::interface >
find_interface(const std::string& name)
{
    const interfaces_map::const_iterator iter = interfaces.find(name);
    PRE(interfaces.find(name) != interfaces.end());
    return (*iter).second;
}


}  // anonymous namespace


void
scheduler::interface::exec_cleanup(
    const model::test_program& /* test_program */,
    const std::string& /* test_case_name */,
    const config::properties_map& /* vars */,
    const utils::fs::path& /* control_directory */) const
{
    // Most test interfaces do not support standalone cleanup routines so
    // provide a default implementation that does nothing.
    UNREACHABLE_MSG("exec_cleanup not implemented for an interface that "
                    "supports standalone cleanup routines");
}


/// Internal implementation of a lazy_test_program.
struct engine::scheduler::lazy_test_program::impl : utils::noncopyable {
    /// Whether the test cases list has been yet loaded or not.
    bool _loaded;

    /// User configuration to pass to the test program list operation.
    config::tree _user_config;

    /// Scheduler context to use to load test cases.
    scheduler::scheduler_handle& _scheduler_handle;

    /// Constructor.
    ///
    /// \param user_config_ User configuration to pass to the test program list
    ///     operation.
    /// \param scheduler_handle_ Scheduler context to use when loading test
    ///     cases.
    impl(const config::tree& user_config_,
         scheduler::scheduler_handle& scheduler_handle_) :
        _loaded(false), _user_config(user_config_),
        _scheduler_handle(scheduler_handle_)
    {
    }
};


/// Constructs a new test program.
///
/// \param interface_name_ Name of the test program interface.
/// \param binary_ The name of the test program binary relative to root_.
/// \param root_ The root of the test suite containing the test program.
/// \param test_suite_name_ The name of the test suite this program belongs to.
/// \param md_ Metadata of the test program.
/// \param user_config_ User configuration to pass to the scheduler.
/// \param scheduler_handle_ Scheduler context to use to load test cases.
scheduler::lazy_test_program::lazy_test_program(
    const std::string& interface_name_,
    const fs::path& binary_,
    const fs::path& root_,
    const std::string& test_suite_name_,
    const model::metadata& md_,
    const config::tree& user_config_,
    scheduler::scheduler_handle& scheduler_handle_) :
    test_program(interface_name_, binary_, root_, test_suite_name_, md_,
                 model::test_cases_map()),
    _pimpl(new impl(user_config_, scheduler_handle_))
{
}


/// Gets or loads the list of test cases from the test program.
///
/// \return The list of test cases provided by the test program.
const model::test_cases_map&
scheduler::lazy_test_program::test_cases(void) const
{
    _pimpl->_scheduler_handle.check_interrupt();

    if (!_pimpl->_loaded) {
        const model::test_cases_map tcs = _pimpl->_scheduler_handle.list_tests(
            this, _pimpl->_user_config);

        // Due to the restrictions on when set_test_cases() may be called (as a
        // way to lazily initialize the test cases list before it is ever
        // returned), this cast is valid.
        const_cast< scheduler::lazy_test_program* >(this)->set_test_cases(tcs);

        _pimpl->_loaded = true;

        _pimpl->_scheduler_handle.check_interrupt();
    }

    INV(_pimpl->_loaded);
    return test_program::test_cases();
}


/// Internal implementation for the result_handle class.
struct engine::scheduler::result_handle::bimpl : utils::noncopyable {
    /// Generic executor exit handle for this result handle.
    executor::exit_handle generic;

    /// Mutable pointer to the corresponding scheduler state.
    ///
    /// This object references a member of the scheduler_handle that yielded
    /// this result_handle instance.  We need this direct access to clean up
    /// after ourselves when the result is destroyed.
    exec_data_map& all_exec_data;

    /// Constructor.
    ///
    /// \param generic_ Generic executor exit handle for this result handle.
    /// \param [in,out] all_exec_data_ Global object keeping track of all active
    ///     executions for an scheduler.  This is a pointer to a member of the
    ///     scheduler_handle object.
    bimpl(const executor::exit_handle generic_, exec_data_map& all_exec_data_) :
        generic(generic_), all_exec_data(all_exec_data_)
    {
    }

    /// Destructor.
    ~bimpl(void)
    {
        LD(F("Removing %s from all_exec_data") % generic.original_pid());
        all_exec_data.erase(generic.original_pid());
    }
};


/// Constructor.
///
/// \param pbimpl Constructed internal implementation.
scheduler::result_handle::result_handle(std::shared_ptr< bimpl > pbimpl) :
    _pbimpl(pbimpl)
{
}


/// Destructor.
scheduler::result_handle::~result_handle(void)
{
}


/// Cleans up the test case results.
///
/// This function should be called explicitly as it provides the means to
/// control any exceptions raised during cleanup.  Do not rely on the destructor
/// to clean things up.
///
/// \throw engine::error If the cleanup fails, especially due to the inability
///     to remove the work directory.
void
scheduler::result_handle::cleanup(void)
{
    _pbimpl->generic.cleanup();
}


/// Returns the original PID corresponding to this result.
///
/// \return An exec_handle.
int
scheduler::result_handle::original_pid(void) const
{
    return _pbimpl->generic.original_pid();
}


/// Returns the timestamp of when spawn_test was called.
///
/// \return A timestamp.
const datetime::timestamp&
scheduler::result_handle::start_time(void) const
{
    return _pbimpl->generic.start_time();
}


/// Returns the timestamp of when wait_any_test returned this object.
///
/// \return A timestamp.
const datetime::timestamp&
scheduler::result_handle::end_time(void) const
{
    return _pbimpl->generic.end_time();
}


/// Returns the path to the test-specific work directory.
///
/// This is guaranteed to be clear of files created by the scheduler.
///
/// \return The path to a directory that exists until cleanup() is called.
fs::path
scheduler::result_handle::work_directory(void) const
{
    return _pbimpl->generic.work_directory();
}


/// Returns the path to the test's stdout file.
///
/// \return The path to a file that exists until cleanup() is called.
const fs::path&
scheduler::result_handle::stdout_file(void) const
{
    return _pbimpl->generic.stdout_file();
}


/// Returns the path to the test's stderr file.
///
/// \return The path to a file that exists until cleanup() is called.
const fs::path&
scheduler::result_handle::stderr_file(void) const
{
    return _pbimpl->generic.stderr_file();
}


/// Internal implementation for the test_result_handle class.
struct engine::scheduler::test_result_handle::impl : utils::noncopyable {
    /// Test program data for this test case.
    model::test_program_ptr test_program;

    /// Name of the test case.
    std::string test_case_name;

    /// The actual result of the test execution.
    const model::test_result test_result;

    /// Constructor.
    ///
    /// \param test_program_ Test program data for this test case.
    /// \param test_case_name_ Name of the test case.
    /// \param test_result_ The actual result of the test execution.
    impl(const model::test_program_ptr test_program_,
         const std::string& test_case_name_,
         const model::test_result& test_result_) :
        test_program(test_program_),
        test_case_name(test_case_name_),
        test_result(test_result_)
    {
    }
};


/// Constructor.
///
/// \param pbimpl Constructed internal implementation for the base object.
/// \param pimpl Constructed internal implementation.
scheduler::test_result_handle::test_result_handle(
    std::shared_ptr< bimpl > pbimpl, std::shared_ptr< impl > pimpl) :
    result_handle(pbimpl), _pimpl(pimpl)
{
}


/// Destructor.
scheduler::test_result_handle::~test_result_handle(void)
{
}


/// Returns the test program that yielded this result.
///
/// \return A test program.
const model::test_program_ptr
scheduler::test_result_handle::test_program(void) const
{
    return _pimpl->test_program;
}


/// Returns the name of the test case that yielded this result.
///
/// \return A test case name
const std::string&
scheduler::test_result_handle::test_case_name(void) const
{
    return _pimpl->test_case_name;
}


/// Returns the actual result of the test execution.
///
/// \return A test result.
const model::test_result&
scheduler::test_result_handle::test_result(void) const
{
    return _pimpl->test_result;
}


/// Internal implementation for the scheduler_handle.
struct engine::scheduler::scheduler_handle::impl : utils::noncopyable {
    /// Generic executor instance encapsulated by this one.
    executor::executor_handle generic;

    /// Mapping of exec handles to the data required at run time.
    exec_data_map all_exec_data;

    /// Collection of test_exec_data objects.
    typedef std::vector< const test_exec_data* > test_exec_data_vector;

    /// Constructor.
    impl(void) : generic(executor::setup())
    {
    }

    /// Destructor.
    ///
    /// This runs any pending cleanup routines, which should only happen if the
    /// scheduler is abruptly terminated (aka if a signal is received).
    ~impl(void)
    {
        const test_exec_data_vector tests_data = tests_needing_cleanup();

        for (test_exec_data_vector::const_iterator iter = tests_data.begin();
             iter != tests_data.end(); ++iter) {
            const test_exec_data* test_data = *iter;

            try {
                sync_cleanup(test_data);
            } catch (const std::runtime_error& e) {
                LW(F("Failed to run cleanup routine for %s:%s on abrupt "
                     "termination")
                   % test_data->test_program->relative_path()
                   % test_data->test_case_name);
            }
        }

        const test_exec_data_vector td = tests_needing_execenv_cleanup();

        for (test_exec_data_vector::const_iterator iter = td.begin();
             iter != td.end(); ++iter) {
            const test_exec_data* test_data = *iter;

            try {
                sync_execenv_cleanup(test_data);
            } catch (const std::runtime_error& e) {
                LW(F("Failed to run execenv cleanup routine for %s:%s on abrupt "
                     "termination")
                   % test_data->test_program->relative_path()
                   % test_data->test_case_name);
            }
        }
    }

    /// Finds any pending exec_datas that correspond to tests needing cleanup.
    ///
    /// \return The collection of test_exec_data objects that have their
    /// needs_cleanup property set to true.
    test_exec_data_vector
    tests_needing_cleanup(void)
    {
        test_exec_data_vector tests_data;

        for (exec_data_map::const_iterator iter = all_exec_data.begin();
             iter != all_exec_data.end(); ++iter) {
            const exec_data_ptr data = (*iter).second;

            try {
                test_exec_data* test_data = &dynamic_cast< test_exec_data& >(
                    *data.get());
                if (test_data->needs_cleanup) {
                    tests_data.push_back(test_data);
                    test_data->needs_cleanup = false;
                    if (!test_data->exit_handle)
                        test_data->exit_handle = generic.reap(test_data->pid);
                }
            } catch (const std::bad_cast& e) {
                // Do nothing for cleanup_exec_data objects.
            }
        }

        return tests_data;
    }

    /// Finds any pending exec_datas that correspond to tests needing execenv
    /// cleanup.
    ///
    /// \return The collection of test_exec_data objects that have their
    /// specific execenv property set.
    test_exec_data_vector
    tests_needing_execenv_cleanup(void)
    {
        test_exec_data_vector tests_data;

        for (exec_data_map::const_iterator iter = all_exec_data.begin();
             iter != all_exec_data.end(); ++iter) {
            const exec_data_ptr data = (*iter).second;

            try {
                test_exec_data* test_data = &dynamic_cast< test_exec_data& >(
                    *data.get());
                if (test_data->needs_execenv_cleanup) {
                    tests_data.push_back(test_data);
                    test_data->needs_execenv_cleanup = false;
                    if (!test_data->exit_handle)
                        test_data->exit_handle = generic.reap(test_data->pid);
                }
            } catch (const std::bad_cast& e) {
                // Do nothing for other objects.
            }
        }

        return tests_data;
    }

    /// Cleans up a single test case synchronously.
    ///
    /// \param test_data The data of the previously executed test case to be
    ///     cleaned up.
    void
    sync_cleanup(const test_exec_data* test_data)
    {
        // The message in this result should never be seen by the user, but use
        // something reasonable just in case it leaks and we need to pinpoint
        // the call site.
        model::test_result result(model::test_result_broken,
                                  "Test case died abruptly");

        const executor::exec_handle cleanup_handle = spawn_cleanup(
            test_data->test_program, test_data->test_case_name,
            test_data->user_config, test_data->exit_handle.get(),
            result);
        generic.wait(cleanup_handle);
    }

    /// Forks and executes a test case cleanup routine asynchronously.
    ///
    /// \param test_program The container test program.
    /// \param test_case_name The name of the test case to run.
    /// \param user_config User-provided configuration variables.
    /// \param body_handle The exit handle of the test case's corresponding
    ///     body.  The cleanup will be executed in the same context.
    /// \param body_result The result of the test case's corresponding body.
    ///
    /// \return A handle for the background operation.  Used to match the result
    /// of the execution returned by wait_any() with this invocation.
    executor::exec_handle
    spawn_cleanup(const model::test_program_ptr test_program,
                  const std::string& test_case_name,
                  const config::tree& user_config,
                  const executor::exit_handle& body_handle,
                  const model::test_result& body_result)
    {
        generic.check_interrupt();

        const std::shared_ptr< scheduler::interface > interface =
            find_interface(test_program->interface_name());

        LI(F("Spawning %s:%s (cleanup)") % test_program->absolute_path() %
           test_case_name);

        const executor::exec_handle handle = generic.spawn_followup(
            run_test_cleanup(interface, test_program, test_case_name,
                             user_config),
            body_handle, cleanup_timeout);

        const exec_data_ptr data(new cleanup_exec_data(
            test_program, test_case_name, body_handle, body_result));
        LD(F("Inserting %s into all_exec_data (cleanup)") % handle.pid());
        INV_MSG(all_exec_data.find(handle.pid()) == all_exec_data.end(),
                F("PID %s already in all_exec_data; not properly cleaned "
                  "up or reused too fast") % handle.pid());;
        all_exec_data.insert(exec_data_map::value_type(handle.pid(), data));

        return handle;
    }

    /// Cleans up a single test case execenv synchronously.
    ///
    /// \param test_data The data of the previously executed test case to be
    ///     cleaned up.
    void
    sync_execenv_cleanup(const test_exec_data* test_data)
    {
        // The message in this result should never be seen by the user, but use
        // something reasonable just in case it leaks and we need to pinpoint
        // the call site.
        model::test_result result(model::test_result_broken,
                                  "Test case died abruptly");

        const executor::exec_handle cleanup_handle = spawn_execenv_cleanup(
            test_data->test_program, test_data->test_case_name,
            test_data->exit_handle.get(), result);
        generic.wait(cleanup_handle);
    }

    /// Forks and executes a test case execenv cleanup asynchronously.
    ///
    /// \param test_program The container test program.
    /// \param test_case_name The name of the test case to run.
    /// \param body_handle The exit handle of the test case's corresponding
    ///     body.  The cleanup will be executed in the same context.
    /// \param body_result The result of the test case's corresponding body.
    ///
    /// \return A handle for the background operation.  Used to match the result
    /// of the execution returned by wait_any() with this invocation.
    executor::exec_handle
    spawn_execenv_cleanup(const model::test_program_ptr test_program,
                          const std::string& test_case_name,
                          const executor::exit_handle& body_handle,
                          const model::test_result& body_result)
    {
        generic.check_interrupt();

        LI(F("Spawning %s:%s (execenv cleanup)")
            % test_program->absolute_path() % test_case_name);

        const executor::exec_handle handle = generic.spawn_followup(
            run_execenv_cleanup(test_program, test_case_name),
            body_handle, execenv_cleanup_timeout);

        const exec_data_ptr data(new execenv_exec_data(
            test_program, test_case_name, body_handle, body_result));
        LD(F("Inserting %s into all_exec_data (execenv cleanup)") % handle.pid());
        INV_MSG(all_exec_data.find(handle.pid()) == all_exec_data.end(),
                F("PID %s already in all_exec_data; not properly cleaned "
                  "up or reused too fast") % handle.pid());;
        all_exec_data.insert(exec_data_map::value_type(handle.pid(), data));

        return handle;
    }
};


/// Constructor.
scheduler::scheduler_handle::scheduler_handle(void) : _pimpl(new impl())
{
}


/// Destructor.
scheduler::scheduler_handle::~scheduler_handle(void)
{
}


/// Queries the path to the root of the work directory for all tests.
///
/// \return A path.
const fs::path&
scheduler::scheduler_handle::root_work_directory(void) const
{
    return _pimpl->generic.root_work_directory();
}


/// Cleans up the scheduler state.
///
/// This function should be called explicitly as it provides the means to
/// control any exceptions raised during cleanup.  Do not rely on the destructor
/// to clean things up.
///
/// \throw engine::error If there are problems cleaning up the scheduler.
void
scheduler::scheduler_handle::cleanup(void)
{
    _pimpl->generic.cleanup();
}


/// Checks if the given interface name is valid.
///
/// \param name The name of the interface to validate.
///
/// \throw engine::error If the given interface is not supported.
void
scheduler::ensure_valid_interface(const std::string& name)
{
    if (interfaces.find(name) == interfaces.end())
        throw engine::error(F("Unsupported test interface '%s'") % name);
}


/// Registers a new interface.
///
/// \param name The name of the interface.  Must not have yet been registered.
/// \param spec Interface specification.
void
scheduler::register_interface(const std::string& name,
                              const std::shared_ptr< interface > spec)
{
    PRE(interfaces.find(name) == interfaces.end());
    interfaces.insert(interfaces_map::value_type(name, spec));
}


/// Returns the names of all registered interfaces.
///
/// \return A collection of interface names.
std::set< std::string >
scheduler::registered_interface_names(void)
{
    std::set< std::string > names;
    for (interfaces_map::const_iterator iter = interfaces.begin();
         iter != interfaces.end(); ++iter) {
        names.insert((*iter).first);
    }
    return names;
}


/// Initializes the scheduler.
///
/// \pre This function can only be called if there is no other scheduler_handle
/// object alive.
///
/// \return A handle to the operations of the scheduler.
scheduler::scheduler_handle
scheduler::setup(void)
{
    return scheduler_handle();
}


/// Retrieves the list of test cases from a test program.
///
/// This operation is currently synchronous.
///
/// This operation should never throw.  Any errors during the processing of the
/// test case list are subsumed into a single test case in the return value that
/// represents the failed retrieval.
///
/// \param test_program The test program from which to obtain the list of test
/// cases.
/// \param user_config User-provided configuration variables.
///
/// \return The list of test cases.
model::test_cases_map
scheduler::scheduler_handle::list_tests(
    const model::test_program* test_program,
    const config::tree& user_config)
{
    _pimpl->generic.check_interrupt();

    const std::shared_ptr< scheduler::interface > interface = find_interface(
        test_program->interface_name());

    try {
        const executor::exec_handle exec_handle = _pimpl->generic.spawn(
            list_test_cases(interface, test_program, user_config),
            list_timeout, none);
        executor::exit_handle exit_handle = _pimpl->generic.wait(exec_handle);

        const model::test_cases_map test_cases = interface->parse_list(
            exit_handle.status(),
            exit_handle.stdout_file(),
            exit_handle.stderr_file());

        exit_handle.cleanup();

        if (test_cases.empty())
            throw std::runtime_error("Empty test cases list");

        return test_cases;
    } catch (const std::runtime_error& e) {
        // TODO(jmmv): This is a very ugly workaround for the fact that we
        // cannot report failures at the test-program level.
        LW(F("Failed to load test cases list: %s") % e.what());
        model::test_cases_map fake_test_cases;
        fake_test_cases.insert(model::test_cases_map::value_type(
            "__test_cases_list__",
            model::test_case(
                "__test_cases_list__",
                "Represents the correct processing of the test cases list",
                model::test_result(model::test_result_broken, e.what()))));
        return fake_test_cases;
    }
}


/// Forks and executes a test case asynchronously.
///
/// Note that the caller needn't know if the test has a cleanup routine or not.
/// If there indeed is a cleanup routine, we trigger it at wait_any() time.
///
/// \param test_program The container test program.
/// \param test_case_name The name of the test case to run.
/// \param user_config User-provided configuration variables.
///
/// \return A handle for the background operation.  Used to match the result of
/// the execution returned by wait_any() with this invocation.
scheduler::exec_handle
scheduler::scheduler_handle::spawn_test(
    const model::test_program_ptr test_program,
    const std::string& test_case_name,
    const config::tree& user_config)
{
    _pimpl->generic.check_interrupt();

    const std::shared_ptr< scheduler::interface > interface = find_interface(
        test_program->interface_name());

    LI(F("Spawning %s:%s") % test_program->absolute_path() % test_case_name);

    const model::test_case& test_case = test_program->find(test_case_name);

    optional< passwd::user > unprivileged_user;
    if (user_config.is_set("unprivileged_user") &&
        test_case.get_metadata().required_user() == "unprivileged") {
        unprivileged_user = user_config.lookup< engine::user_node >(
            "unprivileged_user");
    }

    const executor::exec_handle handle = _pimpl->generic.spawn(
        run_test_program(interface, test_program, test_case_name,
                         user_config),
        test_case.get_metadata().timeout(),
        unprivileged_user);

    const exec_data_ptr data(new test_exec_data(
        test_program, test_case_name, interface, user_config, handle.pid()));
    LD(F("Inserting %s into all_exec_data") % handle.pid());
    INV_MSG(
        _pimpl->all_exec_data.find(handle.pid()) == _pimpl->all_exec_data.end(),
        F("PID %s already in all_exec_data; not cleaned up or reused too fast")
        % handle.pid());;
    _pimpl->all_exec_data.insert(exec_data_map::value_type(handle.pid(), data));

    return handle.pid();
}


/// Waits for completion of any forked test case.
///
/// Note that if the terminated test case has a cleanup routine, this function
/// is the one in charge of spawning the cleanup routine asynchronously.
///
/// \return The result of the execution of a subprocess.  This is a dynamically
/// allocated object because the scheduler can spawn subprocesses of various
/// types and, at wait time, we don't know upfront what we are going to get.
scheduler::result_handle_ptr
scheduler::scheduler_handle::wait_any(void)
{
    _pimpl->generic.check_interrupt();

    executor::exit_handle handle = _pimpl->generic.wait_any();

    const exec_data_map::iterator iter = _pimpl->all_exec_data.find(
        handle.original_pid());
    exec_data_ptr data = (*iter).second;

    utils::dump_stacktrace_if_available(data->test_program->absolute_path(),
                                        _pimpl->generic, handle);

    optional< model::test_result > result;

    // test itself
    try {
        test_exec_data* test_data = &dynamic_cast< test_exec_data& >(
            *data.get());
        LD(F("Got %s from all_exec_data") % handle.original_pid());

        test_data->exit_handle = handle;

        const model::test_case& test_case = test_data->test_program->find(
            test_data->test_case_name);

        result = test_case.fake_result();

        if (!result && handle.status() && handle.status().get().exited() &&
            handle.status().get().exitstatus() == exit_skipped) {
            // If the test's process terminated with our magic "exit_skipped"
            // status, there are two cases to handle.  The first is the case
            // where the "skipped cookie" exists, in which case we never got to
            // actually invoke the test program; if that's the case, handle it
            // here.  The second case is where the test case actually decided to
            // exit with the "exit_skipped" status; in that case, just fall back
            // to the regular status handling.
            const fs::path skipped_cookie_path = handle.control_directory() /
                skipped_cookie;
            std::ifstream input(skipped_cookie_path.c_str());
            if (input) {
                result = model::test_result(model::test_result_skipped,
                                            utils::read_stream(input));
                input.close();

                // If we determined that the test needs to be skipped, we do not
                // want to run the cleanup routine because doing so could result
                // in errors.  However, we still want to run the cleanup routine
                // if the test's body reports a skip (because actions could have
                // already been taken).
                test_data->needs_cleanup = false;
                test_data->needs_execenv_cleanup = false;
            }
        }
        if (!result) {
            result = test_data->interface->compute_result(
                handle.status(),
                handle.control_directory(),
                handle.stdout_file(),
                handle.stderr_file());
        }
        INV(result);

        if (!result.get().good()) {
            append_files_listing(handle.work_directory(),
                                 handle.stderr_file());
        }

        std::shared_ptr< debugger > debugger = test_case.get_debugger();
        if (debugger) {
            debugger->before_cleanup(test_data->test_program, test_case,
                result, handle);
        }

        if (test_data->needs_cleanup) {
            INV(test_case.get_metadata().has_cleanup());

            // The test body has completed and we have processed it.  If there
            // is a cleanup routine, trigger it now and wait for any other test
            // completion.  The caller never knows about cleanup routines.
            _pimpl->spawn_cleanup(test_data->test_program,
                                  test_data->test_case_name,
                                  test_data->user_config, handle, result.get());

            // TODO(jmmv): Chaining this call is ugly.  We'd be better off by
            // looping over terminated processes until we got a result suitable
            // for user consumption.  For the time being this is good enough and
            // not a problem because the call chain won't get big: the majority
            // of test cases do not have cleanup routines.
            return wait_any();
        }

        if (test_data->needs_execenv_cleanup) {
            INV(test_case.get_metadata().has_execenv());
            _pimpl->spawn_execenv_cleanup(test_data->test_program,
                                          test_data->test_case_name,
                                          handle, result.get());
            test_data->needs_execenv_cleanup = false;
            return wait_any();
        }
    } catch (const std::bad_cast& e) {
        // ok, let's check for another type
    }

    // test cleanup
    try {
        const cleanup_exec_data* cleanup_data =
            &dynamic_cast< const cleanup_exec_data& >(*data.get());
        LD(F("Got %s from all_exec_data (cleanup)") % handle.original_pid());

        // Handle the completion of cleanup subprocesses internally: the caller
        // is not aware that these exist so, when we return, we must return the
        // data for the original test that triggered this routine.  For example,
        // because the caller wants to see the exact same exec_handle that was
        // returned by spawn_test.

        const model::test_result& body_result = cleanup_data->body_result;
        if (body_result.good()) {
            if (!handle.status()) {
                result = model::test_result(model::test_result_broken,
                                            "Test case cleanup timed out");
            } else {
                if (!handle.status().get().exited() ||
                    handle.status().get().exitstatus() != EXIT_SUCCESS) {
                    result = model::test_result(
                        model::test_result_broken,
                        "Test case cleanup did not terminate successfully");
                } else {
                    result = body_result;
                }
            }
        } else {
            result = body_result;
        }

        // Untrack the cleanup process.  This must be done explicitly because we
        // do not create a result_handle object for the cleanup, and that is the
        // one in charge of doing so in the regular (non-cleanup) case.
        LD(F("Removing %s from all_exec_data (cleanup) in favor of %s")
           % handle.original_pid()
           % cleanup_data->body_exit_handle.original_pid());
        _pimpl->all_exec_data.erase(handle.original_pid());

        handle = cleanup_data->body_exit_handle;

        const exec_data_map::iterator it = _pimpl->all_exec_data.find(
            handle.original_pid());
        if (it != _pimpl->all_exec_data.end()) {
            exec_data_ptr d = (*it).second;
            test_exec_data* test_data = &dynamic_cast< test_exec_data& >(
                *d.get());
            const model::test_case& test_case =
                cleanup_data->test_program->find(cleanup_data->test_case_name);
            test_data->needs_cleanup = false;

            if (test_data->needs_execenv_cleanup) {
                INV(test_case.get_metadata().has_execenv());
                _pimpl->spawn_execenv_cleanup(cleanup_data->test_program,
                                              cleanup_data->test_case_name,
                                              handle, result.get());
                test_data->needs_execenv_cleanup = false;
                return wait_any();
            }
        }
    } catch (const std::bad_cast& e) {
        // ok, let's check for another type
    }

    // execenv cleanup
    try {
        const execenv_exec_data* execenv_data =
            &dynamic_cast< const execenv_exec_data& >(*data.get());
        LD(F("Got %s from all_exec_data (execenv cleanup)") % handle.original_pid());

        const model::test_result& body_result = execenv_data->body_result;
        if (body_result.good()) {
            if (!handle.status()) {
                result = model::test_result(model::test_result_broken,
                                            "Test case execenv cleanup timed out");
            } else {
                if (!handle.status().get().exited() ||
                    handle.status().get().exitstatus() != EXIT_SUCCESS) {
                    result = model::test_result(
                        model::test_result_broken,
                        "Test case execenv cleanup did not terminate successfully"); // ?
                } else {
                    result = body_result;
                }
            }
        } else {
            result = body_result;
        }

        LD(F("Removing %s from all_exec_data (execenv cleanup) in favor of %s")
           % handle.original_pid()
           % execenv_data->body_exit_handle.original_pid());
        _pimpl->all_exec_data.erase(handle.original_pid());

        handle = execenv_data->body_exit_handle;
    } catch (const std::bad_cast& e) {
        // ok, it was one of the types above
    }

    INV(result);

    std::shared_ptr< result_handle::bimpl > result_handle_bimpl(
        new result_handle::bimpl(handle, _pimpl->all_exec_data));
    std::shared_ptr< test_result_handle::impl > test_result_handle_impl(
        new test_result_handle::impl(
            data->test_program, data->test_case_name, result.get()));
    return result_handle_ptr(new test_result_handle(result_handle_bimpl,
                                                    test_result_handle_impl));
}


/// Forks and executes a test case synchronously for debugging.
///
/// \pre No other processes should be in execution by the scheduler.
///
/// \param test_program The container test program.
/// \param test_case_name The name of the test case to run.
/// \param user_config User-provided configuration variables.
/// \param stdout_target File to which to write the stdout of the test case.
/// \param stderr_target File to which to write the stderr of the test case.
///
/// \return The result of the execution of the test.
scheduler::result_handle_ptr
scheduler::scheduler_handle::debug_test(
    const model::test_program_ptr test_program,
    const std::string& test_case_name,
    const config::tree& user_config,
    const fs::path& stdout_target,
    const fs::path& stderr_target)
{
    const exec_handle exec_handle = spawn_test(
        test_program, test_case_name, user_config);
    result_handle_ptr result_handle = wait_any();

    // TODO(jmmv): We need to do this while the subprocess is alive.  This is
    // important for debugging purposes, as we should see the contents of stdout
    // or stderr as they come in.
    //
    // Unfortunately, we cannot do so.  We cannot just read and block from a
    // file, waiting for further output to appear... as this only works on pipes
    // or sockets.  We need a better interface for this whole thing.
    {
        std::unique_ptr< std::ostream > output = utils::open_ostream(
            stdout_target);
        *output << utils::read_file(result_handle->stdout_file());
    }
    {
        std::unique_ptr< std::ostream > output = utils::open_ostream(
            stderr_target);
        *output << utils::read_file(result_handle->stderr_file());
    }

    INV(result_handle->original_pid() == exec_handle);
    return result_handle;
}


/// Checks if an interrupt has fired.
///
/// Calls to this function should be sprinkled in strategic places through the
/// code protected by an interrupts_handler object.
///
/// This is just a wrapper over signals::check_interrupt() to avoid leaking this
/// dependency to the caller.
///
/// \throw signals::interrupted_error If there has been an interrupt.
void
scheduler::scheduler_handle::check_interrupt(void) const
{
    _pimpl->generic.check_interrupt();
}


/// Queries the current execution context.
///
/// \return The queried context.
model::context
scheduler::current_context(void)
{
    return model::context(fs::current_path(), utils::getallenv());
}


/// Generates the set of configuration variables for a test program.
///
/// \param user_config The configuration variables provided by the user.
/// \param test_suite The name of the test suite.
///
/// \return The mapping of configuration variables for the test program.
config::properties_map
scheduler::generate_config(const config::tree& user_config,
                           const std::string& test_suite)
{
    config::properties_map props;

    try {
        props = user_config.all_properties(F("test_suites.%s") % test_suite,
                                           true);
    } catch (const config::unknown_key_error& unused_error) {
        // Ignore: not all test suites have entries in the configuration.
    }

    // TODO(jmmv): This is a hack that exists for the ATF interface only, so it
    // should be moved there.
    if (user_config.is_set("unprivileged_user")) {
        const passwd::user& user =
            user_config.lookup< engine::user_node >("unprivileged_user");
        // The property is duplicated using both ATF and Kyua naming styles
        // for better UX.
        props["unprivileged-user"] = user.name;
        props["unprivileged_user"] = user.name;
    }

    return props;
}
