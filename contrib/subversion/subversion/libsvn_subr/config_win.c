/*
 * config_win.c :  parsing configuration data from the registry
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



#include "svn_private_config.h"

#ifdef WIN32
/* We must include windows.h ourselves or apr.h includes it for us with
   many ignore options set. Including Winsock is required to resolve IPv6
   compilation errors. APR_HAVE_IPV6 is only defined after including
   apr.h, so we can't detect this case here. */

#define WIN32_LEAN_AND_MEAN
/* winsock2.h includes windows.h */
#include <winsock2.h>
#include <Ws2tcpip.h>

#include <shlobj.h>

#include <apr_file_info.h>

#include "svn_error.h"
#include "svn_path.h"
#include "svn_pools.h"
#include "svn_utf.h"

svn_error_t *
svn_config__win_config_path(const char **folder, int system_path,
                            apr_pool_t *pool)
{
  /* ### Adding CSIDL_FLAG_CREATE here, because those folders really
     must exist.  I'm not too sure about the SHGFP_TYPE_CURRENT
     semancics, though; maybe we should use ..._DEFAULT instead? */
  const int csidl = ((system_path ? CSIDL_COMMON_APPDATA : CSIDL_APPDATA)
                     | CSIDL_FLAG_CREATE);

  WCHAR folder_ucs2[MAX_PATH];
  int inwords, outbytes, outlength;
  char *folder_utf8;

  if (S_OK != SHGetFolderPathW(NULL, csidl, NULL, SHGFP_TYPE_CURRENT,
                               folder_ucs2))
    return svn_error_create(SVN_ERR_BAD_FILENAME, NULL,
                          (system_path
                           ? "Can't determine the system config path"
                           : "Can't determine the user's config path"));

  /* ### When mapping from UCS-2 to UTF-8, we need at most 3 bytes
         per wide char, plus extra space for the nul terminator. */
  inwords = lstrlenW(folder_ucs2);
  outbytes = outlength = 3 * (inwords + 1);

  folder_utf8 = apr_palloc(pool, outlength);

  outbytes = WideCharToMultiByte(CP_UTF8, 0, folder_ucs2, inwords,
                                 folder_utf8, outbytes, NULL, NULL);

  if (outbytes == 0)
    return svn_error_wrap_apr(apr_get_os_error(),
                              "Can't convert config path to UTF-8");

  /* Note that WideCharToMultiByte does _not_ terminate the
     outgoing buffer. */
  folder_utf8[outbytes] = '\0';
  *folder = folder_utf8;

  return SVN_NO_ERROR;
}


#include "config_impl.h"

/* ### These constants are insanely large, but (a) we want to avoid
   reallocating strings if possible, and (b) the realloc logic might
   not actually work -- you never know with Win32 ... */
#define SVN_REG_DEFAULT_NAME_SIZE  2048
#define SVN_REG_DEFAULT_VALUE_SIZE 8192

static svn_error_t *
parse_section(svn_config_t *cfg, HKEY hkey, const char *section,
              svn_stringbuf_t *option, svn_stringbuf_t *value)
{
  DWORD option_len, type, index;
  LONG err;

  /* Start with a reasonable size for the buffers. */
  svn_stringbuf_ensure(option, SVN_REG_DEFAULT_NAME_SIZE);
  svn_stringbuf_ensure(value, SVN_REG_DEFAULT_VALUE_SIZE);
  for (index = 0; ; ++index)
    {
      option_len = (DWORD)option->blocksize;
      err = RegEnumValue(hkey, index, option->data, &option_len,
                         NULL, &type, NULL, NULL);
      if (err == ERROR_NO_MORE_ITEMS)
          break;
      if (err == ERROR_INSUFFICIENT_BUFFER)
        {
          svn_stringbuf_ensure(option, option_len);
          err = RegEnumValue(hkey, index, option->data, &option_len,
                             NULL, &type, NULL, NULL);
        }
      if (err != ERROR_SUCCESS)
        return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL,
                                "Can't enumerate registry values");

      /* Ignore option names that start with '#', see
         http://subversion.tigris.org/issues/show_bug.cgi?id=671 */
      if (type == REG_SZ && option->data[0] != '#')
        {
          DWORD value_len = (DWORD)value->blocksize;
          err = RegQueryValueEx(hkey, option->data, NULL, NULL,
                                (LPBYTE)value->data, &value_len);
          if (err == ERROR_MORE_DATA)
            {
              svn_stringbuf_ensure(value, value_len);
              err = RegQueryValueEx(hkey, option->data, NULL, NULL,
                                    (LPBYTE)value->data, &value_len);
            }
          if (err != ERROR_SUCCESS)
            return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL,
                                    "Can't read registry value data");

          svn_config_set(cfg, section, option->data, value->data);
        }
    }

  return SVN_NO_ERROR;
}



