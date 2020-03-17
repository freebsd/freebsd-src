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

#include "drivers/run_tests.hpp"

#include <utility>

#include "engine/config.hpp"
#include "engine/filters.hpp"
#include "engine/kyuafile.hpp"
#include "engine/scanner.hpp"
#include "engine/scheduler.hpp"
#include "model/context.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "store/write_backend.hpp"
#include "store/write_transaction.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/text/operations.ipp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace scheduler = engine::scheduler;
namespace text = utils::text;

using utils::none;
using utils::optional;


namespace {


/// Map of test program identifiers (relative paths) to their identifiers in the
/// database.  We need to keep this in memory because test programs can be
/// returned by the scanner in any order, and we only want to put each test
/// program once.
typedef std::map< fs::path, int64_t > path_to_id_map;


/// Map of in-flight PIDs to their corresponding test case IDs.
typedef std::map< int, int64_t > pid_to_id_map;


/// Pair of PID to a test case ID.
typedef pid_to_id_map::value_type pid_and_id_pair;


/// Puts a test program in the store and returns its identifier.
///
/// This function is idempotent: we maintain a side cache of already-put test
/// programs so that we can return their identifiers without having to put them
/// again.
/// TODO(jmmv): It's possible that the store module should offer this
/// functionality and not have to do this ourselves here.
///
/// \param test_program The test program being put.
/// \param [in,out] tx Writable transaction on the store.
/// \param [in,out] ids_cache Cache of already-put test programs.
///
/// \return A test program identifier.
static int64_t
find_test_program_id(const model::test_program_ptr test_program,
                     store::write_transaction& tx,
                     path_to_id_map& ids_cache)
{
    const fs::path& key = test_program->relative_path();
    std::map< fs::path, int64_t >::const_iterator iter = ids_cache.find(key);
    if (iter == ids_cache.end()) {
        const int64_t id = tx.put_test_program(*test_program);
        ids_cache.insert(std::make_pair(key, id));
        return id;
    } else {
        return (*iter).second;
    }
}


/// Stores the result of an execution in the database.
///
/// \param test_case_id Identifier of the test case in the database.
/// \param result The result of the execution.
/// \param [in,out] tx Writable transaction where to store the result data.
static void
put_test_result(const int64_t test_case_id,
                const scheduler::test_result_handle& result,
                store::write_transaction& tx)
{
    tx.put_result(result.test_result(), test_case_id,
                  result.start_time(), result.end_time());
    tx.put_test_case_file("__STDOUT__", result.stdout_file(), test_case_id);
    tx.put_test_case_file("__STDERR__", result.stderr_file(), test_case_id);

}


/// Cleans up a test case and folds any errors into the test result.
///
/// \param handle The result handle for the test.
///
/// \return The test result if the cleanup succeeds; a broken test result
/// otherwise.
model::test_result
safe_cleanup(scheduler::test_result_handle handle) throw()
{
    try {
        handle.cleanup();
        return handle.test_result();
    } catch (const std::exception& e) {
        return model::test_result(
            model::test_result_broken,
            F("Failed to clean up test case's work directory %s: %s") %
            handle.work_directory() % e.what());
    }
}


/// Starts a test asynchronously.
///
/// \param handle Scheduler handle.
/// \param match Test program and test case to start.
/// \param [in,out] tx Writable transaction to obtain test IDs.
/// \param [in,out] ids_cache Cache of already-put test cases.
/// \param user_config The end-user configuration properties.
/// \param hooks The hooks for this execution.
///
/// \returns The PID for the started test and the test case's identifier in the
/// store.
pid_and_id_pair
start_test(scheduler::scheduler_handle& handle,
           const engine::scan_result& match,
           store::write_transaction& tx,
           path_to_id_map& ids_cache,
           const config::tree& user_config,
           drivers::run_tests::base_hooks& hooks)
{
    const model::test_program_ptr test_program = match.first;
    const std::string& test_case_name = match.second;

    hooks.got_test_case(*test_program, test_case_name);

    const int64_t test_program_id = find_test_program_id(
        test_program, tx, ids_cache);
    const int64_t test_case_id = tx.put_test_case(
        *test_program, test_case_name, test_program_id);

    const scheduler::exec_handle exec_handle = handle.spawn_test(
        test_program, test_case_name, user_config);
    return std::make_pair(exec_handle, test_case_id);
}


/// Processes the completion of a test.
///
/// \param [in,out] result_handle The completion handle of the test subprocess.
/// \param test_case_id Identifier of the test case as returned by start_test().
/// \param [in,out] tx Writable transaction to put the test results.
/// \param hooks The hooks for this execution.
///
/// \post result_handle is cleaned up.  The caller cannot clean it up again.
void
finish_test(scheduler::result_handle_ptr result_handle,
            const int64_t test_case_id,
            store::write_transaction& tx,
            drivers::run_tests::base_hooks& hooks)
{
    const scheduler::test_result_handle* test_result_handle =
        dynamic_cast< const scheduler::test_result_handle* >(
            result_handle.get());

    put_test_result(test_case_id, *test_result_handle, tx);

    const model::test_result test_result = safe_cleanup(*test_result_handle);
    hooks.got_result(
        *test_result_handle->test_program(),
        test_result_handle->test_case_name(),
        test_result_handle->test_result(),
        result_handle->end_time() - result_handle->start_time());
}


/// Extracts the keys of a pid_to_id_map and returns them as a string.
///
/// \param map The PID to test ID map from which to get the PIDs.
///
/// \return A user-facing string with the collection of PIDs.
static std::string
format_pids(const pid_to_id_map& map)
{
    std::set< pid_to_id_map::key_type > pids;
    for (pid_to_id_map::const_iterator iter = map.begin(); iter != map.end();
         ++iter) {
        pids.insert(iter->first);
    }
    return text::join(pids, ",");
}


}  // anonymous namespace


