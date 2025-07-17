-- Copyright 2013 The Kyua Authors.
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

-- \file store/v1-to-v2.sql
-- Migration of a database with version 1 of the schema to version 2.
--
-- Version 2 appeared in revision 9a73561a1e3975bba4cbfd19aee6b2365a39519e
-- and its changes were:
--
-- * Changed the primary key of the metadata table to be the
--   schema_version, not the timestamp.  Because timestamps only have
--   second resolution, the old schema made testing of schema migrations
--   difficult.
--
-- * Introduced the metadatas table, which holds the metadata of all test
--   programs and test cases in an abstract manner regardless of their
--   interface.
--
-- * Added the metadata_id field to the test_programs and test_cases
--   tables, referencing the new metadatas table.
--
-- * Changed the precision of the timeout metadata field to be in seconds
--   rather than in microseconds.  There is no data loss, and the code that
--   writes the metadata is simplified.
--
-- * Removed the atf_* and plain_* tables.
--
-- * Added missing indexes to improve the performance of reports.
--
-- * Added missing column affinities to the absolute_path and relative_path
--   columns of the test_programs table.


-- TODO(jmmv): Implement addition of missing affinities.


--
-- Change primary key of the metadata table.
--


CREATE TABLE new_metadata (
    schema_version INTEGER PRIMARY KEY CHECK (schema_version >= 1),
    timestamp TIMESTAMP NOT NULL CHECK (timestamp >= 0)
);

INSERT INTO new_metadata (schema_version, timestamp)
    SELECT schema_version, timestamp FROM metadata;

DROP TABLE metadata;
ALTER TABLE new_metadata RENAME TO metadata;


--
-- Add the new tables, columns and indexes.
--


CREATE TABLE metadatas (
    metadata_id INTEGER NOT NULL,
    property_name TEXT NOT NULL,
    property_value TEXT,

    PRIMARY KEY (metadata_id, property_name)
);


-- Upgrade the test_programs table by adding missing column affinities and
-- the new metadata_id column.
CREATE TABLE new_test_programs (
    test_program_id INTEGER PRIMARY KEY AUTOINCREMENT,
    action_id INTEGER REFERENCES actions,

    absolute_path TEXT NOT NULL,
    root TEXT NOT NULL,
    relative_path TEXT NOT NULL,
    test_suite_name TEXT NOT NULL,
    metadata_id INTEGER,
    interface TEXT NOT NULL
);
PRAGMA foreign_keys = OFF;
INSERT INTO new_test_programs (test_program_id, action_id, absolute_path,
                               root, relative_path, test_suite_name,
                               interface)
    SELECT test_program_id, action_id, absolute_path, root, relative_path,
        test_suite_name, interface FROM test_programs;
DROP TABLE test_programs;
ALTER TABLE new_test_programs RENAME TO test_programs;
PRAGMA foreign_keys = ON;


ALTER TABLE test_cases ADD COLUMN metadata_id INTEGER;


CREATE INDEX index_metadatas_by_id
    ON metadatas (metadata_id);
CREATE INDEX index_test_programs_by_action_id
    ON test_programs (action_id);
CREATE INDEX index_test_cases_by_test_programs_id
    ON test_cases (test_program_id);


--
-- Data migration
--
-- This is, by far, the trickiest part of the migration.
-- TODO(jmmv): Describe the trickiness in here.
--


-- Auxiliary table to construct the final contents of the metadatas table.
--
-- We construct the contents by writing a row for every metadata property of
-- every test program and test case.  Entries corresponding to a test program
-- will have the test_program_id field set to not NULL and entries corresponding
-- to test cases will have the test_case_id set to not NULL.
--
-- The tricky part, however, is to create the individual identifiers for every
-- metadata entry.  We do this by picking the minimum ROWID of a particular set
-- of properties that map to a single test_program_id or test_case_id.
CREATE TABLE tmp_metadatas (
    test_program_id INTEGER DEFAULT NULL,
    test_case_id INTEGER DEFAULT NULL,
    interface TEXT NOT NULL,
    property_name TEXT NOT NULL,
    property_value TEXT NOT NULL,

    UNIQUE (test_program_id, test_case_id, property_name)
);
CREATE INDEX index_tmp_metadatas_by_test_case_id
    ON tmp_metadatas (test_case_id);
CREATE INDEX index_tmp_metadatas_by_test_program_id
    ON tmp_metadatas (test_program_id);


