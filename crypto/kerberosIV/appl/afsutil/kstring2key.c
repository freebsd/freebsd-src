/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $FreeBSD$ */

#include "config.h"

RCSID("$Id: kstring2key.c,v 1.16 1999/12/02 16:58:28 joda Exp $");

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <err.h>

#include <roken.h>

#define OPENSSL_DES_LIBDES_COMPATIBILITY
#include <openssl/des.h>
#include <krb.h>

#define VERIFY 0

static void
usage(void)
{
    fprintf(stderr,
	    "Usage: %s [-c AFS cellname] [ -5 krb5salt ] [ password ]\n",
	    __progname);
    fprintf(stderr,
	    "       krb5salt is realmname APPEND principal APPEND instance\n");
    exit(1);
}

static
void
krb5_string_to_key(char *str,
		   char *salt,
		   des_cblock *key)
{
    char *foo;

    asprintf(&foo, "%s%s", str, salt);
    if (foo == NULL)
	errx (1, "malloc: out of memory");
    des_string_to_key(foo, key);
    free (foo);
}


int
main(int argc, char **argv)
{
    des_cblock key;
    char buf[1024];
    char *cellname = 0, *salt = 0;

    set_progname (argv[0]);

    if (argc >= 3 && argv[1][0] == '-' && argv[1][1] == 'c')
	{
	    cellname = argv[2];
	    argv += 2;
	    argc -= 2;
	}
    else if (argc >= 3 && argv[1][0] == '-' && argv[1][1] == '5')
	{
	    salt = argv[2];
	    argv += 2;
	    argc -= 2;
	}
    if (argc >= 2 && argv[1][0] == '-')
	usage();

    switch (argc) {
    case 1:
	if (des_read_pw_string(buf, sizeof(buf)-1, "password: ", VERIFY))
	    errx (1, "Error reading password.");
	break;
    case 2:
	strlcpy(buf, argv[1], sizeof(buf));
	break;
    default:
	usage();
	break;
    }

    if (cellname != 0)
	afs_string_to_key(buf, cellname, &key);
    else if (salt != 0)
        krb5_string_to_key(buf, salt, &key);
    else
	des_string_to_key(buf, &key);

    {
	int j;
	unsigned char *tkey = (unsigned char *) &key;
	printf("ascii = ");
	for(j = 0; j < 8; j++)
	    if(tkey[j] != '\\' && isalpha(tkey[j]) != 0)
		printf("%c", tkey[j]);
	    else
		printf("\\%03o",(unsigned char)tkey[j]);
	printf("\n");
	printf("hex   = ");
	for(j = 0; j < 8; j++)
	    printf("%02x",(unsigned char)tkey[j]);
	printf("\n");
    }
    exit(0);
}
