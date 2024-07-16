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

/// \file engine/scheduler.hpp
/// Multiprogrammed executor of test related operations.
///
/// The scheduler's public interface exposes test cases as "black boxes".  The
/// handling of cleanup routines is completely hidden from the caller and
/// happens in two cases: first, once a test case completes; and, second, in the
/// case of abrupt termination due to the reception of a signal.
///
/// Hiding cleanup routines from the caller is an attempt to keep the logic of
/// execution and results handling in a single place.  Otherwise, the various
/// drivers (say run_tests and debug_test) would need to replicate the handling
/// of this logic, which is tricky in itself (particularly due to signal
/// handling) and would lead to inconsistencies.
///
/// Handling cleanup routines in the manner described above is *incredibly
/// complicated* (insane, actually) as you will see from the code.  The
/// complexity will bite us in the future (today is 2015-06-26).  Switching to a
/// threads-based implementation would probably simplify the code flow
/// significantly and allow parallelization of the test case listings in a
/// reasonable manner, though it depends on whether we can get clean handling of
/// signals and on whether we could use C++11's std::thread.  (Is this a to-do?
/// Maybe.  Maybe not.)
///
/// See the documentation in utils/process/executor.hpp for details on
/// the expected workflow of these classes.

#if !defined(ENGINE_SCHEDULER_HPP)
#define ENGINE_SCHEDULER_HPP

#include "engine/scheduler_fwd.hpp"

#include <memory>
#include <set>
#include <string>

#include "model/context_fwd.hpp"
#include "model/metadata_fwd.hpp"
#include "model/test_case_fwd.hpp"
#include "model/test_program.hpp"
#include "model/test_result_fwd.hpp"
#include "utils/config/tree_fwd.hpp"
#include "utils/datetime_fwd.hpp"
#include "utils/defs.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/optional.hpp"
#include "utils/process/executor_fwd.hpp"
#include "utils/process/status_fwd.hpp"

namespace engine {
namespace scheduler {


/// Abstract interface of a test program scheduler interface.
///
/// This interface defines the test program-specific operations that need to be
/// invoked at different points during the execution of a given test case.  The
/// scheduler internally instantiates one of these for every test case.
class interface {
public:
    /// Destructor.
    virtual ~interface() {}

    /// Executes a test program's list operation.
    ///
    /// This method is intended to be called within a subprocess and is expected
    /// to terminate execution either by exec(2)ing the test program or by
    /// exiting with a failure.
    ///
    /// \param test_program The test program to execute.
    /// \param vars User-provided variables to pass to the test program.
    virtual void exec_list(const model::test_program& test_program,
                           const utils::config::properties_map& vars)
        const UTILS_NORETURN = 0;

    /// Computes the test cases list of a test program.
    ///
    /// \param status The termination status of the subprocess used to execute
    ///     the exec_test() method or none if the test timed out.
    /// \param stdout_path Path to the file containing the stdout of the test.
    /// \param stderr_path Path to the file containing the stderr of the test.
    ///
    /// \return A list of test cases.
    virtual model::test_cases_map parse_list(
        const utils::optional< utils::process::status >& status,
        const utils::fs::path& stdout_path,
        const utils::fs::path& stderr_path) const = 0;

    /// Executes a test case of the test program.
    ///
    /// This method is intended to be called within a subprocess and is expected
    /// to terminate execution either by exec(2)ing the test program or by
    /// exiting with a failure.
    ///
    /// \param test_program The test program to execute.
    /// \param test_case_name Name of the test case to invoke.
    /// \param vars User-provided variables to pass to the test program.
    /// \param control_directory Directory where the interface may place control
    ///     files.
    virtual void exec_test(const model::test_program& test_program,
                           const std::string& test_case_name,
                           const utils::config::properties_map& vars,
                           const utils::fs::path& control_directory)
        const UTILS_NORETURN = 0;