-- Populate default metadata values for all test programs and test cases.
--
-- We do this first to ensure that all test programs and test cases have
-- explicit values for their metadata.  Because we want to keep historical data
-- for the tests, we must record these values unconditionally instead of relying
-- on the built-in values in the code.
--
-- Once this is done, we override any values explicity set by the tests.
CREATE TABLE tmp_default_metadata (
    default_name TEXT PRIMARY KEY,
    default_value TEXT NOT NULL
);
INSERT INTO tmp_default_metadata VALUES ('allowed_architectures', '');
INSERT INTO tmp_default_metadata VALUES ('allowed_platforms', '');
INSERT INTO tmp_default_metadata VALUES ('description', '');
INSERT INTO tmp_default_metadata VALUES ('has_cleanup', 'false');
INSERT INTO tmp_default_metadata VALUES ('required_configs', '');
INSERT INTO tmp_default_metadata VALUES ('required_files', '');
INSERT INTO tmp_default_metadata VALUES ('required_memory', '0');
INSERT INTO tmp_default_metadata VALUES ('required_programs', '');
INSERT INTO tmp_default_metadata VALUES ('required_user', '');
INSERT INTO tmp_default_metadata VALUES ('timeout', '300');
INSERT INTO tmp_metadatas
    SELECT test_program_id, NULL, interface, default_name, default_value
        FROM test_programs JOIN tmp_default_metadata;
INSERT INTO tmp_metadatas
    SELECT NULL, test_case_id, interface, default_name, default_value
        FROM test_programs JOIN test_cases
        ON test_cases.test_program_id = test_programs.test_program_id
        JOIN tmp_default_metadata;
DROP TABLE tmp_default_metadata;


-- Populate metadata overrides from plain test programs.
UPDATE tmp_metadatas
    SET property_value = (
        SELECT CAST(timeout / 1000000 AS TEXT) FROM plain_test_programs AS aux
            WHERE aux.test_program_id = tmp_metadatas.test_program_id)
    WHERE test_program_id IS NOT NULL AND property_name = 'timeout'
        AND interface = 'plain';
UPDATE tmp_metadatas
    SET property_value = (
        SELECT DISTINCT CAST(timeout / 1000000 AS TEXT)
        FROM test_cases AS aux JOIN plain_test_programs
            ON aux.test_program_id == plain_test_programs.test_program_id
        WHERE aux.test_case_id = tmp_metadatas.test_case_id)
    WHERE test_case_id IS NOT NULL AND property_name = 'timeout'
        AND interface = 'plain';


CREATE INDEX index_tmp_atf_test_cases_multivalues_by_test_case_id
    ON atf_test_cases_multivalues (test_case_id);


-- Populate metadata overrides from ATF test cases.
UPDATE atf_test_cases SET description = '' WHERE description IS NULL;
UPDATE atf_test_cases SET required_user = '' WHERE required_user IS NULL;

UPDATE tmp_metadatas
    SET property_value = (
        SELECT description FROM atf_test_cases AS aux
            WHERE aux.test_case_id = tmp_metadatas.test_case_id)
    WHERE test_case_id IS NOT NULL AND property_name = 'description'
        AND interface = 'atf';
UPDATE tmp_metadatas
    SET property_value = (
        SELECT has_cleanup FROM atf_test_cases AS aux
            WHERE aux.test_case_id = tmp_metadatas.test_case_id)
    WHERE test_case_id IS NOT NULL AND property_name = 'has_cleanup'
        AND interface = 'atf';
UPDATE tmp_metadatas
    SET property_value = (
        SELECT CAST(timeout / 1000000 AS TEXT) FROM atf_test_cases AS aux
            WHERE aux.test_case_id = tmp_metadatas.test_case_id)
    WHERE test_case_id IS NOT NULL AND property_name = 'timeout'
        AND interface = 'atf';
UPDATE tmp_metadatas
    SET property_value = (
        SELECT CAST(required_memory AS TEXT) FROM atf_test_cases AS aux
            WHERE aux.test_case_id = tmp_metadatas.test_case_id)
    WHERE test_case_id IS NOT NULL AND property_name = 'required_memory'
        AND interface = 'atf';
UPDATE tmp_metadatas
    SET property_value = (
        SELECT required_user FROM atf_test_cases AS aux
            WHERE aux.test_case_id = tmp_metadatas.test_case_id)
    WHERE test_case_id IS NOT NULL AND property_name = 'required_user'
        AND interface = 'atf';
UPDATE tmp_metadatas
    SET property_value = (
        SELECT GROUP_CONCAT(aux.property_value, ' ')
            FROM atf_test_cases_multivalues AS aux
            WHERE aux.test_case_id = tmp_metadatas.test_case_id AND
                aux.property_name = 'require.arch')
    WHERE test_case_id IS NOT NULL AND property_name = 'allowed_architectures'
        AND interface = 'atf'
        AND EXISTS(SELECT 1 FROM atf_test_cases_multivalues AS aux
                   WHERE aux.test_case_id = tmp_metadatas.test_case_id
                   AND property_name = 'require.arch');
