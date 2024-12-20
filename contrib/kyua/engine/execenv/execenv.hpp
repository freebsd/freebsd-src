// Copyright 2023 The Kyua Authors.
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

/// \file engine/execenv/execenv.hpp
/// Execution environment subsystem interface.

#if !defined(ENGINE_EXECENV_EXECENV_HPP)
#define ENGINE_EXECENV_EXECENV_HPP

#include "model/test_program.hpp"
#include "utils/optional.ipp"
#include "utils/process/operations_fwd.hpp"

using utils::process::args_vector;
using utils::optional;

namespace engine {
namespace execenv {


extern const char* default_execenv_name;


/// Abstract interface of an execution environment.
class interface {
protected:
    const model::test_program& _test_program;
    const std::string& _test_case_name;

public:
    /// Constructor.
    ///
    /// \param program The test program.
    /// \param test_case_name Name of the test case.
    interface(const model::test_program& test_program,
              const std::string& test_case_name) :
        _test_program(test_program),
        _test_case_name(test_case_name)
    {}

    /// Destructor.
    virtual ~interface() {}

    /// Initializes execution environment.
    ///
    /// It's expected to be called inside a fork which runs
    /// scheduler::interface::exec_test(), so we can fail a test fast if its
    /// execution environment setup fails, and test execution could use the
    /// configured proc environment, if expected.
    virtual void init() const = 0;

    /// Cleanups or removes execution environment.
    ///
    /// It's expected to be called inside a fork for execenv cleanup.
    virtual void cleanup() const = 0;

    /// Executes a test within the execution environment.
    ///
    /// It's expected to be called inside a fork which runs
    /// scheduler::interface::exec_test() or exec_cleanup().
    ///
    /// \param args The arguments to pass to the binary.
    virtual void exec(const args_vector& args) const UTILS_NORETURN = 0;
};


/// Abstract interface of an execution environment manager.
class manager {
public:
    /// Destructor.
    virtual ~manager() {}

    /// Returns name of an execution environment.
    virtual const std::string& name() const = 0;

    /// Returns whether this execution environment is actually supported.
    ///
    /// It can be compile time and/or runtime check.
    virtual bool is_supported() const = 0;

    /// Returns execution environment for a test.
    ///
    /// It checks if the given test is designed for this execution environment.
    ///
    /// \param program The test program.
    /// \param test_case_name Name of the test case.
    ///
    /// \return An execenv object if the test conforms, or none.
    virtual std::unique_ptr< interface > probe(
        const model::test_program& test_program,
        const std::string& test_case_name) const = 0;

    // TODO: execenv related extra metadata could be provided by a manager
    // not to know how exactly and where it should be added to the kyua
};


/// Registers an execution environment.
///
/// \param manager Execution environment manager.
void register_execenv(const std::shared_ptr< manager > manager);


/// Returns list of registered execenv managers, except default host one.
///
/// \return A vector of pointers to execenv managers.
const std::vector< std::shared_ptr< manager> > execenvs();


/// Returns execution environment for a test case.
///
/// \param program The test program.
/// \param test_case_name Name of the test case.
///
/// \return An execution environment of a test.
std::unique_ptr< execenv::interface > get(
    const model::test_program& test_program,
    const std::string& test_case_name);


}  // namespace execenv
}  // namespace engine

#endif  // !defined(ENGINE_EXECENV_EXECENV_HPP)
