#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef	__MSDOS__
#include <dos.h>
#endif
#ifdef unix
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#endif

#include "skey.h"
#include "mdx.h"

/* Crunch a key:
 * concatenate the seed and the password, run through MD4 and
 * collapse to 64 bits. This is defined as the user's starting key.
 */
int
keycrunch(result,seed,passwd)
char *result;   /* 8-byte result */
char *seed;     /* Seed, any length */
char *passwd;   /* Password, any length */
{
	char *buf;
	MDX_CTX md;
	u_long results[4];
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
	u_long results[4];

	MDXInit(&md);
	MDXUpdate(&md,(unsigned char *)x,8);
	MDXFinal((unsigned char *)results,&md);
	/* Fold 128 to 64 bits */
	results[0] ^= results[2];
	results[1] ^= results[3];

	/* Only works on byte-addressed little-endian machines!! */
	memcpy(x,(char *)results,8);
}

/* Strip trailing cr/lf from a line of text */
void
rip(buf)
char *buf;
{
	char *cp;

	if((cp = strchr(buf,'\r')) != NULL)
		*cp = '\0';

	if((cp = strchr(buf,'\n')) != NULL)
		*cp = '\0';
}
/************************/
#ifdef	__MSDOS__
char *
readpass(buf,n)
char *buf;
int n;
{
	int i;
	char *cp;

	for(cp=buf,i = 0; i < n ; i++)
		if ((*cp++ = bdos(7,0,0)) == '\r')
			break;
	*cp = '\0';
	printf("\n");
	rip(buf);
	return buf;
}
#else
static struct termios saved_ttymode;

static void interrupt()
{
	tcsetattr(0, TCSANOW, &saved_ttymode);
	exit(1);
}

char *
readpass(buf,n)
char *buf;
int n;
{
	struct termios noecho_ttymode;
	void (*oldsig)();

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

#endif

sevenbit(s)
char *s;
{
	/* make sure there are only 7 bit code in the line*/
	while(*s){
		*s = 0x7f & ( *s);
		s++;
	}
}
