/* $FreeBSD: src/lib/libskey/skeysubr.c,v 1.9.6.1 2000/07/20 20:13:42 obrien Exp $ */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>

#include "skey.h"
#include "mdx.h"

/* Crunch a key:
 * concatenate the seed and the password, run through MDX and
 * collapse to 64 bits. This is defined as the user's starting key.
 */
int
keycrunch(result,seed,passwd)
char *result;   /* 8-byte result */
const char *seed;     /* Seed, any length */
const char *passwd;   /* Password, any length */
{
	char *buf;
	MDX_CTX md;
	u_int32_t results[4];
	unsigned int buflen;

	buflen = strlen(seed) + strlen(passwd);
	if((buf = malloc(buflen+1)) == NULL)
		return -1;
	strcpy(buf,seed);
	strcat(buf,passwd);

	/* Crunch the key through MD[45] */
	sevenbit(buf);
	MDXInit(&md);
	MDXUpdate(&md,(unsigned char *)buf,buflen);
	MDXFinal((unsigned char *)results,&md);
	free(buf);

	results[0] ^= results[2];
	results[1] ^= results[3];

	memcpy(result,(char *)results,8);

	return 0;
}

/* The one-way function f(). Takes 8 bytes and returns 8 bytes in place */
void
f(x)
char *x;
{
	MDX_CTX md;
	u_int32_t results[4];

	MDXInit(&md);
	MDXUpdate(&md,(unsigned char *)x,8);
	MDXFinal((unsigned char *)results,&md);
	/* Fold 128 to 64 bits */
	results[0] ^= results[2];
	results[1] ^= results[3];

	memcpy(x,(char *)results,8);
}

/* Strip trailing cr/lf from a line of text */
void
rip(buf)
char *buf;
{
	buf[strcspn(buf, "\r\n")] = 0;
}

static struct termios saved_ttymode;

static void interrupt __P((int));

static void interrupt(sig)
int sig;
{
	tcsetattr(0, TCSANOW, &saved_ttymode);
	err(1, "interrupted by signal %s", sys_siglist[sig]);
}

char *
readpass(buf,n)
char *buf;
int n;
{
	struct termios noecho_ttymode;
	void (*oldsig) __P((int));

	/* Save normal line editing modes */
	tcgetattr(0, &saved_ttymode);
	if ((oldsig = signal(SIGINT, SIG_IGN)) != SIG_IGN)
		signal(SIGINT, interrupt);

	/* Turn off echoing */
	tcgetattr(0, &noecho_ttymode);
	noecho_ttymode.c_lflag &= ~ECHO;
	tcsetattr(0, TCSANOW, &noecho_ttymode);
	fgets(buf,n,stdin);
	rip(buf);

	/* Restore previous tty modes */
	tcsetattr(0, TCSANOW, &saved_ttymode);
	if (oldsig != SIG_IGN)
		signal(SIGINT, oldsig);

	/*
	after the secret key is taken from the keyboard, the line feed is
	written to standard error instead of standard output.  That means that
	anyone using the program from a terminal won't notice, but capturing
	standard output will get the key words without a newline in front of
	them.
	*/
        fprintf(stderr, "\n");
        fflush(stderr);
	sevenbit(buf);

	return buf;
}

void
sevenbit(s)
char *s;
{
	/* make sure there are only 7 bit code in the line*/
	while(*s){
		*s &= 0x7f;
		s++;
	}
}
