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

-- \file store/testdata_v2.sql
-- Populates a v2 database with some test data.


BEGIN TRANSACTION;


--
-- Action 1: Empty context and no test programs nor test cases.
--


-- context_id 1
INSERT INTO contexts (context_id, cwd) VALUES (1, '/some/root');

-- action_id 1
INSERT INTO actions (action_id, context_id) VALUES (1, 1);


--
-- Action 2: Plain test programs only.
--
-- This action contains 5 test programs, each with one test case, and each
-- reporting one of all possible result types.
--


-- context_id 2
INSERT INTO contexts (context_id, cwd) VALUES (2, '/test/suite/root');
INSERT INTO env_vars (context_id, var_name, var_value)
    VALUES (2, 'HOME', '/home/test');
INSERT INTO env_vars (context_id, var_name, var_value)
    VALUES (2, 'PATH', '/bin:/usr/bin');

-- action_id 2
INSERT INTO actions (action_id, context_id) VALUES (2, 2);

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
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (1, 2, '/test/suite/root/foo_test', '/test/suite/root',
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
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (2, 2, '/test/suite/root/subdir/another_test', '/test/suite/root',
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
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (3, 2, '/test/suite/root/subdir/bar_test', '/test/suite/root',
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
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (4, 2, '/test/suite/root/top_test', '/test/suite/root',
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
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (5, 2, '/test/suite/root/last_test', '/test/suite/root',
            'last_test', 'suite-name', 5, 'plain');

-- test_case_id 5
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (5, 5, 'main', 5);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (5, 'skipped', 'Does not apply', 1357643632000000, 1357643638000000);


--
-- Action 3: ATF test programs only.
--


-- context_id 3
INSERT INTO contexts (context_id, cwd) VALUES (3, '/usr/tests');
INSERT INTO env_vars (context_id, var_name, var_value)
    VALUES (3, 'PATH', '/bin:/usr/bin');

-- action_id 3
INSERT INTO actions (action_id, context_id) VALUES (3, 3);

-- metadata_id 6
INSERT INTO metadatas VALUES (6, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (6, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (6, 'description', '');
INSERT INTO metadatas VALUES (6, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (6, 'required_configs', '');
INSERT INTO metadatas VALUES (6, 'required_files', '');
INSERT INTO metadatas VALUES (6, 'required_memory', '0');
INSERT INTO metadatas VALUES (6, 'required_programs', '');
INSERT INTO metadatas VALUES (6, 'required_user', '');
INSERT INTO metadatas VALUES (6, 'timeout', '300');

-- test_program_id 6
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (6, 3, '/usr/tests/complex_test', '/usr/tests',
            'complex_test', 'suite-name', 6, 'atf');

-- metadata_id 7
INSERT INTO metadatas VALUES (7, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (7, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (7, 'description', '');
INSERT INTO metadatas VALUES (7, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (7, 'required_configs', '');
INSERT INTO metadatas VALUES (7, 'required_files', '');
INSERT INTO metadatas VALUES (7, 'required_memory', '0');
INSERT INTO metadatas VALUES (7, 'required_programs', '');
INSERT INTO metadatas VALUES (7, 'required_user', '');
INSERT INTO metadatas VALUES (7, 'timeout', '300');

-- test_case_id 6, passed, no optional metadata.
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (6, 6, 'this_passes', 7);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (6, 'passed', NULL, 1357648712000000, 1357648718000000);

-- metadata_id 8
INSERT INTO metadatas VALUES (8, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (8, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (8, 'description', 'Test description');
INSERT INTO metadatas VALUES (8, 'has_cleanup', 'true');
INSERT INTO metadatas VALUES (8, 'required_configs', '');
INSERT INTO metadatas VALUES (8, 'required_files', '');
INSERT INTO metadatas VALUES (8, 'required_memory', '128');
INSERT INTO metadatas VALUES (8, 'required_programs', '');
INSERT INTO metadatas VALUES (8, 'required_user', 'root');
INSERT INTO metadatas VALUES (8, 'timeout', '300');

-- test_case_id 7, failed, optional non-multivalue metadata.
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (7, 6, 'this_fails', 8);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (7, 'failed', 'Some reason', 1357648719000000, 1357648720897182);

-- metadata_id 9
INSERT INTO metadatas VALUES (9, 'allowed_architectures', 'powerpc x86_64');
INSERT INTO metadatas VALUES (9, 'allowed_platforms', 'amd64 macppc');
INSERT INTO metadatas VALUES (9, 'description', 'Test explanation');
INSERT INTO metadatas VALUES (9, 'has_cleanup', 'true');
INSERT INTO metadatas VALUES (9, 'required_configs', 'unprivileged_user X-foo');
INSERT INTO metadatas VALUES (9, 'required_files', '/the/data/file');
INSERT INTO metadatas VALUES (9, 'required_memory', '512');
INSERT INTO metadatas VALUES (9, 'required_programs', 'cp /bin/ls');
INSERT INTO metadatas VALUES (9, 'required_user', 'unprivileged');
INSERT INTO metadatas VALUES (9, 'timeout', '600');

-- test_case_id 8, skipped, all optional metadata.
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (8, 6, 'this_skips', 9);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (8, 'skipped', 'Another reason', 1357648729182013, 1357648730000000);

-- file_id 3
INSERT INTO files (file_id, contents)
    VALUES (3, x'416e6f74686572207374646f7574');
INSERT INTO test_case_files (test_case_id, file_name, file_id)
    VALUES (8, '__STDOUT__', 3);

-- metadata_id 10
INSERT INTO metadatas VALUES (10, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (10, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (10, 'description', '');
INSERT INTO metadatas VALUES (10, 'has_cleanup', 'false');
INSERT INTO metadatas VALUES (10, 'required_configs', '');
INSERT INTO metadatas VALUES (10, 'required_files', '');
INSERT INTO metadatas VALUES (10, 'required_memory', '0');
INSERT INTO metadatas VALUES (10, 'required_programs', '');
INSERT INTO metadatas VALUES (10, 'required_user', '');
INSERT INTO metadatas VALUES (10, 'timeout', '300');

-- test_program_id 7
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (7, 3, '/usr/tests/simple_test', '/usr/tests',
            'simple_test', 'subsuite-name', 10, 'atf');

-- metadata_id 11
INSERT INTO metadatas VALUES (11, 'allowed_architectures', '');
INSERT INTO metadatas VALUES (11, 'allowed_platforms', '');
INSERT INTO metadatas VALUES (11, 'description', 'More text');
INSERT INTO metadatas VALUES (11, 'has_cleanup', 'true');
INSERT INTO metadatas VALUES (11, 'required_configs', '');
INSERT INTO metadatas VALUES (11, 'required_files', '');
INSERT INTO metadatas VALUES (11, 'required_memory', '128');
INSERT INTO metadatas VALUES (11, 'required_programs', '');
INSERT INTO metadatas VALUES (11, 'required_user', 'unprivileged');
INSERT INTO metadatas VALUES (11, 'timeout', '300');

-- test_case_id 9
INSERT INTO test_cases (test_case_id, test_program_id, name, metadata_id)
    VALUES (9, 7, 'main', 11);
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (9, 'failed', 'Exited with code 1',
            1357648740120000, 1357648750081700);

-- file_id 4
INSERT INTO files (file_id, contents)
    VALUES (4, x'416e6f7468657220737464657272');
INSERT INTO test_case_files (test_case_id, file_name, file_id)
    VALUES (9, '__STDERR__', 4);


--
-- Action 4: Mixture of test programs.
--


-- context_id 4
INSERT INTO contexts (context_id, cwd) VALUES (4, '/usr/tests');
INSERT INTO env_vars (context_id, var_name, var_value)
    VALUES (4, 'LANG', 'C');
INSERT INTO env_vars (context_id, var_name, var_value)
    VALUES (4, 'PATH', '/bin:/usr/bin');
INSERT INTO env_vars (context_id, var_name, var_value)
    VALUES (4, 'TERM', 'xterm');

-- action_id 4
INSERT INTO actions (action_id, context_id) VALUES (4, 4);

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
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (8, 4, '/usr/tests/subdir/another_test', '/usr/tests',
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
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (9, 4, '/usr/tests/complex_test', '/usr/tests',
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
