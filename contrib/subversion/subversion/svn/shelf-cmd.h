/*
 * shelf-cmd.h:  experimental shelving v3
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

/* ==================================================================== */



#ifndef SVN_SHELF_CMD_H
#define SVN_SHELF_CMD_H

/*** Includes. ***/
#include <apr_getopt.h>

#include "svn_opt.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


extern const svn_opt_subcommand_desc3_t svn_cl__cmd_table_shelf3[];
/*extern const apr_getopt_option_t svn_cl__options_shelving3[];*/


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_SHELF_CMD_H */
