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
// Modularized by Brandon Gillespie
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
crypt_md5(pw, pl, sp, sl, passwd, token)
	register const unsigned char *pw;
        const unsigned int pl;
	register const unsigned char *sp;
        const unsigned int sl;
        char * passwd;
        char * token;
{
	char *p;
	unsigned char final[_MD5_SIZE];
	int i,j;
	MD5_CTX	ctx,ctx1;
	unsigned long l;

	MD5Init(&ctx);

	/* The password first, since that is what is most unknown */
	MD5Update(&ctx,pw,pl);

	/* Then our magic string */
	MD5Update(&ctx,(unsigned char *)token,strlen(token));

	/* Then the raw salt */
	MD5Update(&ctx,sp,sl);

	/* Then just as many characters of the MD5(pw,salt,pw) */
	MD5Init(&ctx1);
	MD5Update(&ctx1,pw,pl);
	MD5Update(&ctx1,sp,sl);
	MD5Update(&ctx1,pw,pl);
	MD5Final(final,&ctx1);
	for(i = pl; i > 0; i -= 16)
		MD5Update(&ctx,final,i>16 ? 16 : i);

	/* Don't leave anything around in vm they could use. */
	memset(final,0,sizeof final);

	/* Then something really weird... */
	for (j=0,i = pl; i ; i >>= 1)
		if(i&1)
		    MD5Update(&ctx, final+j, 1);
		else
		    MD5Update(&ctx, pw+j, 1);

	/* Now make the output string */
	strcpy(passwd, token);
	strncat(passwd, (char *)sp, (int)sl);
	strcat(passwd, "$");

	MD5Final(final,&ctx);

	/*
	 * and now, just to make sure things don't run too fast
	 * On a 60 Mhz Pentium this takes 34 msec, so you would
	 * need 30 seconds to build a 1000 entry dictionary...
	 */
	for(i=0;i<1000;i++) {
		MD5Init(&ctx1);
		if(i & 1)
			MD5Update(&ctx1,pw,pl);
		else
			MD5Update(&ctx1,final,_MD5_SIZE);

		if(i % 3)
			MD5Update(&ctx1,sp,sl);

		if(i % 7)
			MD5Update(&ctx1,pw,pl);

		if(i & 1)
			MD5Update(&ctx1,final,_MD5_SIZE);
		else
			MD5Update(&ctx1,pw,pl);
		MD5Final(final,&ctx1);
	}

	p = passwd + strlen(passwd);

	l = (final[ 0]<<16) | (final[ 6]<<8) | final[12]; to64(p,l,4); p += 4;
	l = (final[ 1]<<16) | (final[ 7]<<8) | final[13]; to64(p,l,4); p += 4;
	l = (final[ 2]<<16) | (final[ 8]<<8) | final[14]; to64(p,l,4); p += 4;
	l = (final[ 3]<<16) | (final[ 9]<<8) | final[15]; to64(p,l,4); p += 4;
	l = (final[ 4]<<16) | (final[10]<<8) | final[ 5]; to64(p,l,4); p += 4;
	l =                    final[11]                ; to64(p,l,2); p += 2;
	*p = '\0';

	/* Don't leave anything around in vm they could use. */
	memset(final,0,sizeof final);

	return passwd;
}

