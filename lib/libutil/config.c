/*
 * Copyright (c) 1983,1991 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$Id: config.c,v 1.1 1994/01/14 12:24:39 jkh Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * This file contains four procedures to read config-files with.
 *
 * char * config_open(const char *filename,int contlines)
 *	Will open the named file, and read it into a private malloced area,
 *	and close the file again.  If contlines are non-zero 
 *	continuation-lines will be allowed.  In case of trouble, the name
 *	of the system-call causing the trouble will be returned.  On success
 *	NULL is returned.
 *
 * void   config_close()
 *	This will free the internal malloced area.
 *
 * char * config_next()
 *	This will return a pointer to the next entry in the area.  NULL is
 *	returned at "end of file".  If continuation-lines are used, the '\n'
 *	will be converted to ' '.  The return value is '\0' terminated, and
 *	can be modified, but the contents must be copied somewhere else.
 *
 * char * config_skip(char **p)
 *	This will pick out the next word from the string.  The return-value
 *	points to the word found, and *p is advanced past the word.  NULL is
 *	returned at "end of string".
 *
 * The point about this is, that many programs have an n*100 bytes config-file
 * and some N*1000 bytes of source to read it.  Doing pointer-aerobics on
 * files that small is waste of time, and bashing around with getchar/ungetc
 * isn't much better.  These routines implement a simple algorithm and syntax.
 *
 * 1. Lines starting in '#' are comments.
 * 2. An entry starts with the first '!isspace()' character found.
 * 3. If continuation-lines are enabled, an entry ends before the first
 *    empty line or before the first line not starting in an 'isspace()'
 *    character, whichever comes first.
 * 4. Otherwise, an entry ends at the first '\n'.
 *
 * For config_skip goes that it considers a word as a contiguous string of
 * !isspace() characters.
 *
 * There is an #ifdef'ed main() at the end, which is provided for test and
 * illustration of use.
 *
 * 11jan1994 Poul-Henning Kamp  phk@login.dkuug.dk
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

static char * file_buf;
static char * ptr;
static int clines;

char *
config_open(const char *filename, int contlines)
{
    int fd;
    struct stat st;

    clines = contlines;
    if((fd = open(filename,O_RDONLY)) < 0)
	return "open";
    if(fstat(fd,&st) < 0) {
 	close(fd);
	return "fstat";
    }
    if(file_buf)
	free(file_buf);
    file_buf = malloc(st.st_size+1);
    if(!file_buf) {
	close(fd);
	return "malloc";
    }
    if(st.st_size != read(fd,file_buf,st.st_size)) {
	free(file_buf);
	file_buf = (char*)0;
	close(fd);
	return "read";
    }
    close(fd);
    file_buf[st.st_size] = '\0';
    ptr = file_buf;
    return 0;
}

void
config_close(void)
{
    if(file_buf)
	free(file_buf);
    ptr = file_buf = 0;
}

/*
 * Get next entry.  An entry starts in column 0, and not with a '#',
 * following lines are joined, if they start with white-space.
 */
char *
config_next(void)
{
    char *p,*q;

    /* We might be done already ! */
    if(!ptr || !*ptr)
	return 0;

    /* Skip comments and blank lines */
    while(*ptr) {
	if(*ptr == '#') {
	    ptr = strchr(ptr,'\n');
	    if(!ptr) 
		return 0;
	    ptr++;
	    continue;
	}
	for(q=ptr;*q != '\n' && isspace(*q);q++) ;
	if(*q != '\n')
	    break;
	ptr = q+1;
    }

    if(!*ptr)
	return 0;

    p = ptr;
    while(1) {
	ptr = strchr(ptr,'\n');
	if(!ptr)		/* last line ? */
	    return p;
	if(clines && isspace(ptr[1])) {
	    for(q=ptr+1;*q != '\n' && isspace(*q);q++) ;
	    if(*q != '\n') {
		*ptr++ = ' ';
	        continue;
	    }
	}
	*ptr++ = '\0';
	return p;
    }
}

/*
 * return next word
 */

char *
config_skip(char **p)
{
    char *q,*r;

    if(!*p || !**p)
	return 0;
    for(q = *p;isspace(*q);q++) ;
    if(!*q)
	return 0;
    for(r=q;*r && !isspace(*r);r++) ;
    if(*r) 
	*r++ = '\0';
    *p = r;
    return q;
}