/*** Exported interface. ***/

svn_error_t *
svn_config__parse_registry(svn_config_t *cfg, const char *file,
                           svn_boolean_t must_exist, apr_pool_t *pool)
{
  apr_pool_t *subpool;
  svn_stringbuf_t *section, *option, *value;
  svn_error_t *svn_err = SVN_NO_ERROR;
  HKEY base_hkey, hkey;
  DWORD index;
  LONG err;

  if (0 == strncmp(file, SVN_REGISTRY_HKLM, SVN_REGISTRY_HKLM_LEN))
    {
      base_hkey = HKEY_LOCAL_MACHINE;
      file += SVN_REGISTRY_HKLM_LEN;
    }
  else if (0 == strncmp(file, SVN_REGISTRY_HKCU, SVN_REGISTRY_HKCU_LEN))
    {
      base_hkey = HKEY_CURRENT_USER;
      file += SVN_REGISTRY_HKCU_LEN;
    }
  else
    {
      return svn_error_createf(SVN_ERR_BAD_FILENAME, NULL,
                               "Unrecognised registry path '%s'",
                               svn_dirent_local_style(file, pool));
    }

  err = RegOpenKeyEx(base_hkey, file, 0,
                     KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE,
                     &hkey);
  if (err != ERROR_SUCCESS)
    {
      const int is_enoent = APR_STATUS_IS_ENOENT(APR_FROM_OS_ERROR(err));
      if (!is_enoent)
        return svn_error_createf(SVN_ERR_BAD_FILENAME, NULL,
                                 "Can't open registry key '%s'",
                                 svn_dirent_local_style(file, pool));
      else if (must_exist && is_enoent)
        return svn_error_createf(SVN_ERR_BAD_FILENAME, NULL,
                                 "Can't find registry key '%s'",
                                 svn_dirent_local_style(file, pool));
      else
        return SVN_NO_ERROR;
    }


  subpool = svn_pool_create(pool);
  section = svn_stringbuf_create_empty(subpool);
  option = svn_stringbuf_create_empty(subpool);
  value = svn_stringbuf_create_empty(subpool);

  /* The top-level values belong to the [DEFAULT] section */
  svn_err = parse_section(cfg, hkey, SVN_CONFIG__DEFAULT_SECTION,
                          option, value);
  if (svn_err)
    goto cleanup;

  /* Now enumerate the rest of the keys. */
  svn_stringbuf_ensure(section, SVN_REG_DEFAULT_NAME_SIZE);
  for (index = 0; ; ++index)
    {
      DWORD section_len = (DWORD)section->blocksize;
      HKEY sub_hkey;

      err = RegEnumKeyEx(hkey, index, section->data, &section_len,
                         NULL, NULL, NULL, NULL);
      if (err == ERROR_NO_MORE_ITEMS)
          break;
      if (err == ERROR_MORE_DATA)
        {
          svn_stringbuf_ensure(section, section_len);
          err = RegEnumKeyEx(hkey, index, section->data, &section_len,
                             NULL, NULL, NULL, NULL);
        }
      if (err != ERROR_SUCCESS)
        {
          svn_err =  svn_error_create(SVN_ERR_MALFORMED_FILE, NULL,
                                      "Can't enumerate registry keys");
          goto cleanup;
        }

      err = RegOpenKeyEx(hkey, section->data, 0,
                         KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE,
                         &sub_hkey);
      if (err != ERROR_SUCCESS)
        {
          svn_err =  svn_error_create(SVN_ERR_MALFORMED_FILE, NULL,
                                      "Can't open existing subkey");
          goto cleanup;
        }

      svn_err = parse_section(cfg, sub_hkey, section->data, option, value);
      RegCloseKey(sub_hkey);
      if (svn_err)
        goto cleanup;
    }

 cleanup:
  RegCloseKey(hkey);
  svn_pool_destroy(subpool);
  return svn_err;
}

#endif /* WIN32 */
