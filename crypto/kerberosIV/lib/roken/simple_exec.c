/*
 * Copyright (c) 1998, 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. All advertising materials mentioning features or use of this software 
 *    must display the following acknowledgement: 
 *      This product includes software developed by Kungliga Tekniska 
 *      Högskolan and its contributors. 
 *
 * 4. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: simple_exec.c,v 1.4 1999/03/20 02:43:16 assar Exp $");
#endif

#include <stdarg.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>

#include <roken.h>

#define EX_NOEXEC	126
#define EX_NOTFOUND	127

/* return values:
   -1   on `unspecified' system errors
   -2   on fork failures
   -3   on waitpid errors
   0-   is return value from subprocess
   126  if the program couldn't be executed
   127  if the program couldn't be found
   128- is 128 + signal that killed subprocess
   */

int
simple_execvp(const char *file, char *const args[])
{
    pid_t pid = fork();
    switch(pid){
    case -1:
	return -2;
    case 0:
	execvp(file, args);
	exit((errno == ENOENT) ? EX_NOTFOUND : EX_NOEXEC);
    default: 
	while(1) {
	    int status;

	    while(waitpid(pid, &status, 0) < 0)
		if (errno != EINTR)
		    return -3;
	    if(WIFSTOPPED(status))
		continue;
	    if(WIFEXITED(status))
		return WEXITSTATUS(status);
	    if(WIFSIGNALED(status))
		return WTERMSIG(status) + 128;
	}
    }
}

int
simple_execlp(const char *file, ...)
{
    va_list ap;
    char **argv = NULL;
    int argc, i;

    argc = i = 0;
    va_start(ap, file);
    do {
	if(i == argc) {
	    char **tmp = realloc(argv, (argc + 5) * sizeof(*argv));
	    if(tmp == NULL) {
		errno = ENOMEM;
		return -1;
	    }
	    argv = tmp;
	    argc += 5;
	}
	argv[i++] = va_arg(ap, char*);
    } while(argv[i - 1] != NULL);
    va_end(ap);
    i = simple_execvp(file, argv);
    free(argv);
    return i;
}
