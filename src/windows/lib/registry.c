/*
 * Copyright (c) 1997 Cygnus Solutions
 *
 * Author:  Michael Graff
 */

#include <stdlib.h>
#include <windows.h>
#include <windowsx.h>

#include "registry.h"

HKEY
registry_open(HKEY hkey, char *base, REGSAM sam)
{
	HKEY    k = INVALID_HANDLE_VALUE;
	DWORD   err;

	/*
	 * if the base path is null, return the already open key in hkey
	 */
	if (base == NULL)
		return hkey;

	err = RegOpenKeyEx(hkey, base, 0, sam, &hkey);
	if (err != ERROR_SUCCESS)
		return INVALID_HANDLE_VALUE;

	return hkey;
}

void
registry_close(HKEY hkey)
{
	CloseHandle(hkey);
}

HKEY
registry_key_create(HKEY hkey, char *sub, REGSAM sam)
{
	HKEY    key;
	DWORD   err;
	DWORD   disp;

	err = RegCreateKeyEx(hkey, sub, 0, 0, REG_OPTION_NON_VOLATILE, sam,
			     NULL, &key, &disp);
	if (err != ERROR_SUCCESS)
		return INVALID_HANDLE_VALUE;

	return key;
}

int
registry_key_delete(HKEY hkey, char *sub)
{
	DWORD err;

	err = RegDeleteKey(hkey, sub);
	if (err != ERROR_SUCCESS)
		return -1;

	return 0;
}

int
registry_string_get(HKEY hkey, char *sub, char **val)
{
	DWORD   err;
	DWORD   type;
	DWORD   datasize;

	err = RegQueryValueEx(hkey, sub, 0, &type, 0, &datasize);
	if (err != ERROR_SUCCESS || type != REG_SZ) {
		*val = NULL;
		return -1;
	}

	*val = malloc(datasize);
	if (*val == NULL)
		return -1;

	err = RegQueryValueEx(hkey, sub, 0, &type, *val, &datasize);
	if (err != ERROR_SUCCESS) {
		free(*val);
		*val = NULL;
		return -1;
	}

	return 0;
}

int
registry_dword_get(HKEY hkey, char *sub, DWORD *val)
{
	DWORD   err;
	DWORD   type;
	DWORD   datasize;

	err = RegQueryValueEx(hkey, sub, 0, &type, 0, &datasize);
	if (err != ERROR_SUCCESS || type != REG_DWORD) {
		*val = 0;
		return -1;
	}

	err = RegQueryValueEx(hkey, sub, 0, &type, (BYTE *)val, &datasize);
	if (err != ERROR_SUCCESS) {
		*val = 0;
		return -1;
	}

	return 0;
}

int
registry_string_set(HKEY hkey, char *sub, char *x)
{
	DWORD   err;

	err = RegSetValueEx(hkey, sub, 0, REG_SZ, (BYTE *)x, (DWORD)strlen(x) + 1);
	if (err != ERROR_SUCCESS)
		return -1;

	return 0;
}

int
registry_dword_set(HKEY hkey, char *sub, DWORD x)
{
	DWORD   err;

	err = RegSetValueEx(hkey, sub, 0, REG_DWORD, (CONST BYTE *)&x, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
		return -1;

	return 0;
}

int
registry_keyval_dword_set(HKEY hkey, char *base, char *sub, DWORD val)
{
	HKEY   k;
	int    err;

	k = registry_open(hkey, base, KEY_WRITE);
	if (k == INVALID_HANDLE_VALUE)
		return -1;

	err = registry_dword_set(k, sub, val);

	registry_close(k);

	return err;
}

int
registry_keyval_dword_get(HKEY hkey, char *base, char *sub, DWORD *val)
{
	HKEY   k;
	int    err;

	k = registry_open(hkey, base, KEY_READ);
	if (k == INVALID_HANDLE_VALUE)
		return -1;

	err = registry_dword_get(k, sub, val);

	registry_close(k);

	return err;
}

int
registry_keyval_string_get(HKEY hkey, char *base, char *sub, char **val)
{
	HKEY   k;
	int    err;

	k = registry_open(hkey, base, KEY_READ);
	if (k == INVALID_HANDLE_VALUE) {
		*val = NULL;
		return -1;
	}

	err = registry_string_get(k, sub, val);

	registry_close(k);

	return err;
}

int
registry_keyval_string_set(HKEY hkey, char *base, char *sub, char *val)
{
	HKEY   k;
	int    err;

	k = registry_open(hkey, base, KEY_WRITE);
	if (k == INVALID_HANDLE_VALUE)
		return -1;

	err = registry_string_set(k, sub, val);

	registry_close(k);

	return err;
}

int
registry_value_delete(HKEY hkey, char *sub)
{
	if (RegDeleteValue(hkey, sub))
		return -1;

	return 0;
}

int
registry_keyval_delete(HKEY hkey, char *base, char *sub)
{
	HKEY   k;
	int    err;

	k = registry_open(hkey, base, KEY_WRITE);
	if (k == INVALID_HANDLE_VALUE)
		return -1;

	err = registry_value_delete(k, sub);

	registry_close(k);

	return err;
}
