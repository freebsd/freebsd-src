/*
 * Permissions System
 * Manages app permissions: install-time grants + runtime prompts
 */

#ifndef _PERMISSIONS_H_
#define _PERMISSIONS_H_

#include <sys/types.h>
#include <stdlib.h>

#define PERM_PATH_MAX      256
#define PERM_NAME_MAX      32
#define PERM_MAX_PERMS     32
#define PERM_POLICY_PATH   "/etc/perms.conf"
#define PERM_GRANTED_PATH  "/var/lib/perms"

typedef enum {
    PERM_CAMERA_BIT       = (1 << 0),
    PERM_MICROPHONE_BIT   = (1 << 1),
    PERM_LOCATION_BIT     = (1 << 2),
    PERM_CONTACTS_BIT     = (1 << 3),
    PERM_SMS_BIT          = (1 << 4),
    PERM_STORAGE_BIT      = (1 << 5),
    PERM_INTERNET_BIT     = (1 << 6),
    PERM_BLUETOOTH_BIT    = (1 << 7),
    PERM_NFC_BIT          = (1 << 8),
    PERM_PHONE_BIT        = (1 << 9),
    PERM_SYSTEM_BIT       = (1 << 10),
    PERM_ACCESSIBILITY_BIT = (1 << 11),
} perm_bit_t;

typedef struct perm_def {
    const char *name;
    perm_bit_t bit;
    int         runtime_prompt;
    const char *description;
} perm_def_t;

int perm_init(void);
void perm_shutdown(void);
int perm_check(uid_t uid, perm_bit_t perm);
int perm_grant(uid_t uid, perm_bit_t perm);
int perm_revoke(uid_t uid, perm_bit_t perm);
int perm_get_all(uid_t uid, perm_bit_t *out_perms);
perm_bit_t perm_name_to_bit(const char *name);
const char *perm_bit_to_name(perm_bit_t perm);
perm_bit_t perm_from_categories(const char *categories);
int perm_runtime_prompt(uid_t uid, perm_bit_t perm, const char *reason);

#endif /* _PERMISSIONS_H_ */
