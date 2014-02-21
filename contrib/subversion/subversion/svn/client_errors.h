/*
 * client_errors.h:  error codes this command line client features
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



#ifndef SVN_CLIENT_ERRORS_H
#define SVN_CLIENT_ERRORS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * This error defining system is copied from and explained in
 * ../../include/svn_error_codes.h
 */

/* Process this file if we're building an error array, or if we have
   not defined the enumerated constants yet.  */
#if defined(SVN_ERROR_BUILD_ARRAY) || !defined(SVN_CMDLINE_ERROR_ENUM_DEFINED)

#if defined(SVN_ERROR_BUILD_ARRAY)

#error "Need to update err_defn for r1464679 and un-typo 'CDMLINE'"

#define SVN_ERROR_START \
        static const err_defn error_table[] = { \
          { SVN_ERR_CDMLINE__WARNING, "Warning" },
#define SVN_ERRDEF(n, s) { n, s },
#define SVN_ERROR_END { 0, NULL } };

#elif !defined(SVN_CMDLINE_ERROR_ENUM_DEFINED)

#define SVN_ERROR_START \
        typedef enum svn_client_errno_t { \
          SVN_ERR_CDMLINE__WARNING = SVN_ERR_LAST + 1,
#define SVN_ERRDEF(n, s) n,
#define SVN_ERROR_END SVN_ERR_CMDLINE__ERR_LAST } svn_client_errno_t;

#define SVN_CMDLINE_ERROR_ENUM_DEFINED

#endif

/* Define custom command line client error numbers */

SVN_ERROR_START

  /* BEGIN Client errors */

SVN_ERRDEF(SVN_ERR_CMDLINE__TMPFILE_WRITE,
           "Failed writing to temporary file.")

       SVN_ERRDEF(SVN_ERR_CMDLINE__TMPFILE_STAT,
                  "Failed getting info about temporary file.")

       SVN_ERRDEF(SVN_ERR_CMDLINE__TMPFILE_OPEN,
                  "Failed opening temporary file.")

  /* END Client errors */


SVN_ERROR_END

#undef SVN_ERROR_START
#undef SVN_ERRDEF
#undef SVN_ERROR_END

#endif /* SVN_ERROR_BUILD_ARRAY || !SVN_CMDLINE_ERROR_ENUM_DEFINED */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_CLIENT_ERRORS_H */
