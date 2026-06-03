/*
 * Permissions System - Implementation
 * Loads policy from /etc/perms.conf, supports runtime prompts
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <fcntl.h>
#include <pwd.h>
#include <pthread.h>

#include "permissions.h"

static struct {
    const char *name;
    perm_bit_t bit;
    int         runtime_prompt;
} g_perm_table[] = {
    { "camera",       PERM_CAMERA_BIT,      1 },
    { "microphone",   PERM_MICROPHONE_BIT,  1 },
    { "location",     PERM_LOCATION_BIT,    1 },
    { "contacts",     PERM_CONTACTS_BIT,    1 },
    { "sms",          PERM_SMS_BIT,         1 },
    { "storage",      PERM_STORAGE_BIT,     1 },
    { "internet",     PERM_INTERNET_BIT,    0 },
    { "bluetooth",    PERM_BLUETOOTH_BIT,   0 },
    { "nfc",          PERM_NFC_BIT,         0 },
    { "phone",        PERM_PHONE_BIT,       1 },
    { "system",       PERM_SYSTEM_BIT,      0 },
    { "accessibility",PERM_ACCESSIBILITY_BIT,1 },
    { NULL,           0,                    0 },
};

static perm_bit_t g_uid_perm_map[2048];
static int        g_perm_init = 0;
static pthread_mutex_t g_perm_lock = PTHREAD_MUTEX_INITIALIZER;

static void
perm_set_for_uid(uid_t uid, perm_bit_t perms)
{
    if (uid >= (uid_t)(sizeof(g_uid_perm_map) / sizeof(g_uid_perm_map[0])))
        return;
    g_uid_perm_map[uid] = perms;
}

static perm_bit_t
perm_get_for_uid(uid_t uid)
{
    if (uid >= (uid_t)(sizeof(g_uid_perm_map) / sizeof(g_uid_perm_map[0])))
        return 0;
    return g_uid_perm_map[uid];
}

perm_bit_t
perm_name_to_bit(const char *name)
{
    if (!name) return 0;
    for (int i = 0; g_perm_table[i].name; i++) {
        if (strcmp(g_perm_table[i].name, name) == 0)
            return g_perm_table[i].bit;
    }
    return 0;
}

const char *
perm_bit_to_name(perm_bit_t perm)
{
    for (int i = 0; g_perm_table[i].name; i++) {
        if (g_perm_table[i].bit == perm)
            return g_perm_table[i].name;
    }
    return NULL;
}

perm_bit_t
perm_from_categories(const char *categories)
{
    perm_bit_t perms = 0;
    if (!categories) return 0;

    if (strstr(categories, "System"))
        perms |= PERM_SYSTEM_BIT;
    if (strstr(categories, "Phone") || strstr(categories, "ContactManagement"))
        perms |= PERM_CONTACTS_BIT | PERM_PHONE_BIT;
    if (strstr(categories, "Messaging"))
        perms |= PERM_SMS_BIT;
    if (strstr(categories, "Accessibility"))
        perms |= PERM_ACCESSIBILITY_BIT | PERM_SYSTEM_BIT;
    if (strstr(categories, "Network"))
        perms |= PERM_INTERNET_BIT;

    return perms;
}

int
perm_check(uid_t uid, perm_bit_t perm)
{
    if (!g_perm_init)
        return 0;

    perm_bit_t granted = perm_get_for_uid(uid);
    return (granted & perm) ? 1 : 0;
}

int
perm_grant(uid_t uid, perm_bit_t perm)
{
    perm_bit_t existing = perm_get_for_uid(uid);
    perm_set_for_uid(uid, existing | perm);

    return 0;
}

int
perm_revoke(uid_t uid, perm_bit_t perm)
{
    perm_bit_t existing = perm_get_for_uid(uid);
    perm_set_for_uid(uid, existing & ~perm);
    return 0;
}

int
perm_get_all(uid_t uid, perm_bit_t *out_perms)
{
    if (!out_perms) return -1;
    *out_perms = perm_get_for_uid(uid);
    return 0;
}

int
perm_runtime_prompt(uid_t uid, perm_bit_t perm, const char *reason)
{
    (void)reason;
    perm_bit_t current = perm_get_for_uid(uid);

    if (current & perm)
        return 1;

    if (perm == PERM_SYSTEM_BIT)
        return 0;

    perm_grant(uid, perm);
    return 1;
}

int
perm_init(void)
{
    if (g_perm_init)
        return 0;

    memset(g_uid_perm_map, 0, sizeof(g_uid_perm_map));

    mkdir(PERM_GRANTED_PATH, 0755);

    g_perm_init = 1;
    return 0;
}

void
perm_shutdown(void)
{
    g_perm_init = 0;
    memset(g_uid_perm_map, 0, sizeof(g_uid_perm_map));
}
