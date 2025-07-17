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
-- This contains 5 test programs, each with one test case, and each
-- reporting one of all possible result types.


BEGIN TRANSACTION;


-- context
INSERT INTO contexts (cwd) VALUES ('/test/suite/root');
INSERT INTO env_vars (var_name, var_value)
    VALUES ('HOME', '/home/test');
INSERT INTO env_vars (var_name, var_value)
    VALUES ('PATH', '/bin:/usr/bin');

-- metadata_id 1
INSERT INTO metadatas VALUES (1, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (1, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (1, 'description', '');
INSERT INTO metadatas VALUES (1, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (1, 'required_configs', '');
INSERT INTO metadatas VALUES (1, 'required_files', '');
INSERT INTO metadatas VALUES (1, 'required_memory', '0');
INSERT INTO metadatas VALUES (1, 'required_programs', '');
INSERT INTO metadatas VALUES (1, 'required_user', '');
INSERT INTO metadatas VALUES (1, 'timeout', '300');

-- test_program_id 1
INSERT INTO test_programs (test_program_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (1, '/test/suite/root/foo_test', '/test/suite/root',
            'foo_test', 'suite-name', 1, 'plain');

-- test_case_id 1
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (1, 1, 'main', 1);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (1, 'passed', NULL, 1357643611000000, 1357643621000500);

-- metadata_id 2
INSERT INTO metadatas VALUES (2, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (2, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (2, 'description', '');
INSERT INTO metadatas VALUES (2, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (2, 'required_configs', '');
INSERT INTO metadatas VALUES (2, 'required_files', '');
INSERT INTO metadatas VALUES (2, 'required_memory', '0');
INSERT INTO metadatas VALUES (2, 'required_programs', '');
INSERT INTO metadatas VALUES (2, 'required_user', '');
INSERT INTO metadatas VALUES (2, 'timeout', '10');

-- test_program_id 2
INSERT INTO test_programs (test_program_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (2, '/test/suite/root/subdir/another_test', '/test/suite/root',
            'subdir/another_test', 'subsuite-name', 2, 'plain');

-- test_case_id 2
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (2, 2, 'main', 2);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (2, 'failed', 'Exited with code 1',
            1357643622001200, 1357643622900021);

-- file_id 1
INSERT INTO files (file_id, contents) VALUES (1, x'54657374207374646f7574');
INSERT INTO test_case_files (test_case_id, file_name, file_id)
    VALUES (2, '__STDOUT__', 1);

-- file_id 2
INSERT INTO files (file_id, contents) VALUES (2, x'5465737420737464657272');
INSERT INTO test_case_files (test_case_id, file_name, file_id)
    VALUES (2, '__STDERR__', 2);

-- metadata_id 3
INSERT INTO metadatas VALUES (3, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (3, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (3, 'description', '');
INSERT INTO metadatas VALUES (3, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (3, 'required_configs', '');
INSERT INTO metadatas VALUES (3, 'required_files', '');
INSERT INTO metadatas VALUES (3, 'required_memory', '0');
INSERT INTO metadatas VALUES (3, 'required_programs', '');
INSERT INTO metadatas VALUES (3, 'required_user', '');
INSERT INTO metadatas VALUES (3, 'timeout', '300');

-- test_program_id 3
INSERT INTO test_programs (test_program_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (3, '/test/suite/root/subdir/bar_test', '/test/suite/root',
            'subdir/bar_test', 'subsuite-name', 3, 'plain');

-- test_case_id 3
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (3, 3, 'main', 3);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (3, 'broken', 'Received signal 1',
            1357643623500000, 1357643630981932);

-- metadata_id 4
INSERT INTO metadatas VALUES (4, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (4, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (4, 'description', '');
INSERT INTO metadatas VALUES (4, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (4, 'required_configs', '');
INSERT INTO metadatas VALUES (4, 'required_files', '');
INSERT INTO metadatas VALUES (4, 'required_memory', '0');
INSERT INTO metadatas VALUES (4, 'required_programs', '');
INSERT INTO metadatas VALUES (4, 'required_user', '');
INSERT INTO metadatas VALUES (4, 'timeout', '300');

-- test_program_id 4
INSERT INTO test_programs (test_program_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (4, '/test/suite/root/top_test', '/test/suite/root',
            'top_test', 'suite-name', 4, 'plain');

-- test_case_id 4
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (4, 4, 'main', 4);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (4, 'expected_failure', 'Known bug',
            1357643631000000, 1357643631020000);

-- metadata_id 5
INSERT INTO metadatas VALUES (5, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (5, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (5, 'description', '');
INSERT INTO metadatas VALUES (5, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (5, 'required_configs', '');
INSERT INTO metadatas VALUES (5, 'required_files', '');
INSERT INTO metadatas VALUES (5, 'required_memory', '0');
INSERT INTO metadatas VALUES (5, 'required_programs', '');
INSERT INTO metadatas VALUES (5, 'required_user', '');
INSERT INTO metadatas VALUES (5, 'timeout', '300');

-- test_program_id 5
INSERT INTO test_programs (test_program_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (5, '/test/suite/root/last_test', '/test/suite/root',
            'last_test', 'suite-name', 5, 'plain');

-- test_case_id 5
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (5, 5, 'main', 5);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (5, 'skipped', 'Does not apply', 1357643632000000, 1357643638000000);


COMMIT TRANSACTION;
