/*
 * Copyright (c) 1998 - 2001 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors 
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
RCSID("$Id: simple_exec.c,v 1.10 2001/06/21 03:38:03 assar Exp $");
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
wait_for_process(pid_t pid)
{
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

int
pipe_execv(FILE **stdin_fd, FILE **stdout_fd, FILE **stderr_fd, 
	   const char *file, ...)
{
    int in_fd[2], out_fd[2], err_fd[2];
    pid_t pid;
    va_list ap;
    char **argv;

    if(stdin_fd != NULL)
	pipe(in_fd);
    if(stdout_fd != NULL)
	pipe(out_fd);
    if(stderr_fd != NULL)
	pipe(err_fd);
    pid = fork();
    switch(pid) {
    case 0:
	va_start(ap, file);
	argv = vstrcollect(&ap);
	va_end(ap);
	if(argv == NULL)
	    exit(-1);

	/* close pipes we're not interested in */
	if(stdin_fd != NULL)
	    close(in_fd[1]);
	if(stdout_fd != NULL)
	    close(out_fd[0]);
	if(stderr_fd != NULL)
	    close(err_fd[0]);

	/* pipe everything caller doesn't care about to /dev/null */
	if(stdin_fd == NULL)
	    in_fd[0] = open(_PATH_DEVNULL, O_RDONLY);
	if(stdout_fd == NULL)
	    out_fd[1] = open(_PATH_DEVNULL, O_WRONLY);
	if(stderr_fd == NULL)
	    err_fd[1] = open(_PATH_DEVNULL, O_WRONLY);

	/* move to proper descriptors */
	if(in_fd[0] != STDIN_FILENO) {
	    dup2(in_fd[0], STDIN_FILENO);
	    close(in_fd[0]);
	}
	if(out_fd[1] != STDOUT_FILENO) {
	    dup2(out_fd[1], STDOUT_FILENO);
	    close(out_fd[1]);
	}
	if(err_fd[1] != STDERR_FILENO) {
	    dup2(err_fd[1], STDERR_FILENO);
	    close(err_fd[1]);
	}

	execv(file, argv);
	exit((errno == ENOENT) ? EX_NOTFOUND : EX_NOEXEC);
    case -1:
	if(stdin_fd != NULL) {
	    close(in_fd[0]);
	    close(in_fd[1]);
	}
	if(stdout_fd != NULL) {
	    close(out_fd[0]);
	    close(out_fd[1]);
	}
	if(stderr_fd != NULL) {
	    close(err_fd[0]);
	    close(err_fd[1]);
	}
	return -2;
    default:
	if(stdin_fd != NULL) {
	    close(in_fd[0]);
	    *stdin_fd = fdopen(in_fd[1], "w");
	}
	if(stdout_fd != NULL) {
	    close(out_fd[1]);
	    *stdout_fd = fdopen(out_fd[0], "r");
	}
	if(stderr_fd != NULL) {
	    close(err_fd[1]);
	    *stderr_fd = fdopen(err_fd[0], "r");
	}
    }
    return pid;
}

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
	return wait_for_process(pid);
    }
}

/* gee, I'd like a execvpe */
int
simple_execve(const char *file, char *const args[], char *const envp[])
{
    pid_t pid = fork();
    switch(pid){
    case -1:
	return -2;
    case 0:
	execve(file, args, envp);
	exit((errno == ENOENT) ? EX_NOTFOUND : EX_NOEXEC);
    default: 
	return wait_for_process(pid);
    }
}

int
simple_execlp(const char *file, ...)
{
    va_list ap;
    char **argv;
    int ret;

    va_start(ap, file);
    argv = vstrcollect(&ap);
    va_end(ap);
    if(argv == NULL)
	return -1;
    ret = simple_execvp(file, argv);
    free(argv);
    return ret;
}

int
simple_execle(const char *file, ... /* ,char *const envp[] */)
{
    va_list ap;
    char **argv;
    char *const* envp;
    int ret;

    va_start(ap, file);
    argv = vstrcollect(&ap);
    envp = va_arg(ap, char **);
    va_end(ap);
    if(argv == NULL)
	return -1;
    ret = simple_execve(file, argv, envp);
    free(argv);
    return ret;
}

int
simple_execl(const char *file, ...) 
{
    va_list ap;
    char **argv;
    int ret;

    va_start(ap, file);
    argv = vstrcollect(&ap);
    va_end(ap);
    if(argv == NULL)
	return -1;
    ret = simple_execve(file, argv, environ);
    free(argv);
    return ret;
}
