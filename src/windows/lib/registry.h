/*
 * Copyright (c) 1997 Cygnus Solutions
 *
 * Author:  Michael Graff
 */

#ifndef LIB_WINDOWS_REGISTRY_H
#define LIB_WINDOWS_REGISTRY_H

#include <windows.h>
#include <windowsx.h>

HKEY registry_open(HKEY, char *, REGSAM);
void registry_close(HKEY);
HKEY registry_key_create(HKEY, char *, REGSAM);
int registry_key_delete(HKEY, char *);
int registry_string_get(HKEY, char *, char **);
int registry_dword_get(HKEY, char *, DWORD *);
int registry_string_set(HKEY, char *, char *);
int registry_dword_set(HKEY, char *, DWORD);
int registry_keyval_dword_set(HKEY, char *, char *, DWORD);
int registry_keyval_dword_get(HKEY, char *, char *, DWORD *);
int registry_keyval_string_get(HKEY, char *, char *, char **);
int registry_keyval_string_set(HKEY, char *, char *, char *);
int registry_value_delete(HKEY, char *);
int registry_keyval_delete(HKEY, char *, char *);

#define CYGNUS_SOLUTIONS     "SOFTWARE\\Cygnus Solutions"

#define KERBNET_SANS_VERSION CYGNUS_SOLUTIONS "\\Kerbnet"
#define KERBNET_BASE         KERBNET_SANS_VERSION "\\1"

#define KERBNET_TELNET_BASE  KERBNET_BASE "\\telnet"
#define KERBNET_TELNET_HOST  KERBNET_TELNET_BASE "\\hosts"

#define KERBNET_CNS_BASE     KERBNET_BASE "\\cns"

#define KERBNET_HOME         "KERBNET_HOME"

#endif /* LIB_WINDOWS_REGISTRY_H */
