/*
 * Copyright (c) 1997 Cygnus Solutions
 *
 * Author:  Michael Graff
 */

#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cns.h"
#include "cns_reg.h"

#include "../lib/registry.h"

cns_reg_t cns_res;  /* yes, a global.  Sue me. */

/*
 * function to load all the data we will want from the registry.  If the
 * registry data cannot be found this function will initialize a default
 * environment.
 */
void
cns_load_registry(void)
{
  char    tmp[1024];
  DWORD   tdw;
  char   *ts;
  HKEY    key;
  int     i;

  /*
   * Set up reasonable default values.  These will all be overwritten if
   * the registry is successfully opened.
   */
  cns_res.name[0] = '\0';
  cns_res.realm[0] = '\0';
  cns_res.x = 0;
  cns_res.y = 0;
  cns_res.cx = 0;
  cns_res.cy = 0;

  cns_res.alert = 0;
  cns_res.beep = 0;
  cns_res.lifetime = DEFAULT_TKT_LIFE * 5;
  cns_res.forwardable = 1;
  cns_res.noaddresses = 0;

  for (i = 1 ; i < FILE_MENU_MAX_LOGINS ; i++)
    cns_res.logins[i][0] = '\0';

  /*
   * by default, allow the user to override the config file location and NOT the
   * cred cache name.
   */
  cns_res.conf_override = 1;
  cns_res.cc_override = 0;

  {
	char *s;
	s = krb5_cc_default_name(k5_context);

	strcpy(cns_res.def_ccname, s);
  }

  cns_res.def_confname[0] = '\0';

  /*
   * If the system has these keys in the registry, do not allow the user to
   * override the config file and ccache locations.
   */
  key = registry_open(HKEY_LOCAL_MACHINE, KERBNET_BASE, KEY_READ);
  if (key != INVALID_HANDLE_VALUE) {
	if (registry_string_get(key, KERBNET_HOME, &ts) == 0) {
		cns_res.conf_override = 0;
		cns_res.def_confname[sizeof(cns_res.def_confname) - 1];
		strncpy(cns_res.def_confname, ts,
		        sizeof(cns_res.def_confname) - 1);
		strncat(cns_res.def_confname, "\\etc\\krb5.conf",
			sizeof(cns_res.def_confname) - 1 -
			strlen(cns_res.def_confname));
		free(ts);
	  }

	  if (registry_string_get(key, "ccname", &ts) == 0) {
		cns_res.cc_override = 0;
		strcpy(cns_res.def_ccname, ts);
		free(ts);
	  }
  }

  /*
   * Try to open the registry.  If we succeed, read the last used values from there.  If we
   * do not get the registry open simply return.
   */
  key = registry_open(HKEY_CURRENT_USER, KERBNET_CNS_BASE, KEY_ALL_ACCESS);

  if (key == INVALID_HANDLE_VALUE)
	return;

  if (registry_dword_get(key, "x", &tdw) == 0)
	  cns_res.x = tdw;

  if (registry_dword_get(key, "y", &tdw) == 0)
	  cns_res.y = tdw;

  if (registry_dword_get(key, "cx", &tdw) == 0)
	  cns_res.cx = tdw;

  if (registry_dword_get(key, "cy", &tdw) == 0)
	  cns_res.cy = tdw;

  if (registry_dword_get(key, "lifetime", &tdw) == 0)
	  cns_res.lifetime = tdw;

  if (registry_dword_get(key, "forwardable", &tdw) == 0)
	  cns_res.forwardable = tdw;

  if (registry_dword_get(key, "noaddresses", &tdw) == 0)
   	  cns_res.noaddresses = tdw;

  if (registry_dword_get(key, "alert", &tdw) == 0)
	  cns_res.alert = tdw;

  if (registry_dword_get(key, "beep", &tdw) == 0)
	  cns_res.beep = tdw;

  if (registry_string_get(key, "name", &ts) == 0) {
	strcpy(cns_res.name, ts);
	free(ts);
  }

  if (registry_string_get(key, "realm", &ts) == 0) {
	strcpy(cns_res.realm, ts);
	free(ts);
  }

  if (cns_res.conf_override && (registry_string_get(key, "confname", &ts) == 0)) {
	strcpy(cns_res.confname, ts);
	free(ts);
  } else
	  strcpy(cns_res.confname, cns_res.def_confname);

  if (cns_res.cc_override && (registry_string_get(key, "ccname", &ts) == 0)) {
	strcpy(cns_res.ccname, ts);
	free(ts);
  } else
	  strcpy(cns_res.ccname, cns_res.def_ccname);

  for (i = 0 ; i < FILE_MENU_MAX_LOGINS ; i++) {
    sprintf(tmp, "login_%02d", i);
    if (registry_string_get(key, tmp, &ts) == 0) {
      strcpy(cns_res.logins[i], ts);
      free(ts);
    }
  }

  registry_close(key);
}

/*
 * save all the registry data, creating the keys if needed.
 */
void
cns_save_registry(void)
{
  char    tmp[1024];
  HKEY    key;
  int     i;

  /*
   * First, create the heirachy...  This is gross, but functional
   */
  key = registry_key_create(HKEY_CURRENT_USER, CYGNUS_SOLUTIONS, KEY_WRITE);
  if (key == INVALID_HANDLE_VALUE)
	  return;

  key = registry_key_create(HKEY_CURRENT_USER, KERBNET_SANS_VERSION, KEY_WRITE);
  if (key == INVALID_HANDLE_VALUE)
	  return;
  registry_close(key);

  key = registry_key_create(HKEY_CURRENT_USER, KERBNET_BASE, KEY_WRITE);
  if (key == INVALID_HANDLE_VALUE)
	  return;
  registry_close(key);

  key = registry_key_create(HKEY_CURRENT_USER, KERBNET_CNS_BASE, KEY_WRITE);
  if (key == INVALID_HANDLE_VALUE)
	return;

  registry_dword_set(key, "x", cns_res.x);
  registry_dword_set(key, "y", cns_res.y);
  registry_dword_set(key, "cx", cns_res.cx);
  registry_dword_set(key, "cy", cns_res.cy);

  registry_dword_set(key, "alert", cns_res.alert);
  registry_dword_set(key, "beep", cns_res.beep);
  registry_dword_set(key, "lifetime", cns_res.lifetime);
  registry_dword_set(key, "forwardable", cns_res.forwardable);
  registry_dword_set(key, "noaddresses", cns_res.noaddresses);

  registry_string_set(key, "name", cns_res.name);
  registry_string_set(key, "realm", cns_res.realm);

  if (cns_res.conf_override)
  {
      if (strcmp(cns_res.confname, cns_res.def_confname))
	  registry_string_set(key, "confname", cns_res.confname);
      else
	  registry_value_delete(key, "confname");
  }

  if (cns_res.cc_override)
  {
      if (strcmp(cns_res.ccname, cns_res.def_ccname))
	  registry_string_set(key, "ccname", cns_res.ccname);
      else
	  registry_value_delete(key, "ccname");
  }

  for (i = 0 ; i < FILE_MENU_MAX_LOGINS ; i++)
    if (cns_res.logins[i][0] != '\0') {
      sprintf(tmp, "login_%02d", i);
      registry_string_set(key, tmp, cns_res.logins[i]);
    }

  registry_close(key);
}
