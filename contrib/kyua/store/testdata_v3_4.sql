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

-- \file store/testdata_v3.sql
-- Populates a v3 database with some test data.
--
-- Mixture of test programs.


BEGIN TRANSACTION;


-- context
INSERT INTO contexts (cwd) VALUES ('/usr/tests');
INSERT INTO env_vars (var_name, var_value)
    VALUES ('LANG', 'C');
INSERT INTO env_vars (var_name, var_value)
    VALUES ('PATH', '/bin:/usr/bin');
INSERT INTO env_vars (var_name, var_value)
    VALUES ('TERM', 'xterm');

-- metadata_id 12
INSERT INTO metadatas VALUES (12, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (12, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (12, 'description', '');
INSERT INTO metadatas VALUES (12, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (12, 'required_configs', '');
INSERT INTO metadatas VALUES (12, 'required_files', '');
INSERT INTO metadatas VALUES (12, 'required_memory', '0');
INSERT INTO metadatas VALUES (12, 'required_programs', '');
INSERT INTO metadatas VALUES (12, 'required_user', '');
INSERT INTO metadatas VALUES (12, 'timeout', '10');

-- test_program_id 8
INSERT INTO test_programs (test_program_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (8, '/usr/tests/subdir/another_test', '/usr/tests',
            'subdir/another_test', 'subsuite-name', 12, 'plain');

-- test_case_id 10
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (10, 8, 'main', 12);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (10, 'failed', 'Exit failure', 1357644395000000, 1357644396000000);

-- file_id 5
INSERT INTO files (file_id, contents) VALUES (5, x'54657374207374646f7574');
INSERT INTO test_case_files (test_case_id, file_name, file_id)
    VALUES (10, '__STDOUT__', 5);

-- file_id 6
INSERT INTO files (file_id, contents) VALUES (6, x'5465737420737464657272');
INSERT INTO test_case_files (test_case_id, file_name, file_id)
    VALUES (10, '__STDERR__', 6);

-- metadata_id 13
INSERT INTO metadatas VALUES (13, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (13, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (13, 'description', '');
INSERT INTO metadatas VALUES (13, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (13, 'required_configs', '');
INSERT INTO metadatas VALUES (13, 'required_files', '');
INSERT INTO metadatas VALUES (13, 'required_memory', '0');
INSERT INTO metadatas VALUES (13, 'required_programs', '');
INSERT INTO metadatas VALUES (13, 'required_user', '');
INSERT INTO metadatas VALUES (13, 'timeout', '300');

-- test_program_id 9
INSERT INTO test_programs (test_program_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (9, '/usr/tests/complex_test', '/usr/tests',
            'complex_test', 'suite-name', 14, 'atf');

-- metadata_id 15
INSERT INTO metadatas VALUES (15, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (15, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (15, 'description', '');
INSERT INTO metadatas VALUES (15, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (15, 'required_configs', '');
INSERT INTO metadatas VALUES (15, 'required_files', '');
INSERT INTO metadatas VALUES (15, 'required_memory', '0');
INSERT INTO metadatas VALUES (15, 'required_programs', '');
INSERT INTO metadatas VALUES (15, 'required_user', '');
INSERT INTO metadatas VALUES (15, 'timeout', '300');

-- test_case_id 11
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (11, 9, 'this_passes', 15);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (11, 'passed', NULL, 1357644396500000, 1357644397000000);

-- metadata_id 16
INSERT INTO metadatas VALUES (16, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (16, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (16, 'description', 'Test description');
INSERT INTO metadatas VALUES (16, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (16, 'required_configs', '');
INSERT INTO metadatas VALUES (16, 'required_files', '');
INSERT INTO metadatas VALUES (16, 'required_memory', '0');
INSERT INTO metadatas VALUES (16, 'required_programs', '');
INSERT INTO metadatas VALUES (16, 'required_user', 'root');
INSERT INTO metadatas VALUES (16, 'timeout', '300');

-- test_case_id 12
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (12, 9, 'this_fails', 16);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (12, 'failed', 'Some reason', 1357644397100000, 1357644399005000);


COMMIT TRANSACTION;
