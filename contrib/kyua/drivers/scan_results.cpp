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

#include "drivers/scan_results.hpp"

#include "engine/filters.hpp"
#include "model/context.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "store/read_backend.hpp"
#include "store/read_transaction.hpp"
#include "utils/defs.hpp"

namespace fs = utils::fs;


/// Pure abstract destructor.
drivers::scan_results::base_hooks::~base_hooks(void)
{
}


/// Callback executed before any operation is performed.
void
drivers::scan_results::base_hooks::begin(void)
{
}


/// Callback executed after all operations are performed.
void
drivers::scan_results::base_hooks::end(const result& /* r */)
{
}


/// Executes the operation.
///
/// \param store_path The path to the database store.
/// \param raw_filters The test case filters as provided by the user.
/// \param hooks The hooks for this execution.
///
/// \returns A structure with all results computed by this driver.
drivers::scan_results::result
drivers::scan_results::drive(const fs::path& store_path,
                             const std::set< engine::test_filter >& raw_filters,
                             base_hooks& hooks)
{
    engine::filters_state filters(raw_filters);

    store::read_backend db = store::read_backend::open_ro(store_path);
    store::read_transaction tx = db.start_read();

    hooks.begin();

    const model::context context = tx.get_context();
    hooks.got_context(context);

    store::results_iterator iter = tx.get_results();
    while (iter) {
        // TODO(jmmv): We should be filtering at the test case level for
        // efficiency, but that means we would need to execute more than one
        // query on the database and our current interfaces don't support that.
        //
        // Reuse engine::filters_state for the time being because it is simpler
        // and we get tracking of unmatched filters "for free".
        const model::test_program_ptr test_program = iter.test_program();
        if (filters.match_test_program(test_program->relative_path())) {
            const model::test_case& test_case = test_program->find(
                iter.test_case_name());
            if (filters.match_test_case(test_program->relative_path(),
                                        test_case.name())) {
                hooks.got_result(iter);
            }
        }
        ++iter;
    }

    result r(filters.unused());
    hooks.end(r);
    return r;
}
