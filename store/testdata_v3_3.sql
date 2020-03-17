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
-- ATF test programs only.


BEGIN TRANSACTION;


-- context
INSERT INTO contexts (cwd) VALUES ('/usr/tests');
INSERT INTO env_vars (var_name, var_value)
    VALUES ('PATH', '/bin:/usr/bin');

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
INSERT INTO test_programs (test_program_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (6, '/usr/tests/complex_test', '/usr/tests',
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
INSERT INTO test_programs (test_program_id, absolute_path, root,
                           relative_path, test_suite_name, metadata_id,
                           interface)
    VALUES (7, '/usr/tests/simple_test', '/usr/tests',
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


COMMIT TRANSACTION;
