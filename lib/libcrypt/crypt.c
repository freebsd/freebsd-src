/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: crypt.c,v 1.3 1995/05/30 05:42:22 rgrimes Exp $
 *
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$Header: /pub/FreeBSD/FreeBSD-CVS/src/lib/libcrypt/crypt.c,v 1.3 1995/05/30 05:42:22 rgrimes Exp $";
#endif /* LIBC_SCCS and not lint */

#include <unistd.h>
#include <stdio.h>
#include <md5.h>

static unsigned char itoa64[] =		/* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void
to64(s, v, n)
	char *s;
	unsigned long v;
	int n;
{
	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}

/*
 * UNIX password
 *
 * Use MD5 for what it is best at...
 */

char *
crypt(pw, salt)
	register const char *pw;
	register const char *salt;
{
	static char	*magic = "$1$";	/*
						 * This string is magic for
						 * this algorithm.  Having
						 * it this way, we can get
						 * get better later on
						 */
	static char     passwd[120], *p;
	static const char *sp,*ep;
	unsigned char	final[16];
	int sl,pl,i,j;
	MD5_CTX	ctx,ctx1;
	unsigned long l;

	/* Refine the Salt first */
	sp = salt;

	/* If it starts with the magic string, then skip that */
	if(!strncmp(sp,magic,strlen(magic)))
		sp += strlen(magic);

	/* It stops at the first '$', max 8 chars */
	for(ep=sp;*ep && *ep != '$' && ep < (sp+8);ep++)
		continue;

	/* get the length of the true salt */
	sl = ep - sp;

	MD5Init(&ctx);

	/* The password first, since that is what is most unknown */
	MD5Update(&ctx,pw,strlen(pw));

	/* Then our magic string */
	MD5Update(&ctx,magic,strlen(magic));

	/* Then the raw salt */
	MD5Update(&ctx,sp,sl);

	/* Then just as many characters of the MD5(pw,salt,pw) */
	MD5Init(&ctx1);
	MD5Update(&ctx1,pw,strlen(pw));
	MD5Update(&ctx1,sp,sl);
	MD5Update(&ctx1,pw,strlen(pw));
	MD5Final(final,&ctx1);
	for(pl = strlen(pw); pl > 0; pl -= 16)
		MD5Update(&ctx,final,pl>16 ? 16 : pl);

	/* Don't leave anything around in vm they could use. */
	memset(final,0,sizeof final);

	/* Then something really weird... */
	for (j=0,i = strlen(pw); i ; i >>= 1)
		if(i&1)
		    MD5Update(&ctx, final+j, 1);
		else
		    MD5Update(&ctx, pw+j, 1);

	/* Now make the output string */
	strcpy(passwd,magic);
	strncat(passwd,sp,sl);
	strcat(passwd,"$");

	MD5Final(final,&ctx);

	/*
	 * and now, just to make sure things don't run too fast
	 * On a 60 Mhz Pentium this takes 34 msec, so you would
	 * need 30 seconds to build a 1000 entry dictionary...
	 */
	for(i=0;i<1000;i++) {
		MD5Init(&ctx1);
		if(i & 1)
			MD5Update(&ctx1,pw,strlen(pw));
		else
			MD5Update(&ctx1,final,16);

		if(i % 3)
			MD5Update(&ctx1,sp,sl);

		if(i % 7)
			MD5Update(&ctx1,pw,strlen(pw));

		if(i & 1)
			MD5Update(&ctx1,final,16);
		else
			MD5Update(&ctx1,pw,strlen(pw));
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

