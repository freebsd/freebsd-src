#include <windows.h>
#include "leash-int.h"

static
LONG
write_registry_setting_ex(
    HKEY hRoot,
    char* setting,
    DWORD type,
    void* buffer,
    size_t size
    )
{
    HKEY hKey = 0;
    LONG rc = 0;

    if (rc = RegCreateKeyEx(hRoot, LEASH_SETTINGS_REGISTRY_KEY_NAME, 0, 0, 0,
                            KEY_ALL_ACCESS, 0, &hKey, 0))
        goto cleanup;

    rc = RegSetValueEx(hKey, setting, 0, type, (LPBYTE)buffer, size);
 cleanup:
    if (hKey)
        RegCloseKey(hKey);
    return rc;
}

LONG
write_registry_setting(
    char* setting,
    DWORD type,
    void* buffer,
    size_t size
    )
{
    return write_registry_setting_ex(HKEY_CURRENT_USER,
                                     setting,
                                     type,
                                     buffer,
                                     size);
}

static
LONG
read_registry_setting_ex(
    HKEY hRoot,
    char* setting,
    void* buffer,
    size_t size
    )
{
    HKEY hKey = 0;
    LONG rc = 0;
    DWORD dwType;
    DWORD dwCount;

    if (rc = RegOpenKeyEx(hRoot,
                          LEASH_SETTINGS_REGISTRY_KEY_NAME,
                          0, KEY_QUERY_VALUE, &hKey))
        goto cleanup;

    memset(buffer, 0, size);
    dwCount = size;
    rc = RegQueryValueEx(hKey, setting, NULL, &dwType, (LPBYTE)buffer,
                         &dwCount);
 cleanup:
    if (hKey)
        RegCloseKey(hKey);
    return rc;
}

LONG
read_registry_setting_user(
    char* setting,
    void* buffer,
    size_t size
    )
{
    return read_registry_setting_ex(HKEY_CURRENT_USER, setting, buffer, size);
}

static
LONG
read_registry_setting_machine(
    char* setting,
    void* buffer,
    size_t size
    )
{
    return read_registry_setting_ex(HKEY_LOCAL_MACHINE, setting, buffer, size);
}

LONG
read_registry_setting(
    char* setting,
    void* buffer,
    size_t size
    )
{
    LONG rc;
    rc = read_registry_setting_user(setting, buffer, size);
    if (!rc) return rc;
    rc = read_registry_setting_machine(setting, buffer, size);
    return rc;
}
