/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>

#define LOAD_TINYBUF	2048

/*
 * Attempt to load the file at (path) into an allocated
 * area on the heap, return a pointer to it or NULL
 * on failure.
 *
 * Because in many cases it is impossible to determine the
 * true size of a file without reading it, we do just that.
 */
char *
filedup(const char *path, int flags)
{
    char	*buf;
    int		fd;
    size_t	size, result;
    
    if ((fd = open(path, F_READ | flags)) == -1)
	return(NULL);
    
    printf("%s open, flags 0x%x\n", path, files[fd].f_flags);
    buf = alloc(LOAD_TINYBUF);

    /* Read the first buffer-full */
    size = read(fd, buf, LOAD_TINYBUF);
    if (size < 1) {
	free(buf, LOAD_TINYBUF);
	close(fd);
	return(NULL);
    }
    /* If it all fitted, then just return the buffer straight out */
    if (size < LOAD_TINYBUF) {
	close(fd);
	buf[size] = 0;
	return(buf);
    }

    printf("tinybuf loaded, size %d\n", size);
    getchar();
    
    
    /* Read everything until we know how big it is */
    for (;;) {
	result = read(fd, buf, LOAD_TINYBUF);
	if (size == -1) {
	    free(buf, LOAD_TINYBUF);
	    close(fd);
	    return(NULL);
	}
	if (result == 0)
	    break;
	size += result;
    }
    
    /* discard the old buffer, close the file */
    free(buf, LOAD_TINYBUF);
    close(fd);

    /* reopen the file, realloc the buffer */
    if ((fd = open(path, F_READ | flags)) == -1)
	return(NULL);
    buf = alloc(size);
    result = read(fd, buf, size);
    close(fd);
    if (result != size) {
	free(buf, size);
	return(NULL);
    }
    return(buf);
}

