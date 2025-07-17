-- Copyright 2014 The Kyua Authors.
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

-- \file store/v2-to-v3.sql
-- Migration of a database with version 2 of the schema to version 3.
--
-- Version 3 appeared in revision 084d740b1da635946d153475156e335ddfc4aed6
-- and its changes were:
--
-- * Removal of historical data.
--
-- Because from v2 to v3 we went from a unified database to many separate
-- databases, this file is parameterized on @ACTION_ID@.  The file has to
-- be executed once per action with this string replaced.


ATTACH DATABASE "@OLD_DATABASE@" AS old_store;


-- New database already contains a record for v3.  Just import older entries.
INSERT INTO metadata SELECT * FROM old_store.metadata;

INSERT INTO contexts
    SELECT cwd
    FROM old_store.actions
        NATURAL JOIN old_store.contexts
    WHERE action_id == @ACTION_ID@;

INSERT INTO env_vars
    SELECT var_name, var_value
    FROM old_store.actions
        NATURAL JOIN old_store.contexts
        NATURAL JOIN old_store.env_vars
    WHERE action_id == @ACTION_ID@;

INSERT INTO metadatas
    SELECT metadata_id, property_name, property_value
    FROM old_store.metadatas
    WHERE metadata_id IN (
        SELECT test_programs.metadata_id
            FROM old_store.test_programs
            WHERE action_id == @ACTION_ID@
    ) OR metadata_id IN (
        SELECT test_cases.metadata_id
            FROM old_store.test_programs JOIN old_store.test_cases
                ON test_programs.test_program_id == test_cases.test_program_id
            WHERE action_id == @ACTION_ID@
    );

INSERT INTO test_programs
    SELECT test_program_id, absolute_path, root, relative_path,
        test_suite_name, metadata_id, interface
    FROM old_store.test_programs
    WHERE action_id == @ACTION_ID@;

INSERT INTO test_cases
    SELECT test_cases.test_case_id, test_cases.test_program_id,
        test_cases.name, test_cases.metadata_id
    FROM old_store.test_cases JOIN old_store.test_programs
        ON test_cases.test_program_id == test_programs.test_program_id
    WHERE action_id == @ACTION_ID@;

INSERT INTO test_results
    SELECT test_results.test_case_id, test_results.result_type,
    test_results.result_reason, test_results.start_time, test_results.end_time
    FROM old_store.test_results
        JOIN old_store.test_cases
            ON test_results.test_case_id == test_cases.test_case_id
        JOIN old_store.test_programs
            ON test_cases.test_program_id == test_programs.test_program_id
    WHERE action_id == @ACTION_ID@;

INSERT INTO files
    SELECT files.file_id, files.contents
    FROM old_store.files
        JOIN old_store.test_case_files
            ON files.file_id == test_case_files.file_id
        JOIN old_store.test_cases
            ON test_case_files.test_case_id == test_cases.test_case_id
        JOIN old_store.test_programs
            ON test_cases.test_program_id == test_programs.test_program_id
    WHERE action_id == @ACTION_ID@;

INSERT INTO test_case_files
    SELECT test_case_files.test_case_id, test_case_files.file_name,
        test_case_files.file_id
    FROM old_store.test_case_files
        JOIN old_store.test_cases
            ON test_case_files.test_case_id == test_cases.test_case_id
        JOIN old_store.test_programs
            ON test_cases.test_program_id == test_programs.test_program_id
    WHERE action_id == @ACTION_ID@;


DETACH DATABASE old_store;
