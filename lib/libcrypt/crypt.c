/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

/*
 * It has since been changed by Brandon Gillespie, the end result is not
 * too clean, but it is clear and modular; there is no need for crypt()
 * to be optimized (and actually a desire for the opposite) so I am not
 * overly concerned.
 */

/*
 * Assumptions made with the format of passwords:
 *
 *   + Any password beginning with a dollar-sign is assumed to be in
 *     the Modular Crypt Format (MCF), namely: $tag$salt$hash.  Any
 *     algorithms added will also use this format.  Other MCF assumptions:
 *     + The algorithm tag (field 1) will be less than five characters
 *       long (yay, arbitrary limits).  Anything longer is ignored.
 *       New algorithm names are not allowed to be fully numeric as
 *       anything fully numeric is mapped from other OS's not following
 *       our standard, and from older versions of this standard (such as
 *       $1$ for MD5 passwords, rather than $MD5$).
 *     + The salt can be up to 16 characters in length (more arbitrary
 *       limits).
 *   + An invalid or unrecognized algorithm tag will default to use the
 *     'best' encryption method--whatever that may be at the time.
 *   + If the MCF is not specified, use the 'best' method, unless DES
 *     is installed--then use DES.
 *   + Any password beginning with an underscore '_' is assumed to be
 *     the Extended DES Format, which has its own salt requirements,
 *     and is not the same as the MCF.
 *   + Salt must be limited to the same ascii64 character set the hash
 *     is encoded in (namely "./0-9A-Za-z").
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define _CRYPT_C_

#include "crypt.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/*
 * commonly used througout all algorithms
 */

static unsigned char ascii64[] =        /* 0 ... 63 => ascii - 64 */
         "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void
to64(s, v, n)
	char *s;
	unsigned long v;
	int n;
{
	while (--n >= 0) {
		*s++ = ascii64[v&0x3f];
		v >>= 6;
	}
}

static char * hash_word(password, salt, output)
	const char *password;
	const char *salt;
        char *output;
{
    unsigned char   spbuf[_CRYPT_MAX_SALT_LEN+1],
                    pwbuf[_CRYPT_OUTPUT_SIZE+1],
                  * ep, * sp, * pw;
    unsigned int    sl, pl,
                    tag = _CRYPT_DEFAULT_VERSION,
                    mcf = FALSE;

    memset(spbuf, 0, _CRYPT_MAX_SALT_LEN+1);
    memset(pwbuf, 0, _CRYPT_MAX_SALT_LEN+1);
    strncpy((char *) spbuf, (unsigned char *) salt, _CRYPT_MAX_SALT_LEN);
    strncpy((char *) pwbuf, (unsigned char *) password, _CRYPT_OUTPUT_SIZE);
    sp = &spbuf[0];
    pw = &pwbuf[0];
    pl = strlen((char *) pw);

    /* figure out what type of crypt is wanted */
    if (sp && sp[0] == '$') {
        mcf = TRUE;
        sp++;
        if (strncasecmp((char *) sp, "MD5$", 4)==0) {
            tag = _MD5_CRYPT;
            sp += 4;
        } else if (strncasecmp((char *) sp, "1$", 2)==0) {
            tag = _MD5_CRYPT_OLD;
            sp += 2;
        } else if (strncasecmp((char *) sp, "SHA1$", 5)==0) {
            tag = _SHS_CRYPT;
            sp += 5;
        } else {
            tag = _CRYPT_DEFAULT_VERSION;
            while (*sp && *sp != '$')
                sp++;
            if (*sp == '$')
                sp++;
        }
    }

    /* Refine the salt. Go to the end, it stops at the first '$' or NULL */
    for (ep=sp; *ep && *ep != '$'; ep++)
        continue;

    /* we have to do this so we dont overflow _PASSWORD_LEN */
    if ((ep - sp) > 16) {
        sl = 16;
        sp[16] = (char) NULL;
    } else {
        sl = ep - sp;
    }

    switch (tag) {
        case _MD5_CRYPT_OLD:
            return crypt_md5(pw, pl, sp, sl, output, "$1$");
        case _MD5_CRYPT:
            return crypt_md5(pw, pl, sp, sl, output, "$MD5$");
#ifdef DES_CRYPT
        case _DES_CRYPT:
            return crypt_des(pw, pl, sp, sl, output, "");
#endif
          /* dropping a DES password through will likely cause problems,
             but at least crypt() will return as it says it will (we cannot
             return an error condition) */
        case _SHS_CRYPT:
        default:
            return crypt_shs(pw, pl, sp, sl, output, "$SHA1$");
    }
}

char *
crypt(password, salt)
	const char *password;
	const char *salt;
{
    static char output[_CRYPT_OUTPUT_SIZE];

    return hash_word(password, salt, output);
}

char *
malloc_crypt(password, salt)
	const char *password;
	const char *salt;
{
    char * output;

    output = (char *) malloc(sizeof(char) * _CRYPT_OUTPUT_SIZE);
    return hash_word(password, salt, output);
}

int
match_crypted(possible, crypted)
    const char * possible,
               * crypted;
{
    char * pc;
    int    match;

    pc = malloc_crypt(possible, crypted);

    match = !strcmp(pc, crypted);

    free(pc);

    return match;
}

#undef _CRYPT_C_