UPDATE tmp_metadatas
    SET property_value = (
        SELECT GROUP_CONCAT(aux.property_value, ' ')
            FROM atf_test_cases_multivalues AS aux
            WHERE aux.test_case_id = tmp_metadatas.test_case_id AND
                aux.property_name = 'require.machine')
    WHERE test_case_id IS NOT NULL AND property_name = 'allowed_platforms'
        AND interface = 'atf'
        AND EXISTS(SELECT 1 FROM atf_test_cases_multivalues AS aux
                   WHERE aux.test_case_id = tmp_metadatas.test_case_id
                   AND property_name = 'require.machine');
UPDATE tmp_metadatas
    SET property_value = (
        SELECT GROUP_CONCAT(aux.property_value, ' ')
            FROM atf_test_cases_multivalues AS aux
            WHERE aux.test_case_id = tmp_metadatas.test_case_id AND
                aux.property_name = 'require.config')
    WHERE test_case_id IS NOT NULL AND property_name = 'required_configs'
        AND interface = 'atf'
        AND EXISTS(SELECT 1 FROM atf_test_cases_multivalues AS aux
                   WHERE aux.test_case_id = tmp_metadatas.test_case_id
                   AND property_name = 'require.config');
UPDATE tmp_metadatas
    SET property_value = (
        SELECT GROUP_CONCAT(aux.property_value, ' ')
            FROM atf_test_cases_multivalues AS aux
            WHERE aux.test_case_id = tmp_metadatas.test_case_id AND
                aux.property_name = 'require.files')
    WHERE test_case_id IS NOT NULL AND property_name = 'required_files'
        AND interface = 'atf'
        AND EXISTS(SELECT 1 FROM atf_test_cases_multivalues AS aux
                   WHERE aux.test_case_id = tmp_metadatas.test_case_id
                   AND property_name = 'require.files');
UPDATE tmp_metadatas
    SET property_value = (
        SELECT GROUP_CONCAT(aux.property_value, ' ')
            FROM atf_test_cases_multivalues AS aux
            WHERE aux.test_case_id = tmp_metadatas.test_case_id AND
                aux.property_name = 'require.progs')
    WHERE test_case_id IS NOT NULL AND property_name = 'required_programs'
        AND interface = 'atf'
        AND EXISTS(SELECT 1 FROM atf_test_cases_multivalues AS aux
                   WHERE aux.test_case_id = tmp_metadatas.test_case_id
                   AND property_name = 'require.progs');


-- Fill metadata_id pointers in the test_programs and test_cases tables.
UPDATE test_programs
    SET metadata_id = (
        SELECT MIN(ROWID) FROM tmp_metadatas
            WHERE tmp_metadatas.test_program_id = test_programs.test_program_id
    );
UPDATE test_cases
    SET metadata_id = (
        SELECT MIN(ROWID) FROM tmp_metadatas
            WHERE tmp_metadatas.test_case_id = test_cases.test_case_id
    );


-- Populate the metadatas table based on tmp_metadatas.
INSERT INTO metadatas (metadata_id, property_name, property_value)
    SELECT (
        SELECT MIN(ROWID) FROM tmp_metadatas AS s
        WHERE s.test_program_id = tmp_metadatas.test_program_id
    ), property_name, property_value
    FROM tmp_metadatas WHERE test_program_id IS NOT NULL;
INSERT INTO metadatas (metadata_id, property_name, property_value)
    SELECT (
        SELECT MIN(ROWID) FROM tmp_metadatas AS s
        WHERE s.test_case_id = tmp_metadatas.test_case_id
    ), property_name, property_value
    FROM tmp_metadatas WHERE test_case_id IS NOT NULL;


-- Drop temporary entities used during the migration.
DROP INDEX index_tmp_atf_test_cases_multivalues_by_test_case_id;
DROP INDEX index_tmp_metadatas_by_test_program_id;
DROP INDEX index_tmp_metadatas_by_test_case_id;
DROP TABLE tmp_metadatas;


--
-- Drop obsolete tables.
--


DROP TABLE atf_test_cases;
DROP TABLE atf_test_cases_multivalues;
DROP TABLE plain_test_programs;


--
-- Update the metadata version.
--


INSERT INTO metadata (timestamp, schema_version)
    VALUES (strftime('%s', 'now'), 2);
