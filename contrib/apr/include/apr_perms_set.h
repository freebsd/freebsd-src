/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef APR_PERMS_SET_H
#define APR_PERMS_SET_H

/**
 * @file apr_perms_set.h
 * @brief APR Process Locking Routines
 */

#include "apr.h"
#include "apr_pools.h"
#include "apr_errno.h"
#include "apr_user.h"
#include "apr_file_info.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @defgroup apr_perms_set Object permission set functions
 * @ingroup APR 
 * @{
 */

/** Permission set callback function. */
typedef apr_status_t (apr_perms_setfn_t)(void *object, apr_fileperms_t perms,
                                         apr_uid_t uid, apr_gid_t gid);

#define APR_PERMS_SET_IMPLEMENT(type) \
    APR_DECLARE(apr_status_t) apr_##type##_perms_set \
        (void *the##type, apr_fileperms_t perms, \
         apr_uid_t uid, apr_gid_t gid)

#define APR_PERMS_SET_ENOTIMPL(type) \
    APR_DECLARE(apr_status_t) apr_##type##_perms_set \
        (void *the##type, apr_fileperms_t perms, \
         apr_uid_t uid, apr_gid_t gid) \
        { return APR_ENOTIMPL ; }

#define APR_PERMS_SET_FN(type) apr_##type##_perms_set


/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* ! APR_PERMS_SET */
