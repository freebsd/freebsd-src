/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

/*
// Integrated SHA-1 crypt using PHK's MD5 code base.
*/

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$Header: /home/ncvs/src/lib/libcrypt/crypt.c,v 1.4 1996/07/12 18:56:01 jkh Exp $";
#endif /* LIBC_SCCS and not lint */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "crypt.h"

char *
crypt_shs(pw, pl, sp, sl, passwd, token)
	register const unsigned char *pw;
        const unsigned int pl;
	register const unsigned char *sp;
        const unsigned int sl;
        char * passwd;
        char * token;
{
        char *p;
	unsigned char	final[_SHS_SIZE];
	int i,j;
	SHS_CTX	ctx,ctx1;
	unsigned long l;

	shsInit(&ctx);

	/* The password first, since that is what is most unknown */
	shsUpdate(&ctx,pw,pl);

	/* Then our magic string */
	shsUpdate(&ctx,(unsigned char *)token, strlen(token));

	/* Then the raw salt */
	shsUpdate(&ctx,sp,sl);

	/* Then just as many characters of the shs(pw,salt,pw) */
	shsInit(&ctx1);
	shsUpdate(&ctx1,pw,pl);
	shsUpdate(&ctx1,sp,sl);
	shsUpdate(&ctx1,pw,pl);
	shsFinal(&ctx1,final);
	for(i = pl; i > 0; i -= _SHS_SIZE)
		shsUpdate(&ctx,final,i>_SHS_SIZE ? _SHS_SIZE : i);

	/* Don't leave anything around in vm they could use. */
	memset(final,0,sizeof final);

	/* Then something really weird... */
	for (j=0,i = pl; i ; i >>= 1)
		if(i&1)
		    shsUpdate(&ctx, final+j, 1);
		else
		    shsUpdate(&ctx, pw+j, 1);

	/* Now make the output string */
	strcpy(passwd, token);
	strncat(passwd, (char *)sp, (int)sl);
	strcat(passwd, "$");

	shsFinal(&ctx,final);

	/*
	 * and now, just to make sure things don't run too fast
	 * On a 60 Mhz Pentium this takes 34 msec, so you would
	 * need 30 seconds to build a 1000 entry dictionary...
	 */
	for(i=0;i<1000;i++) {
		shsInit(&ctx1);
		if(i & 1)
			shsUpdate(&ctx1,pw,pl);
		else
			shsUpdate(&ctx1,final,_SHS_SIZE);

		if(i % 3)
			shsUpdate(&ctx1,sp,sl);

		if(i % 7)
			shsUpdate(&ctx1,pw,pl);

		if(i & 1)
			shsUpdate(&ctx1,final,_SHS_SIZE);
		else
			shsUpdate(&ctx1,pw,pl);
		shsFinal(&ctx1,final);
	}

	p = passwd + strlen(passwd);

	l = (final[ 0]<<16) | (final[ 6]<<8) | final[12]; to64(p,l,4); p += 4;
	l = (final[ 1]<<16) | (final[ 7]<<8) | final[13]; to64(p,l,4); p += 4;
	l = (final[ 2]<<16) | (final[ 8]<<8) | final[14]; to64(p,l,4); p += 4;
	l = (final[ 3]<<16) | (final[ 9]<<8) | final[15]; to64(p,l,4); p += 4;
	l = (final[ 4]<<16) | (final[10]<<8) | final[16]; to64(p,l,4); p += 4;
	l = (final[ 5]<<16) | (final[11]<<8) | final[17]; to64(p,l,4); p += 4;
	l =                   (final[18]<<8) | final[19]; to64(p,l,3); p += 3;

	*p = '\0';

	/* Don't leave anything around in vm they could use. */
	memset(final,0,sizeof final);

	return passwd;
}

