/* This defines the Andrew string_to_key function.  It accepts a password
 * string as input and converts its via a one-way encryption algorithm to a DES
 * encryption key.  It is compatible with the original Andrew authentication
 * service password database.
 */

#include "krb_locl.h"

RCSID("$Id: str2key.c,v 1.10 1997/03/23 03:53:19 joda Exp $");

static void
mklower(char *s)
{
    for (; *s; s++)
        if ('A' <= *s && *s <= 'Z')
            *s = *s - 'A' + 'a';
}

/*
 * Short passwords, i.e 8 characters or less.
 */
static void
afs_cmu_StringToKey (char *str, char *cell, des_cblock *key)
{
    char  password[8+1];	/* crypt is limited to 8 chars anyway */
    int   i;
    int   passlen;

    memset (key, 0, sizeof(key));
    memset(password, 0, sizeof(password));

    strncpy (password, cell, 8);
    passlen = strlen (str);
    if (passlen > 8) passlen = 8;

    for (i=0; i<passlen; i++)
        password[i] = str[i] ^ cell[i];	/* make sure cell is zero padded */

    for (i=0; i<8; i++)
        if (password[i] == '\0') password[i] = 'X';

    /* crypt only considers the first 8 characters of password but for some
       reason returns eleven characters of result (plus the two salt chars). */
    strncpy((char *)key, (char *)crypt(password, "#~") + 2, sizeof(des_cblock));

    /* parity is inserted into the LSB so leftshift each byte up one bit.  This
       allows ascii characters with a zero MSB to retain as much significance
       as possible. */
    {   char *keybytes = (char *)key;
        unsigned int temp;

        for (i = 0; i < 8; i++) {
            temp = (unsigned int) keybytes[i];
            keybytes[i] = (unsigned char) (temp << 1);
        }
    }
    des_fixup_key_parity (key);
}

/*
 * Long passwords, i.e 9 characters or more.
 */
static void
afs_transarc_StringToKey (char *str, char *cell, des_cblock *key)
{
    des_key_schedule schedule;
    des_cblock temp_key;
    des_cblock ivec;
    char password[512];
    int  passlen;

    strncpy (password, str, sizeof(password));
    if ((passlen = strlen (password)) < sizeof(password)-1)
        strncat (password, cell, sizeof(password)-passlen);
    if ((passlen = strlen(password)) > sizeof(password)) passlen = sizeof(password);

    memcpy(&ivec, "kerberos", 8);
    memcpy(&temp_key, "kerberos", 8);
    des_fixup_key_parity (&temp_key);
    des_key_sched (&temp_key, schedule);
    des_cbc_cksum ((des_cblock *)password, &ivec, passlen, schedule, &ivec);

    memcpy(&temp_key, &ivec, 8);
    des_fixup_key_parity (&temp_key);
    des_key_sched (&temp_key, schedule);
    des_cbc_cksum ((des_cblock *)password, key, passlen, schedule, &ivec);

    des_fixup_key_parity (key);
}

void
afs_string_to_key(char *str, char *cell, des_cblock *key)
{
    char realm[REALM_SZ+1];
    strncpy(realm, cell, REALM_SZ);
    realm[REALM_SZ] = 0;
    mklower(realm);

    if (strlen(str) > 8)
        afs_transarc_StringToKey (str, realm, key);
    else
        afs_cmu_StringToKey (str, realm, key);
}
