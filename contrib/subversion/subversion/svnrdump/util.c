/*
 *  util.c: A few utility functions.
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

#include "svn_error.h"
#include "svn_pools.h"
#include "svn_string.h"
#include "svn_props.h"
#include "svn_subst.h"

#include "svnrdump.h"


svn_error_t *
svn_rdump__normalize_prop(const char *name,
                          const svn_string_t **value,
                          apr_pool_t *result_pool)
{
  if (svn_prop_needs_translation(name))
    {
      const char *cstring;

      SVN_ERR(svn_subst_translate_cstring2((*value)->data, &cstring,
                                           "\n", TRUE,
                                           NULL, FALSE,
                                           result_pool));

      *value = svn_string_create(cstring, result_pool);
    }
  return SVN_NO_ERROR;
}

svn_error_t *
svn_rdump__normalize_props(apr_hash_t **normal_props,
                           apr_hash_t *props,
                           apr_pool_t *result_pool)
{
  apr_hash_index_t *hi;

  *normal_props = apr_hash_make(result_pool);

  for (hi = apr_hash_first(result_pool, props); hi;
        hi = apr_hash_next(hi))
    {
      const char *key = svn__apr_hash_index_key(hi);
      const svn_string_t *value = svn__apr_hash_index_val(hi);

      SVN_ERR(svn_rdump__normalize_prop(key, &value,
                                        result_pool));

      svn_hash_sets(*normal_props, key, value);
    }
  return SVN_NO_ERROR;
}
