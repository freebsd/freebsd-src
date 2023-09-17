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

#include "utils/sqlite/transaction.hpp"

#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"

namespace sqlite = utils::sqlite;


/// Internal implementation for the transaction.
struct utils::sqlite::transaction::impl : utils::noncopyable {
    /// The database this transaction belongs to.
    database& db;

    /// Possible statuses of a transaction.
    enum statuses {
        open_status,
        committed_status,
        rolled_back_status,
    };

    /// The current status of the transaction.
    statuses status;

    /// Constructs a new transaction.
    ///
    /// \param db_ The database this transaction belongs to.
    /// \param status_ The status of the new transaction.
    impl(database& db_, const statuses status_) :
        db(db_),
        status(status_)
    {
    }

    /// Destroys the transaction.
    ///
    /// This rolls back the transaction if it is open.
    ~impl(void)
    {
        if (status == impl::open_status) {
            try {
                rollback();
            } catch (const sqlite::error& e) {
                LW(F("Error while rolling back a transaction: %s") % e.what());
            }
        }
    }

    /// Commits the transaction.
    ///
    /// \throw api_error If there is any problem while committing the
    ///     transaction.
    void
    commit(void)
    {
        PRE(status == impl::open_status);
        db.exec("COMMIT");
        status = impl::committed_status;
    }

    /// Rolls the transaction back.
    ///
    /// \throw api_error If there is any problem while rolling the
    ///     transaction back.
    void
    rollback(void)
    {
        PRE(status == impl::open_status);
        db.exec("ROLLBACK");
        status = impl::rolled_back_status;
    }
};


/// Initializes a transaction object.
///
/// This is an internal function.  Use database::begin_transaction() to
/// instantiate one of these objects.
///
/// \param db The database this transaction belongs to.
sqlite::transaction::transaction(database& db) :
    _pimpl(new impl(db, impl::open_status))
{
}


/// Destructor for the transaction.
sqlite::transaction::~transaction(void)
{
}


/// Commits the transaction.
///
/// \throw api_error If there is any problem while committing the transaction.
void
sqlite::transaction::commit(void)
{
    _pimpl->commit();
}


/// Rolls the transaction back.
///
/// \throw api_error If there is any problem while rolling the transaction back.
void
sqlite::transaction::rollback(void)
{
    _pimpl->rollback();
}
