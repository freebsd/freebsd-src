/* wc-checks.sql -- trigger-based checks for the wc-metadata database.
 *     This is intended for use with SQLite 3
 *
 * ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 */


-- STMT_VERIFICATION_TRIGGERS

/* ------------------------------------------------------------------------- */

CREATE TEMPORARY TRIGGER no_repository_updates BEFORE UPDATE ON repository
BEGIN
  SELECT RAISE(FAIL, 'Updates to REPOSITORY are not allowed.');
END;

/* ------------------------------------------------------------------------- */

/* Verify: on every NODES row: parent_relpath is parent of local_relpath */
CREATE TEMPORARY TRIGGER validation_01 BEFORE INSERT ON nodes
WHEN NOT ((new.local_relpath = '' AND new.parent_relpath IS NULL)
          OR (relpath_depth(new.local_relpath)
              = relpath_depth(new.parent_relpath) + 1))
BEGIN
  SELECT RAISE(FAIL, 'WC DB validity check 01 failed');
END;

/* Verify: on every NODES row: its op-depth <= its own depth */
CREATE TEMPORARY TRIGGER validation_02 BEFORE INSERT ON nodes
WHEN NOT new.op_depth <= relpath_depth(new.local_relpath)
BEGIN
  SELECT RAISE(FAIL, 'WC DB validity check 02 failed');
END;

/* Verify: on every NODES row: it is an op-root or it has a parent with the
    sames op-depth. (Except when the node is a file external) */
CREATE TEMPORARY TRIGGER validation_03 BEFORE INSERT ON nodes
WHEN NOT (
    (new.op_depth = relpath_depth(new.local_relpath))
    OR
    (EXISTS (SELECT 1 FROM nodes
              WHERE wc_id = new.wc_id AND op_depth = new.op_depth
                AND local_relpath = new.parent_relpath))
  )
 AND NOT (new.file_external IS NOT NULL AND new.op_depth = 0)
BEGIN
  SELECT RAISE(FAIL, 'WC DB validity check 03 failed');
END;

/* Verify: on every ACTUAL row (except root): a NODES row exists at its
 * parent path. */
CREATE TEMPORARY TRIGGER validation_04 BEFORE INSERT ON actual_node
WHEN NOT (new.local_relpath = ''
          OR EXISTS (SELECT 1 FROM nodes
                       WHERE wc_id = new.wc_id
                         AND local_relpath = new.parent_relpath))
BEGIN
  SELECT RAISE(FAIL, 'WC DB validity check 04 failed');
END;

