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

-- \file store/testdata_v1.sql
-- Populates a v1 database with some test data.


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

-- test_program_id 1
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, interface)
    VALUES (1, 2, '/test/suite/root/foo_test', '/test/suite/root',
            'foo_test', 'suite-name', 'plain');
INSERT INTO plain_test_programs (test_program_id, timeout)
    VALUES (1, 300000000);

-- test_case_id 1
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (1, 1, 'main');
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (1, 'passed', NULL, 1357643611000000, 1357643621000500);

-- test_program_id 2
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, interface)
    VALUES (2, 2, '/test/suite/root/subdir/another_test', '/test/suite/root',
            'subdir/another_test', 'subsuite-name', 'plain');
INSERT INTO plain_test_programs (test_program_id, timeout)
    VALUES (2, 10000000);

-- test_case_id 2
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (2, 2, 'main');
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

-- test_program_id 3
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, interface)
    VALUES (3, 2, '/test/suite/root/subdir/bar_test', '/test/suite/root',
            'subdir/bar_test', 'subsuite-name', 'plain');
INSERT INTO plain_test_programs (test_program_id, timeout)
    VALUES (3, 300000000);

-- test_case_id 3
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (3, 3, 'main');
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (3, 'broken', 'Received signal 1',
            1357643623500000, 1357643630981932);

-- test_program_id 4
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, interface)
    VALUES (4, 2, '/test/suite/root/top_test', '/test/suite/root',
            'top_test', 'suite-name', 'plain');
INSERT INTO plain_test_programs (test_program_id, timeout)
    VALUES (4, 300000000);

-- test_case_id 4
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (4, 4, 'main');
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (4, 'expected_failure', 'Known bug',
            1357643631000000, 1357643631020000);

-- test_program_id 5
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, interface)
    VALUES (5, 2, '/test/suite/root/last_test', '/test/suite/root',
            'last_test', 'suite-name', 'plain');
INSERT INTO plain_test_programs (test_program_id, timeout)
    VALUES (5, 300000000);

-- test_case_id 5
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (5, 5, 'main');
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

-- test_program_id 6
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, interface)
    VALUES (6, 3, '/usr/tests/complex_test', '/usr/tests',
            'complex_test', 'suite-name', 'atf');

-- test_case_id 6, passed, no optional metadata.
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (6, 6, 'this_passes');
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (6, 'passed', NULL, 1357648712000000, 1357648718000000);
INSERT INTO atf_test_cases (test_case_id, description, has_cleanup, timeout,
                            required_memory, required_user)
    VALUES (6, NULL, 'false', 300000000, 0, NULL);

-- test_case_id 7, failed, optional non-multivalue metadata.
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (7, 6, 'this_fails');
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (7, 'failed', 'Some reason', 1357648719000000, 1357648720897182);
INSERT INTO atf_test_cases (test_case_id, description, has_cleanup, timeout,
                            required_memory, required_user)
    VALUES (7, 'Test description', 'true', 300000000, 128, 'root');

-- test_case_id 8, skipped, all optional metadata.
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (8, 6, 'this_skips');
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (8, 'skipped', 'Another reason', 1357648729182013, 1357648730000000);
INSERT INTO atf_test_cases (test_case_id, description, has_cleanup, timeout,
                            required_memory, required_user)
    VALUES (8, 'Test explanation', 'true', 600000000, 512, 'unprivileged');
INSERT INTO atf_test_cases_multivalues (test_case_id, property_name,
                                        property_value)
    VALUES (8, 'require.arch', 'x86_64');
INSERT INTO atf_test_cases_multivalues (test_case_id, property_name,
                                        property_value)
    VALUES (8, 'require.arch', 'powerpc');
INSERT INTO atf_test_cases_multivalues (test_case_id, property_name,
                                        property_value)
    VALUES (8, 'require.machine', 'amd64');
INSERT INTO atf_test_cases_multivalues (test_case_id, property_name,
                                        property_value)
    VALUES (8, 'require.machine', 'macppc');
INSERT INTO atf_test_cases_multivalues (test_case_id, property_name,
                                        property_value)
    VALUES (8, 'require.config', 'unprivileged_user');
INSERT INTO atf_test_cases_multivalues (test_case_id, property_name,
                                        property_value)
    VALUES (8, 'require.config', 'X-foo');
INSERT INTO atf_test_cases_multivalues (test_case_id, property_name,
                                        property_value)
    VALUES (8, 'require.files', '/the/data/file');
INSERT INTO atf_test_cases_multivalues (test_case_id, property_name,
                                        property_value)
    VALUES (8, 'require.progs', 'cp');
INSERT INTO atf_test_cases_multivalues (test_case_id, property_name,
                                        property_value)
    VALUES (8, 'require.progs', '/bin/ls');

-- file_id 3
INSERT INTO files (file_id, contents)
    VALUES (3, x'416e6f74686572207374646f7574');
INSERT INTO test_case_files (test_case_id, file_name, file_id)
    VALUES (8, '__STDOUT__', 3);

-- test_program_id 7
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, interface)
    VALUES (7, 3, '/usr/tests/simple_test', '/usr/tests',
            'simple_test', 'subsuite-name', 'atf');

-- test_case_id 9
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (9, 7, 'main');
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (9, 'failed', 'Exited with code 1',
            1357648740120000, 1357648750081700);
INSERT INTO atf_test_cases (test_case_id, description, has_cleanup, timeout,
                            required_memory, required_user)
    VALUES (9, 'More text', 'true', 300000000, 128, 'unprivileged');

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

-- test_program_id 8
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, interface)
    VALUES (8, 4, '/usr/tests/subdir/another_test', '/usr/tests',
            'subdir/another_test', 'subsuite-name', 'plain');
INSERT INTO plain_test_programs (test_program_id, timeout)
    VALUES (8, 10000000);

-- test_case_id 10
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (10, 8, 'main');
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

-- test_program_id 9
INSERT INTO test_programs (test_program_id, action_id, absolute_path, root,
                           relative_path, test_suite_name, interface)
    VALUES (9, 4, '/usr/tests/complex_test', '/usr/tests',
            'complex_test', 'suite-name', 'atf');

-- test_case_id 11
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (11, 9, 'this_passes');
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (11, 'passed', NULL, 1357644396500000, 1357644397000000);
INSERT INTO atf_test_cases (test_case_id, description, has_cleanup, timeout,
                            required_memory, required_user)
    VALUES (11, NULL, 'false', 300000000, 0, NULL);

-- test_case_id 12
INSERT INTO test_cases (test_case_id, test_program_id, name)
    VALUES (12, 9, 'this_fails');
INSERT INTO test_results (test_case_id, result_type, result_reason, start_time,
                          end_time)
    VALUES (12, 'failed', 'Some reason', 1357644397100000, 1357644399005000);
INSERT INTO atf_test_cases (test_case_id, description, has_cleanup, timeout,
                            required_memory, required_user)
    VALUES (12, 'Test description', 'false', 300000000, 0, 'root');


COMMIT TRANSACTION;
