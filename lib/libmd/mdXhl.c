/* mdXhl.c
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: mdXhl.c,v 1.5 1995/05/30 05:45:17 rgrimes Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "mdX.h"
#include <sys/file.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

char *
MDXEnd(MDX_CTX *ctx, char *buf)
{
    int i;
    char *p = malloc(33);
    unsigned char digest[16];
    static const char hex[]="0123456789abcdef";

    if (!p)
        p = malloc(33);
    if (!p)
	return 0;
    MDXFinal(digest,ctx);
    for (i=0;i<16;i++) {
	p[i+i] = hex[digest[i] >> 4];
	p[i+i+1] = hex[digest[i] & 0x0f];
    }
    p[i+i] = '\0';
    return p;
}

char *
MDXFile (char *filename, char *buf)
{
    unsigned char buffer[BUFSIZ];
    MDX_CTX ctx;
    int f,i,j;

    MDXInit(&ctx);
    f = open(filename,O_RDONLY);
    if (f < 0) return 0;
    while ((i = read(f,buffer,sizeof buffer)) > 0) {
	MDXUpdate(&ctx,buffer,i);
    }
    j = errno;
    close(f);
    errno = j;
    if (i < 0) return 0;
    return MDXEnd(&ctx, buf);
}

char *
MDXData (const unsigned char *data, unsigned int len, char *buf)
{
    MDX_CTX ctx;

    MDXInit(&ctx);
    MDXUpdate(&ctx,data,len);
    return MDXEnd(&ctx, buf);
}
