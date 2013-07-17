/*
 * auth.c: authentication support functions for Subversion
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


#include <apr_pools.h>
#include <apr_tables.h>
#include <apr_strings.h>

#include "svn_hash.h"
#include "svn_types.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_auth.h"
#include "svn_config.h"
#include "svn_private_config.h"
#include "svn_dso.h"
#include "svn_version.h"
#include "private/svn_dep_compat.h"

#include "auth.h"

/* AN OVERVIEW
   ===========

   A good way to think of this machinery is as a set of tables.

     - Each type of credentials selects a single table.

     - In a given table, each row is a 'provider' capable of returning
       the same type of credentials.  Each column represents a
       provider's repeated attempts to provide credentials.


   Fetching Credentials from Providers
   -----------------------------------

   When the caller asks for a particular type of credentials, the
   machinery in this file walks over the appropriate table.  It starts
   with the first provider (first row), and calls first_credentials()
   to get the first set of credentials (first column).  If the caller
   is unhappy with the credentials, then each subsequent call to
   next_credentials() traverses the row from left to right.  If the
   provider returns error at any point, then we go to the next provider
   (row).  We continue this way until every provider fails, or
   until the client is happy with the returned credentials.

   Note that the caller cannot see the table traversal, and thus has
   no idea when we switch providers.


   Storing Credentials with Providers
   ----------------------------------

   When the server has validated a set of credentials, and when
   credential caching is enabled, we have the chance to store those
   credentials for later use.  The provider which provided the working
   credentials is the first one given the opportunity to (re)cache
   those credentials.  Its save_credentials() function is invoked with
   the working credentials.  If that provider reports that it
   successfully stored the credentials, we're done.  Otherwise, we
   walk the providers (rows) for that type of credentials in order
   from the top of the table, allowing each in turn the opportunity to
   store the credentials.  When one reports that it has done so
   successfully -- or when we run out of providers (rows) to try --
   the table walk ends.
*/



/* This effectively defines a single table.  Every provider in this
   array returns the same kind of credentials. */
typedef struct provider_set_t
{
  /* ordered array of svn_auth_provider_object_t */
  apr_array_header_t *providers;

} provider_set_t;


/* The main auth baton. */
struct svn_auth_baton_t
{
  /* a collection of tables.  maps cred_kind -> provider_set */
  apr_hash_t *tables;

  /* the pool I'm allocated in. */
  apr_pool_t *pool;

  /* run-time parameters needed by providers. */
  apr_hash_t *parameters;

  /* run-time credentials cache. */
  apr_hash_t *creds_cache;

};

/* Abstracted iteration baton */
struct svn_auth_iterstate_t
{
  provider_set_t *table;        /* the table being searched */
  int provider_idx;             /* the current provider (row) */
  svn_boolean_t got_first;      /* did we get the provider's first creds? */
  void *provider_iter_baton;    /* the provider's own iteration context */
  const char *realmstring;      /* The original realmstring passed in */
  const char *cache_key;        /* key to use in auth_baton's creds_cache */
  svn_auth_baton_t *auth_baton; /* the original auth_baton. */
};



void
svn_auth_open(svn_auth_baton_t **auth_baton,
              const apr_array_header_t *providers,
              apr_pool_t *pool)
{
  svn_auth_baton_t *ab;
  svn_auth_provider_object_t *provider;
  int i;

  /* Build the auth_baton. */
  ab = apr_pcalloc(pool, sizeof(*ab));
  ab->tables = apr_hash_make(pool);
  ab->parameters = apr_hash_make(pool);
  ab->creds_cache = apr_hash_make(pool);
  ab->pool = pool;

  /* Register each provider in order.  Providers of different
     credentials will be automatically sorted into different tables by
     register_provider(). */
  for (i = 0; i < providers->nelts; i++)
    {
      provider_set_t *table;
      provider = APR_ARRAY_IDX(providers, i, svn_auth_provider_object_t *);

      /* Add it to the appropriate table in the auth_baton */
      table = svn_hash_gets(ab->tables, provider->vtable->cred_kind);
      if (! table)
        {
          table = apr_pcalloc(pool, sizeof(*table));
          table->providers
            = apr_array_make(pool, 1, sizeof(svn_auth_provider_object_t *));

          svn_hash_sets(ab->tables, provider->vtable->cred_kind, table);
        }
      APR_ARRAY_PUSH(table->providers, svn_auth_provider_object_t *)
        = provider;
    }

  *auth_baton = ab;
}



