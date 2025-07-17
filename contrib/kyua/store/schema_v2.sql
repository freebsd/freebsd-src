-- Copyright 2012 The Kyua Authors.
-- All rights reserved.
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are
-- met:
--
-- * Redistributions of source code must retain the above copyright
--   notice, this list of conditions and the following disclaimer.
-- * Redistributions in binary form must reproduce the above copyright
--   notice, this list of conditions and the following disclaimer in the
--   documentation and/or other materials provided with the distribution.
-- * Neither the name of Google Inc. nor the names of its contributors
--   may be used to endorse or promote products derived from this software
--   without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
-- "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
-- A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
-- OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
-- SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
-- LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
-- DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
-- THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
-- (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
-- OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-- \file store/schema_v2.sql
-- Definition of the database schema.
--
-- The whole contents of this file are wrapped in a transaction.  We want
-- to ensure that the initial contents of the database (the table layout as
-- well as any predefined values) are written atomically to simplify error
-- handling in our code.


BEGIN TRANSACTION;


-- -------------------------------------------------------------------------
-- Metadata.
-- -------------------------------------------------------------------------


-- Database-wide properties.
--
-- Rows in this table are immutable: modifying the metadata implies writing
-- a new record with a new schema_version greater than all existing
-- records, and never updating previous records.  When extracting data from
-- this table, the only "valid" row is the one with the highest
-- scheam_version.  All the other rows are meaningless and only exist for
-- historical purposes.
--
-- In other words, this table keeps the history of the database metadata.
-- The only reason for doing this is for debugging purposes.  It may come
-- in handy to know when a particular database-wide operation happened if
-- it turns out that the database got corrupted.
CREATE TABLE metadata (
    schema_version INTEGER PRIMARY KEY CHECK (schema_version >= 1),
    timestamp TIMESTAMP NOT NULL CHECK (timestamp >= 0)
);


-- -------------------------------------------------------------------------
-- Contexts.
-- -------------------------------------------------------------------------


-- Execution contexts.
--
-- A context represents the execution environment of a particular action.
-- Because every action is invoked by the user, the context may have
-- changed.  We record such information for information and debugging
-- purposes.
CREATE TABLE contexts (
    context_id INTEGER PRIMARY KEY AUTOINCREMENT,
    cwd TEXT NOT NULL

    -- TODO(jmmv): Record the run-time configuration.
);


-- Environment variables of a context.
CREATE TABLE env_vars (
    context_id INTEGER REFERENCES contexts,
    var_name TEXT NOT NULL,
    var_value TEXT NOT NULL,

    PRIMARY KEY (context_id, var_name)
);


-- -------------------------------------------------------------------------
-- Actions.
-- -------------------------------------------------------------------------


-- Representation of user-initiated actions.
--
-- An action is an operation initiated by the user.  At the moment, the
-- only operation Kyua supports is the "test" operation (in the future we
-- should be able to store, e.g. build logs).  To keep things simple the
-- database schema is restricted to represent one single action.
CREATE TABLE actions (
    action_id INTEGER PRIMARY KEY AUTOINCREMENT,
    context_id INTEGER REFERENCES contexts
);


-- -------------------------------------------------------------------------
-- Test suites.
--
-- The tables in this section represent all the components that form a test
-- suite.  This includes data about the test suite itself (test programs
-- and test cases), and also the data about particular runs (test results).
--
-- As you will notice, every object belongs to a particular action, has a
-- unique identifier and there is no attempt to deduplicate data.  This
-- comes from the fact that a test suite is not "stable" over time: i.e. on
-- each execution of the test suite, test programs and test cases may have
-- come and gone.  This has the interesting result of making the
-- distinction of a test case and a test result a pure syntactic
-- difference, because there is always a 1:1 relation.
--
-- The code that performs the processing of the actions is the component in
-- charge of finding correlations between test programs and test cases
-- across different actions.
-- -------------------------------------------------------------------------


-- Representation of the metadata objects.
--
-- The way this table works is like this: every time we record a metadata
-- object, we calculate what its identifier should be as the last rowid of
-- the table.  All properties of that metadata object thus receive the same
-- identifier.
CREATE TABLE metadatas (
    metadata_id INTEGER NOT NULL,

    -- The name of the property.
    property_name TEXT NOT NULL,

    -- One of the values of the property.
    property_value TEXT,

    PRIMARY KEY (metadata_id, property_name)
);


-- Optimize the loading of the metadata of any single entity.
--
-- The metadata_id column of the metadatas table is not enough to act as a
-- primary key, yet we need to locate entries in the metadatas table solely by
-- their identifier.
--
-- TODO(jmmv): I think this index is useless given that the primary key in the
-- metadatas table includes the metadata_id as the first component.  Need to
-- verify this and drop the index or this comment appropriately.
CREATE INDEX index_metadatas_by_id
    ON metadatas (metadata_id);


-- Representation of a test program.
--
-- At the moment, there are no substantial differences between the
-- different interfaces, so we can simplify the design by with having a
-- single table representing all test caes.  We may need to revisit this in
-- the future.
CREATE TABLE test_programs (
    test_program_id INTEGER PRIMARY KEY AUTOINCREMENT,
    action_id INTEGER REFERENCES actions,

    -- The absolute path to the test program.  This should not be necessary
    -- because it is basically the concatenation of root and relative_path.
    -- However, this allows us to very easily search for test programs
    -- regardless of where they were executed from.  (I.e. different
    -- combinations of root + relative_path can map to the same absolute path).
    absolute_path TEXT NOT NULL,

    -- The path to the root of the test suite (where the Kyuafile lives).
    root TEXT NOT NULL,

    -- The path to the test program, relative to the root.
    relative_path TEXT NOT NULL,

    -- Name of the test suite the test program belongs to.
    test_suite_name TEXT NOT NULL,

    -- Reference to the various rows of metadatas.
    metadata_id INTEGER,

    -- The name of the test program interface.
    --
    -- Note that this indicates both the interface for the test program and
    -- its test cases.  See below for the corresponding detail tables.
    interface TEXT NOT NULL
);


-- Optimize the lookup of test programs by the action they belong to.
CREATE INDEX index_test_programs_by_action_id
    ON test_programs (action_id);


-- Representation of a test case.
--
-- At the moment, there are no substantial differences between the
-- different interfaces, so we can simplify the design by with having a
-- single table representing all test caes.  We may need to revisit this in
-- the future.
CREATE TABLE test_cases (
    test_case_id INTEGER PRIMARY KEY AUTOINCREMENT,
    test_program_id INTEGER REFERENCES test_programs,
    name TEXT NOT NULL,

    -- Reference to the various rows of metadatas.
    metadata_id INTEGER
);


-- Optimize the loading of all test cases that are part of a test program.
CREATE INDEX index_test_cases_by_test_programs_id
    ON test_cases (test_program_id);


-- Representation of test case results.
--
-- Note that there is a 1:1 relation between test cases and their results.
-- This is a result of storing the information of a test case on every
-- single action.
CREATE TABLE test_results (
    test_case_id INTEGER PRIMARY KEY REFERENCES test_cases,
    result_type TEXT NOT NULL,
    result_reason TEXT,

    start_time TIMESTAMP NOT NULL,
    end_time TIMESTAMP NOT NULL
);


-- Collection of output files of the test case.
CREATE TABLE test_case_files (
    test_case_id INTEGER NOT NULL REFERENCES test_cases,

    -- The raw name of the file.
    --
    -- The special names '__STDOUT__' and '__STDERR__' are reserved to hold
    -- the stdout and stderr of the test case, respectively.  If any of
    -- these are empty, there will be no corresponding entry in this table
    -- (hence why we do not allow NULLs in these fields).
    file_name TEXT NOT NULL,

    -- Pointer to the file itself.
    file_id INTEGER NOT NULL REFERENCES files,

    PRIMARY KEY (test_case_id, file_name)
);


-- -------------------------------------------------------------------------
-- Verbatim files.
-- -------------------------------------------------------------------------


-- Copies of files or logs generated during testing.
--
-- TODO(jmmv): This will probably grow to unmanageable sizes.  We should add a
-- hash to the file contents and use that as the primary key instead.
CREATE TABLE files (
    file_id INTEGER PRIMARY KEY,

    contents BLOB NOT NULL
);


-- -------------------------------------------------------------------------
-- Initialization of values.
-- -------------------------------------------------------------------------


-- Create a new metadata record.
--
-- For every new database, we want to ensure that the metadata is valid if
-- the database creation (i.e. the whole transaction) succeeded.
--
-- If you modify the value of the schema version in this statement, you
-- will also have to modify the version encoded in the backend module.
INSERT INTO metadata (timestamp, schema_version)
    VALUES (strftime('%s', 'now'), 2);


COMMIT TRANSACTION;