/// Pure abstract destructor.
drivers::run_tests::base_hooks::~base_hooks(void)
{
}


/// Executes the operation.
///
/// \param kyuafile_path The path to the Kyuafile to be loaded.
/// \param build_root If not none, path to the built test programs.
/// \param store_path The path to the store to be used.
/// \param filters The test case filters as provided by the user.
/// \param user_config The end-user configuration properties.
/// \param hooks The hooks for this execution.
///
/// \returns A structure with all results computed by this driver.
drivers::run_tests::result
drivers::run_tests::drive(const fs::path& kyuafile_path,
                          const optional< fs::path > build_root,
                          const fs::path& store_path,
                          const std::set< engine::test_filter >& filters,
                          const config::tree& user_config,
                          base_hooks& hooks)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    const engine::kyuafile kyuafile = engine::kyuafile::load(
        kyuafile_path, build_root, user_config, handle);
    store::write_backend db = store::write_backend::open_rw(store_path);
    store::write_transaction tx = db.start_write();

    {
        const model::context context = scheduler::current_context();
        (void)tx.put_context(context);
    }

    engine::scanner scanner(kyuafile.test_programs(), filters);

    path_to_id_map ids_cache;
    pid_to_id_map in_flight;
    std::vector< engine::scan_result > exclusive_tests;

    const std::size_t slots = user_config.lookup< config::positive_int_node >(
        "parallelism");
    INV(slots >= 1);
    do {
        INV(in_flight.size() <= slots);

        // Spawn as many jobs as needed to fill our execution slots.  We do this
        // first with the assumption that the spawning is faster than any single
        // job, so we want to keep as many jobs in the background as possible.
        while (in_flight.size() < slots) {
            optional< engine::scan_result > match = scanner.yield();
            if (!match)
                break;
            const model::test_program_ptr test_program = match.get().first;
            const std::string& test_case_name = match.get().second;

            const model::test_case& test_case = test_program->find(
                test_case_name);
            if (test_case.get_metadata().is_exclusive()) {
                // Exclusive tests get processed later, separately.
                exclusive_tests.push_back(match.get());
                continue;
            }

            const pid_and_id_pair pid_id = start_test(
                handle, match.get(), tx, ids_cache, user_config, hooks);
            INV_MSG(in_flight.find(pid_id.first) == in_flight.end(),
                    F("Spawned test has PID of still-tracked process %s") %
                    pid_id.first);
            in_flight.insert(pid_id);
        }

        // If there are any used slots, consume any at random and return the
        // result.  We consume slots one at a time to give preference to the
        // spawning of new tests as detailed above.
        if (!in_flight.empty()) {
            scheduler::result_handle_ptr result_handle = handle.wait_any();

            const pid_to_id_map::iterator iter = in_flight.find(
                result_handle->original_pid());
            INV_MSG(iter != in_flight.end(),
                    F("Lost track of in-flight PID %s; tracking %s") %
                    result_handle->original_pid() % format_pids(in_flight));
            const int64_t test_case_id = (*iter).second;
            in_flight.erase(iter);

            finish_test(result_handle, test_case_id, tx, hooks);
        }
    } while (!in_flight.empty() || !scanner.done());

    // Run any exclusive tests that we spotted earlier sequentially.
    for (std::vector< engine::scan_result >::const_iterator
             iter = exclusive_tests.begin(); iter != exclusive_tests.end();
             ++iter) {
        const pid_and_id_pair data = start_test(
            handle, *iter, tx, ids_cache, user_config, hooks);
        scheduler::result_handle_ptr result_handle = handle.wait_any();
        finish_test(result_handle, data.second, tx, hooks);
    }

    tx.commit();

    handle.cleanup();

    return result(scanner.unused_filters());
}