void
svn_auth_set_parameter(svn_auth_baton_t *auth_baton,
                       const char *name,
                       const void *value)
{
  svn_hash_sets(auth_baton->parameters, name, value);
}

const void *
svn_auth_get_parameter(svn_auth_baton_t *auth_baton,
                       const char *name)
{
  return svn_hash_gets(auth_baton->parameters, name);
}


/* Return the key used to address the in-memory cache of auth
   credentials of type CRED_KIND and associated with REALMSTRING. */
static const char *
make_cache_key(const char *cred_kind,
               const char *realmstring,
               apr_pool_t *pool)
{
  return apr_pstrcat(pool, cred_kind, ":", realmstring, (char *)NULL);
}

svn_error_t *
svn_auth_first_credentials(void **credentials,
                           svn_auth_iterstate_t **state,
                           const char *cred_kind,
                           const char *realmstring,
                           svn_auth_baton_t *auth_baton,
                           apr_pool_t *pool)
{
  int i = 0;
  provider_set_t *table;
  svn_auth_provider_object_t *provider = NULL;
  void *creds = NULL;
  void *iter_baton = NULL;
  svn_boolean_t got_first = FALSE;
  svn_auth_iterstate_t *iterstate;
  const char *cache_key;

  /* Get the appropriate table of providers for CRED_KIND. */
  table = svn_hash_gets(auth_baton->tables, cred_kind);
  if (! table)
    return svn_error_createf(SVN_ERR_AUTHN_NO_PROVIDER, NULL,
                             _("No provider registered for '%s' credentials"),
                             cred_kind);

  /* First, see if we have cached creds in the auth_baton. */
  cache_key = make_cache_key(cred_kind, realmstring, pool);
  creds = svn_hash_gets(auth_baton->creds_cache, cache_key);
  if (creds)
    {
       got_first = FALSE;
    }
  else
    /* If not, find a provider that can give "first" credentials. */
    {
      /* Find a provider that can give "first" credentials. */
      for (i = 0; i < table->providers->nelts; i++)
        {
          provider = APR_ARRAY_IDX(table->providers, i,
                                   svn_auth_provider_object_t *);
          SVN_ERR(provider->vtable->first_credentials(&creds, &iter_baton,
                                                      provider->provider_baton,
                                                      auth_baton->parameters,
                                                      realmstring,
                                                      auth_baton->pool));

          if (creds != NULL)
            {
              got_first = TRUE;
              break;
            }
        }
    }

  if (! creds)
    *state = NULL;
  else
    {
      /* Build an abstract iteration state. */
      iterstate = apr_pcalloc(pool, sizeof(*iterstate));
      iterstate->table = table;
      iterstate->provider_idx = i;
      iterstate->got_first = got_first;
      iterstate->provider_iter_baton = iter_baton;
      iterstate->realmstring = apr_pstrdup(pool, realmstring);
      iterstate->cache_key = cache_key;
      iterstate->auth_baton = auth_baton;
      *state = iterstate;

      /* Put the creds in the cache */
      svn_hash_sets(auth_baton->creds_cache,
                    apr_pstrdup(auth_baton->pool, cache_key),
                    creds);
    }

  *credentials = creds;

  return SVN_NO_ERROR;
}


