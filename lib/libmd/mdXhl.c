/* mdXhl.c
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "mdX.h"

char *
MDXEnd(MDX_CTX *ctx, char *buf)
{
    int i;
    unsigned char digest[LENGTH];
    static const char hex[]="0123456789abcdef";

    if (!buf)
        buf = malloc(2*LENGTH + 1);
    if (!buf)
	return 0;
    MDXFinal(digest, ctx);
    for (i = 0; i < LENGTH; i++) {
	buf[i+i] = hex[digest[i] >> 4];
	buf[i+i+1] = hex[digest[i] & 0x0f];
    }
    buf[i+i] = '\0';
    return buf;
}

char *
MDXFile(const char *filename, char *buf)
{
    return MDXFileChunk(filename, buf, 0, 0);
}

char *
MDXFileChunk(const char *filename, char *buf, off_t ofs, off_t len)
{
    unsigned char buffer[BUFSIZ];
    MDX_CTX ctx;
    struct stat stbuf;
    int f, i, e;
    off_t n;

    MDXInit(&ctx);
    f = open(filename, O_RDONLY);
    if (f < 0) return 0;
    if (fstat(f, &stbuf) < 0) return 0;
    if (ofs > stbuf.st_size)
	ofs = stbuf.st_size;
    if ((len == 0) || (len > stbuf.st_size - ofs))
	len = stbuf.st_size - ofs;
    if (lseek(f, ofs, SEEK_SET) < 0) return 0;
    n = len;
    while (n > 0) {
	if (n > sizeof(buffer))
	    i = read(f, buffer, sizeof(buffer));
	else
	    i = read(f, buffer, n);
	if (i < 0) break;
	MDXUpdate(&ctx, buffer, i);
	n -= i;
    } 
    e = errno;
    close(f);
    errno = e;
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
