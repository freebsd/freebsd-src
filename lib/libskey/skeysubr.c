#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef	__MSDOS__
#include <dos.h>
#endif
#ifdef unix	/* Assume POSIX */
#include <fcntl.h>
#include <termios.h>
#endif
#include <skey.h>
#include "md4.h"

#if (defined(__MSDOS__) || defined(MPU8086) || defined(MPU8080) \
 || defined(vax) || defined (MIPSEL))
#define	LITTLE_ENDIAN	/* Low order bytes are first in memory */
#endif			/* Almost all other machines are big-endian */

/* Crunch a key:
 * concatenate the seed and the password, run through MD4 and
 * collapse to 64 bits. This is defined as the user's starting key.
 */
int
keycrunch(result,seed,passwd)
char *result;	/* 8-byte result */
char *seed;	/* Seed, any length */
char *passwd;	/* Password, any length */
{
	char *buf;
	MDstruct md;
	unsigned int buflen;
#ifndef	LITTLE_ENDIAN
	int i;
	register long tmp;
#endif
	
	buflen = strlen(seed) + strlen(passwd);
	if((buf = malloc(buflen+1)) == NULL)
		return -1;
	strcpy(buf,seed);
	strcat(buf,passwd);

	/* Crunch the key through MD4 */
	sevenbit(buf);
	MDbegin(&md);
	MDupdate(&md,(unsigned char *)buf,8*buflen);

	free(buf);

	/* Fold result from 128 to 64 bits */
	md.buffer[0] ^= md.buffer[2];
	md.buffer[1] ^= md.buffer[3];

#ifdef	LITTLE_ENDIAN
	/* Only works on byte-addressed little-endian machines!! */
	memcpy(result,(char *)md.buffer,8);
#else
	/* Default (but slow) code that will convert to
	 * little-endian byte ordering on any machine
	 */
	for(i=0;i<2;i++){
		tmp = md.buffer[i];
		*result++ = tmp;
		tmp >>= 8;
		*result++ = tmp;
		tmp >>= 8;
		*result++ = tmp;
		tmp >>= 8;
		*result++ = tmp;
	}
#endif

	return 0;
}

/* The one-way function f(). Takes 8 bytes and returns 8 bytes in place */
void
f(x)
char *x;
{
	MDstruct md;
#ifndef	LITTLE_ENDIAN
	register long tmp;
#endif

	MDbegin(&md);
	MDupdate(&md,(unsigned char *)x,64);

	/* Fold 128 to 64 bits */
	md.buffer[0] ^= md.buffer[2];
	md.buffer[1] ^= md.buffer[3];

#ifdef	LITTLE_ENDIAN
	/* Only works on byte-addressed little-endian machines!! */
	memcpy(x,(char *)md.buffer,8);

#else
	/* Default (but slow) code that will convert to
	 * little-endian byte ordering on any machine
	 */
	tmp = md.buffer[0];
	*x++ = tmp;
	tmp >>= 8;
	*x++ = tmp;
	tmp >>= 8;
	*x++ = tmp;
	tmp >>= 8;
	*x++ = tmp;

	tmp = md.buffer[1];
	*x++ = tmp;
	tmp >>= 8;
	*x++ = tmp;
	tmp >>= 8;
	*x++ = tmp;
	tmp >>= 8;
	*x = tmp;
#endif
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
char *
readpass(buf,n)
char *buf;
int n;
{
	struct termios saved_ttymode;
	struct termios noecho_ttymode;

	/* Save normal line editing modes */
	tcgetattr(0, &saved_ttymode);

	/* Turn off echoing */
	tcgetattr(0, &noecho_ttymode);
	noecho_ttymode.c_lflag &= ~ECHO;
	tcsetattr(0, TCSANOW, &noecho_ttymode);
	fgets(buf,n,stdin);
	rip(buf);

	/* Restore previous tty modes */
	tcsetattr(0, TCSANOW, &saved_ttymode);

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

/* removebackspaced over charaters from the string*/
backspace(buf)
char *buf;
{
	char bs = 0x8;
	char *cp = buf;
	char *out = buf;

	while(*cp){
		if( *cp == bs ) {
			if(out == buf){
				cp++;
				continue;
			}
			else {
			  cp++;
			  out--;
			}
		}
		else {
			*out++ = *cp++;
		}

	}
	*out = '\0';
	
}
sevenbit(s)
char *s;
{
	/* make sure there are only 7 bit code in the line*/
	while(*s){
		*s = 0x7f & ( *s);
		s++;
	}
}