svn_error_t *
svn_auth_next_credentials(void **credentials,
                          svn_auth_iterstate_t *state,
                          apr_pool_t *pool)
{
  svn_auth_baton_t *auth_baton = state->auth_baton;
  svn_auth_provider_object_t *provider;
  provider_set_t *table = state->table;
  void *creds = NULL;

  /* Continue traversing the table from where we left off. */
  for (/* no init */;
       state->provider_idx < table->providers->nelts;
       state->provider_idx++)
    {
      provider = APR_ARRAY_IDX(table->providers,
                               state->provider_idx,
                               svn_auth_provider_object_t *);
      if (! state->got_first)
        {
          SVN_ERR(provider->vtable->first_credentials(
                      &creds, &(state->provider_iter_baton),
                      provider->provider_baton, auth_baton->parameters,
                      state->realmstring, auth_baton->pool));
          state->got_first = TRUE;
        }
      else if (provider->vtable->next_credentials)
        {
          SVN_ERR(provider->vtable->next_credentials(
                      &creds, state->provider_iter_baton,
                      provider->provider_baton, auth_baton->parameters,
                      state->realmstring, auth_baton->pool));
        }

      if (creds != NULL)
        {
          /* Put the creds in the cache */
          svn_hash_sets(auth_baton->creds_cache, state->cache_key, creds);
          break;
        }

      state->got_first = FALSE;
    }

  *credentials = creds;

  return SVN_NO_ERROR;
}


svn_error_t *
svn_auth_save_credentials(svn_auth_iterstate_t *state,
                          apr_pool_t *pool)
{
  int i;
  svn_auth_provider_object_t *provider;
  svn_boolean_t save_succeeded = FALSE;
  const char *no_auth_cache;
  svn_auth_baton_t *auth_baton;
  void *creds;

  if (! state || state->table->providers->nelts <= state->provider_idx)
    return SVN_NO_ERROR;

  auth_baton = state->auth_baton;
  creds = svn_hash_gets(state->auth_baton->creds_cache, state->cache_key);
  if (! creds)
    return SVN_NO_ERROR;

  /* Do not save the creds if SVN_AUTH_PARAM_NO_AUTH_CACHE is set */
  no_auth_cache = svn_hash_gets(auth_baton->parameters,
                                SVN_AUTH_PARAM_NO_AUTH_CACHE);
  if (no_auth_cache)
    return SVN_NO_ERROR;

  /* First, try to save the creds using the provider that produced them. */
  provider = APR_ARRAY_IDX(state->table->providers,
                           state->provider_idx,
                           svn_auth_provider_object_t *);
  if (provider->vtable->save_credentials)
    SVN_ERR(provider->vtable->save_credentials(&save_succeeded,
                                               creds,
                                               provider->provider_baton,
                                               auth_baton->parameters,
                                               state->realmstring,
                                               pool));
  if (save_succeeded)
    return SVN_NO_ERROR;

  /* Otherwise, loop from the top of the list, asking every provider
     to attempt a save.  ### todo: someday optimize so we don't
     necessarily start from the top of the list. */
  for (i = 0; i < state->table->providers->nelts; i++)
    {
      provider = APR_ARRAY_IDX(state->table->providers, i,
                               svn_auth_provider_object_t *);
      if (provider->vtable->save_credentials)
        SVN_ERR(provider->vtable->save_credentials
                (&save_succeeded, creds,
                 provider->provider_baton,
                 auth_baton->parameters,
                 state->realmstring,
                 pool));

      if (save_succeeded)
        break;
    }

  /* ### notice that at the moment, if no provider can save, there's
     no way the caller will know. */

  return SVN_NO_ERROR;
}


svn_error_t *
svn_auth_forget_credentials(svn_auth_baton_t *auth_baton,
                            const char *cred_kind,
                            const char *realmstring,
                            apr_pool_t *scratch_pool)
{
  SVN_ERR_ASSERT((cred_kind && realmstring) || (!cred_kind && !realmstring));

  /* If we have a CRED_KIND and REALMSTRING, we clear out just the
     cached item (if any).  Otherwise, empty the whole hash. */
  if (cred_kind)
    {
      svn_hash_sets(auth_baton->creds_cache,
                    make_cache_key(cred_kind, realmstring, scratch_pool),
                    NULL);
    }
  else
    {
      apr_hash_clear(auth_baton->creds_cache);
    }

  return SVN_NO_ERROR;
}


svn_auth_ssl_server_cert_info_t *
svn_auth_ssl_server_cert_info_dup
  (const svn_auth_ssl_server_cert_info_t *info, apr_pool_t *pool)
{
  svn_auth_ssl_server_cert_info_t *new_info
    = apr_palloc(pool, sizeof(*new_info));

  *new_info = *info;

  new_info->hostname = apr_pstrdup(pool, new_info->hostname);
  new_info->fingerprint = apr_pstrdup(pool, new_info->fingerprint);
  new_info->valid_from = apr_pstrdup(pool, new_info->valid_from);
  new_info->valid_until = apr_pstrdup(pool, new_info->valid_until);
  new_info->issuer_dname = apr_pstrdup(pool, new_info->issuer_dname);
  new_info->ascii_cert = apr_pstrdup(pool, new_info->ascii_cert);

  return new_info;
}

