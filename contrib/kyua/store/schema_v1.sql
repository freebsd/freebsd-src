-- Copyright 2011 The Kyua Authors.
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

-- \file store/schema_v1.sql
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
-- a new record with a larger timestamp value, and never updating previous
-- records.  When extracting data from this table, the only "valid" row is
-- the one with the highest timestamp.  All the other rows are meaningless.
--
-- In other words, this table keeps the history of the database metadata.
-- The only reason for doing this is for debugging purposes.  It may come
-- in handy to know when a particular database-wide operation happened if
-- it turns out that the database got corrupted.
CREATE TABLE metadata (
    timestamp TIMESTAMP PRIMARY KEY CHECK (timestamp >= 0),
    schema_version INTEGER NOT NULL CHECK (schema_version >= 1)
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
    absolute_path NOT NULL,

    -- The path to the root of the test suite (where the Kyuafile lives).
    root TEXT NOT NULL,

    -- The path to the test program, relative to the root.
    relative_path NOT NULL,

    -- Name of the test suite the test program belongs to.
    test_suite_name TEXT NOT NULL,

    -- The name of the test program interface.
    --
    -- Note that this indicates both the interface for the test program and
    -- its test cases.  See below for the corresponding detail tables.
    interface TEXT NOT NULL
);


-- Representation of a test case.
--
-- At the moment, there are no substantial differences between the
-- different interfaces, so we can simplify the design by with having a
-- single table representing all test caes.  We may need to revisit this in
-- the future.
CREATE TABLE test_cases (
    test_case_id INTEGER PRIMARY KEY AUTOINCREMENT,
    test_program_id INTEGER REFERENCES test_programs,
    name TEXT NOT NULL
);


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
-- Detail tables for the 'atf' test interface.
-- -------------------------------------------------------------------------


-- Properties specific to 'atf' test cases.
--
-- This table contains the representation of singly-valued properties such
-- as 'timeout'.  Properties that can have more than one (textual) value
-- are stored in the atf_test_cases_multivalues table.
--
-- Note that all properties can be NULL because test cases are not required
-- to define them.
CREATE TABLE atf_test_cases (
    test_case_id INTEGER PRIMARY KEY REFERENCES test_cases,

    -- Free-form description of the text case.
    description TEXT,

    -- Either 'true' or 'false', indicating whether the test case has a
    -- cleanup routine or not.
    has_cleanup TEXT,

    -- The timeout for the test case in microseconds.
    timeout INTEGER,

    -- The amount of physical memory required by the test case.
    required_memory INTEGER,

    -- Either 'root' or 'unprivileged', indicating the privileges required by
    -- the test case.
    required_user TEXT
);


-- Representation of test case properties that have more than one value.
--
-- While we could store the flattened values of the properties as provided
-- by the test case itself, we choose to store the processed, split
-- representation.  This allows us to perform queries about the test cases
-- directly on the database without doing text processing; for example,
-- "get all test cases that require /bin/ls".
CREATE TABLE atf_test_cases_multivalues (
    test_case_id INTEGER REFERENCES test_cases,

    -- The name of the property; for example, 'require.progs'.
    property_name TEXT NOT NULL,

    -- One of the values of the property.
    property_value TEXT NOT NULL
);


-- -------------------------------------------------------------------------
-- Detail tables for the 'plain' test interface.
-- -------------------------------------------------------------------------


-- Properties specific to 'plain' test programs.
CREATE TABLE plain_test_programs (
    test_program_id INTEGER PRIMARY KEY REFERENCES test_programs,

    -- The timeout for the test cases in this test program.  While this
    -- setting has a default value for test programs, we explicitly record
    -- the information here.  The "default value" used when the test
    -- program was run might change over time, so we want to know what it
    -- was exactly when this was run.
    timeout INTEGER NOT NULL
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
    VALUES (strftime('%s', 'now'), 1);


COMMIT TRANSACTION;
