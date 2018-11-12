/*
 * nls.c :  Helpers for NLS programs.
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

#include <stdlib.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <apr_errno.h>

#include "svn_nls.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_path.h"

#include "svn_private_config.h"

svn_error_t *
svn_nls_init(void)
{
  svn_error_t *err = SVN_NO_ERROR;

#ifdef ENABLE_NLS
  if (getenv("SVN_LOCALE_DIR"))
    {
      bindtextdomain(PACKAGE_NAME, getenv("SVN_LOCALE_DIR"));
    }
  else
    {
#ifdef WIN32
      WCHAR ucs2_path[MAX_PATH];
      char* utf8_path;
      const char* internal_path;
      apr_pool_t* pool;
      apr_size_t inwords, outbytes, outlength;

      apr_pool_create(&pool, 0);
      /* get exe name - our locale info will be in '../share/locale' */
      inwords = GetModuleFileNameW(0, ucs2_path,
                                   sizeof(ucs2_path) / sizeof(ucs2_path[0]));
      if (! inwords)
        {
          /* We must be on a Win9x machine, so attempt to get an ANSI path,
             and convert it to Unicode. */
          CHAR ansi_path[MAX_PATH];

          if (GetModuleFileNameA(0, ansi_path, sizeof(ansi_path)))
            {
              inwords =
                MultiByteToWideChar(CP_ACP, 0, ansi_path, -1, ucs2_path,
                                    sizeof(ucs2_path) / sizeof(ucs2_path[0]));
              if (! inwords)
                {
                err =
                  svn_error_createf(APR_EINVAL, NULL,
                                    _("Can't convert string to UCS-2: '%s'"),
                                    ansi_path);
                }
            }
          else
            {
              err = svn_error_create(APR_EINVAL, NULL,
                                     _("Can't get module file name"));
            }
        }

      if (! err)
        {
          outbytes = outlength = 3 * (inwords + 1);
          utf8_path = apr_palloc(pool, outlength);

          outbytes = WideCharToMultiByte(CP_UTF8, 0, ucs2_path, inwords,
                                         utf8_path, outbytes, NULL, NULL);

          if (outbytes == 0)
            {
              err = svn_error_wrap_apr(apr_get_os_error(),
                                       _("Can't convert module path "
                                         "to UTF-8 from UCS-2: '%s'"),
                                       ucs2_path);
            }
          else
            {
              utf8_path[outlength - outbytes] = '\0';
              internal_path = svn_dirent_internal_style(utf8_path, pool);
              /* get base path name */
              internal_path = svn_dirent_dirname(internal_path, pool);
              internal_path = svn_dirent_join(internal_path,
                                              SVN_LOCALE_RELATIVE_PATH,
                                              pool);
              bindtextdomain(PACKAGE_NAME, internal_path);
            }
        }
      svn_pool_destroy(pool);
    }
#else /* ! WIN32 */
      bindtextdomain(PACKAGE_NAME, SVN_LOCALE_DIR);
    }
#endif /* WIN32 */

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
#endif /* HAVE_BIND_TEXTDOMAIN_CODESET */

#endif /* ENABLE_NLS */

  return err;
}