    /// Executes a test cleanup routine of the test program.
    ///
    /// This method is intended to be called within a subprocess and is expected
    /// to terminate execution either by exec(2)ing the test program or by
    /// exiting with a failure.
    ///
    /// \param test_program The test program to execute.
    /// \param test_case_name Name of the test case to invoke.
    /// \param vars User-provided variables to pass to the test program.
    /// \param control_directory Directory where the interface may place control
    ///     files.
    virtual void exec_cleanup(const model::test_program& test_program,
                              const std::string& test_case_name,
                              const utils::config::properties_map& vars,
                              const utils::fs::path& control_directory)
        const UTILS_NORETURN;

    /// Computes the result of a test case based on its termination status.
    ///
    /// \param status The termination status of the subprocess used to execute
    ///     the exec_test() method or none if the test timed out.
    /// \param control_directory Directory where the interface may have placed
    ///     control files.
    /// \param stdout_path Path to the file containing the stdout of the test.
    /// \param stderr_path Path to the file containing the stderr of the test.
    ///
    /// \return A test result.
    virtual model::test_result compute_result(
        const utils::optional< utils::process::status >& status,
        const utils::fs::path& control_directory,
        const utils::fs::path& stdout_path,
        const utils::fs::path& stderr_path) const = 0;
};


/// Implementation of a test program with lazy loading of test cases.
class lazy_test_program : public model::test_program {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

public:
    lazy_test_program(const std::string&, const utils::fs::path&,
                      const utils::fs::path&, const std::string&,
                      const model::metadata&,
                      const utils::config::tree&,
                      scheduler_handle&);

    const model::test_cases_map& test_cases(void) const;
};


/// Base type containing the results of the execution of a subprocess.
class result_handle {
protected:
    struct bimpl;

private:
    /// Pointer to internal implementation of the base type.
    std::shared_ptr< bimpl > _pbimpl;

protected:
    friend class scheduler_handle;
    result_handle(std::shared_ptr< bimpl >);

public:
    virtual ~result_handle(void) = 0;

    void cleanup(void);

    int original_pid(void) const;
    const utils::datetime::timestamp& start_time() const;
    const utils::datetime::timestamp& end_time() const;
    utils::fs::path work_directory(void) const;
    const utils::fs::path& stdout_file(void) const;
    const utils::fs::path& stderr_file(void) const;
};


/// Container for all test termination data and accessor to cleanup operations.
class test_result_handle : public result_handle {
    struct impl;
    /// Pointer to internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend class scheduler_handle;
    test_result_handle(std::shared_ptr< bimpl >, std::shared_ptr< impl >);

public:
    ~test_result_handle(void);

    const model::test_program_ptr test_program(void) const;
    const std::string& test_case_name(void) const;
    const model::test_result& test_result(void) const;
};


/// Stateful interface to the multiprogrammed execution of tests.
class scheduler_handle {
    struct impl;
    /// Pointer to internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend scheduler_handle setup(void);
    scheduler_handle(void);

public:
    ~scheduler_handle(void);

    const utils::fs::path& root_work_directory(void) const;

    void cleanup(void);

    model::test_cases_map list_tests(const model::test_program*,
                                     const utils::config::tree&);
    exec_handle spawn_test(const model::test_program_ptr,
                           const std::string&,
                           const utils::config::tree&);
    result_handle_ptr wait_any(void);

    result_handle_ptr debug_test(const model::test_program_ptr,
                                 const std::string&,
                                 const utils::config::tree&,
                                 const utils::fs::path&,
                                 const utils::fs::path&);

    void check_interrupt(void) const;
};


extern utils::datetime::delta cleanup_timeout;
extern utils::datetime::delta execenv_cleanup_timeout;
extern utils::datetime::delta list_timeout;


void ensure_valid_interface(const std::string&);
void register_interface(const std::string&, const std::shared_ptr< interface >);
std::set< std::string > registered_interface_names(void);
scheduler_handle setup(void);

model::context current_context(void);
utils::config::properties_map generate_config(const utils::config::tree&,
                                              const std::string&);


}  // namespace scheduler
}  // namespace engine


#endif  // !defined(ENGINE_SCHEDULER_HPP)