svn_error_t *
svn_auth_get_platform_specific_provider(svn_auth_provider_object_t **provider,
                                        const char *provider_name,
                                        const char *provider_type,
                                        apr_pool_t *pool)
{
  *provider = NULL;

  if (apr_strnatcmp(provider_name, "gnome_keyring") == 0 ||
      apr_strnatcmp(provider_name, "kwallet") == 0)
    {
#if defined(SVN_HAVE_GNOME_KEYRING) || defined(SVN_HAVE_KWALLET)
      apr_dso_handle_t *dso;
      apr_dso_handle_sym_t provider_function_symbol, version_function_symbol;
      const char *library_label, *library_name;
      const char *provider_function_name, *version_function_name;
      library_name = apr_psprintf(pool,
                                  "libsvn_auth_%s-%d.so.%d",
                                  provider_name,
                                  SVN_VER_MAJOR, SVN_SOVERSION);
      library_label = apr_psprintf(pool, "svn_%s", provider_name);
      provider_function_name = apr_psprintf(pool,
                                            "svn_auth_get_%s_%s_provider",
                                            provider_name, provider_type);
      version_function_name = apr_psprintf(pool,
                                           "svn_auth_%s_version",
                                           provider_name);
      SVN_ERR(svn_dso_load(&dso, library_name));
      if (dso)
        {
          if (apr_dso_sym(&version_function_symbol,
                          dso,
                          version_function_name) == 0)
            {
              svn_version_func_t version_function
                = version_function_symbol;
              svn_version_checklist_t check_list[2];

              check_list[0].label = library_label;
              check_list[0].version_query = version_function;
              check_list[1].label = NULL;
              check_list[1].version_query = NULL;
              SVN_ERR(svn_ver_check_list(svn_subr_version(), check_list));
            }
          if (apr_dso_sym(&provider_function_symbol,
                          dso,
                          provider_function_name) == 0)
            {
              if (strcmp(provider_type, "simple") == 0)
                {
                  svn_auth_simple_provider_func_t provider_function
                    = provider_function_symbol;
                  provider_function(provider, pool);
                }
              else if (strcmp(provider_type, "ssl_client_cert_pw") == 0)
                {
                  svn_auth_ssl_client_cert_pw_provider_func_t provider_function
                    = provider_function_symbol;
                  provider_function(provider, pool);
                }
            }
        }
#endif
    }
  else
    {
#if defined(SVN_HAVE_GPG_AGENT)
      if (strcmp(provider_name, "gpg_agent") == 0 &&
          strcmp(provider_type, "simple") == 0)
        {
          svn_auth_get_gpg_agent_simple_provider(provider, pool);
        }
#endif
#ifdef SVN_HAVE_KEYCHAIN_SERVICES
      if (strcmp(provider_name, "keychain") == 0 &&
          strcmp(provider_type, "simple") == 0)
        {
          svn_auth_get_keychain_simple_provider(provider, pool);
        }
      else if (strcmp(provider_name, "keychain") == 0 &&
               strcmp(provider_type, "ssl_client_cert_pw") == 0)
        {
          svn_auth_get_keychain_ssl_client_cert_pw_provider(provider, pool);
        }
#endif

#if defined(WIN32) && !defined(__MINGW32__)
      if (strcmp(provider_name, "windows") == 0 &&
          strcmp(provider_type, "simple") == 0)
        {
          svn_auth_get_windows_simple_provider(provider, pool);
        }
      else if (strcmp(provider_name, "windows") == 0 &&
               strcmp(provider_type, "ssl_client_cert_pw") == 0)
        {
          svn_auth_get_windows_ssl_client_cert_pw_provider(provider, pool);
        }
      else if (strcmp(provider_name, "windows") == 0 &&
               strcmp(provider_type, "ssl_server_trust") == 0)
        {
          svn_auth_get_windows_ssl_server_trust_provider(provider, pool);
        }
#endif
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_auth_get_platform_specific_client_providers(apr_array_header_t **providers,
                                                svn_config_t *config,
                                                apr_pool_t *pool)
{
  svn_auth_provider_object_t *provider;
  const char *password_stores_config_option;
  apr_array_header_t *password_stores;
  int i;

#define SVN__MAYBE_ADD_PROVIDER(list, p) \
  { if (p) APR_ARRAY_PUSH(list, svn_auth_provider_object_t *) = p; }

#define SVN__DEFAULT_AUTH_PROVIDER_LIST \
         "gnome-keyring,kwallet,keychain,gpg-agent,windows-cryptoapi"

  *providers = apr_array_make(pool, 12, sizeof(svn_auth_provider_object_t *));

  /* Fetch the configured list of password stores, and split them into
     an array. */
  svn_config_get(config,
                 &password_stores_config_option,
                 SVN_CONFIG_SECTION_AUTH,
                 SVN_CONFIG_OPTION_PASSWORD_STORES,
                 SVN__DEFAULT_AUTH_PROVIDER_LIST);
  password_stores = svn_cstring_split(password_stores_config_option,
                                      " ,", TRUE, pool);

  for (i = 0; i < password_stores->nelts; i++)
    {
      const char *password_store = APR_ARRAY_IDX(password_stores, i,
                                                 const char *);

      /* GNOME Keyring */
      if (apr_strnatcmp(password_store, "gnome-keyring") == 0)
        {
          SVN_ERR(svn_auth_get_platform_specific_provider(&provider,
                                                          "gnome_keyring",
                                                          "simple",
                                                          pool));
          SVN__MAYBE_ADD_PROVIDER(*providers, provider);

          SVN_ERR(svn_auth_get_platform_specific_provider(&provider,
                                                          "gnome_keyring",
                                                          "ssl_client_cert_pw",
                                                          pool));
          SVN__MAYBE_ADD_PROVIDER(*providers, provider);
        }
      /* GPG-AGENT */
      else if (apr_strnatcmp(password_store, "gpg-agent") == 0)
        {
          SVN_ERR(svn_auth_get_platform_specific_provider(&provider,
                                                          "gpg_agent",
                                                          "simple",
                                                          pool));
          SVN__MAYBE_ADD_PROVIDER(*providers, provider);
        }
      /* KWallet */
      else if (apr_strnatcmp(password_store, "kwallet") == 0)
        {
          SVN_ERR(svn_auth_get_platform_specific_provider(&provider,
                                                          "kwallet",
                                                          "simple",
                                                          pool));
          SVN__MAYBE_ADD_PROVIDER(*providers, provider);

          SVN_ERR(svn_auth_get_platform_specific_provider(&provider,
                                                          "kwallet",
                                                          "ssl_client_cert_pw",
                                                          pool));
          SVN__MAYBE_ADD_PROVIDER(*providers, provider);
        }
      /* Keychain */
      else if (apr_strnatcmp(password_store, "keychain") == 0)
        {
          SVN_ERR(svn_auth_get_platform_specific_provider(&provider,
                                                          "keychain",
                                                          "simple",
                                                          pool));
          SVN__MAYBE_ADD_PROVIDER(*providers, provider);

          SVN_ERR(svn_auth_get_platform_specific_provider(&provider,
                                                          "keychain",
                                                          "ssl_client_cert_pw",
                                                          pool));
          SVN__MAYBE_ADD_PROVIDER(*providers, provider);
        }
      /* Windows */
      else if (apr_strnatcmp(password_store, "windows-cryptoapi") == 0)
        {
          SVN_ERR(svn_auth_get_platform_specific_provider(&provider,
                                                          "windows",
                                                          "simple",
                                                          pool));
          SVN__MAYBE_ADD_PROVIDER(*providers, provider);

          SVN_ERR(svn_auth_get_platform_specific_provider(&provider,
                                                          "windows",
                                                          "ssl_client_cert_pw",
                                                          pool));
          SVN__MAYBE_ADD_PROVIDER(*providers, provider);
        }
    }

  return SVN_NO_ERROR;
}
