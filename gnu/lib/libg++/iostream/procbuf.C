//    This is part of the iostream library, providing input/output for C++.
//    Copyright (C) 1992 Per Bothner.
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Library General Public
//    License as published by the Free Software Foundation; either
//    version 2 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Library General Public License for more details.
//
//    You should have received a copy of the GNU Library General Public
//    License along with this library; if not, write to the Free
//    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#define _POSIX_SOURCE
#include "ioprivate.h"
#include "procbuf.h"
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>


#ifndef FORK
#define FORK vfork
#ifndef __386BSD__
extern "C" _G_pid_t vfork(void);
#else
extern "C" int vfork(void);
#endif
#endif

procbuf::procbuf(const char *command, int mode) : filebuf()
{
    open(command, mode);
}

procbuf *procbuf::open(const char *command, int mode)
{
    int read_or_write;
    if (is_open())
	return NULL;
    int pipe_fds[2];
    int parent_end, child_end;
    if (::pipe(pipe_fds) < 0)
	return NULL;
    if (mode == ios::in) {
	parent_end = pipe_fds[0];
	child_end = pipe_fds[1];
	read_or_write = _S_NO_WRITES;
    }
    else {
	parent_end = pipe_fds[1];
	child_end = pipe_fds[0];
	read_or_write = _S_NO_READS;
    }
    _pid = FORK();
    if (_pid == 0) {
	::close(parent_end);
	int child_std_end = mode == ios::in ? 1 : 0;
	if (child_end != child_std_end) {
	    ::dup2(child_end, child_std_end);
	    ::close(child_end);
	}
	::execl("/bin/sh", "sh", "-c", command, NULL);
	::_exit(127);
    }
    ::close(child_end);
    if (_pid < 0) {
	::close(parent_end);
	return NULL;
    }
    _fb._fileno = parent_end;
    xsetflags(read_or_write, _S_NO_READS|_S_NO_WRITES);
    return this;
}

/* #define USE_SIGMASK */

int procbuf::sys_close()
{
    _G_pid_t wait_pid;
    int status = filebuf::sys_close();
    if (status < 0)
	return status;
    int wstatus;
#if defined(SIG_BLOCK) && defined(SIG_SETMASK)
    sigset_t set, oset;
    sigemptyset (&set);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGQUIT);
    sigaddset (&set, SIGHUP);
    sigprocmask (SIG_BLOCK, &set, &oset);
#else
#ifdef USE_SIGMASK
    int mask = sigblock(sigmask(SIGINT) | sigmask(SIGQUIT) | sigmask(SIGHUP));
#else
    typedef void (*void_func)(int);
    void_func intsave = (void_func)signal(SIGINT, SIG_IGN);
    void_func quitsave = (void_func)signal(SIGQUIT, SIG_IGN);
    void_func hupsave = (void_func)signal(SIGHUP, SIG_IGN);
#endif
#endif
    while ((wait_pid = wait(&wstatus)) != _pid && wait_pid != -1) { }
#if defined(SIG_BLOCK) && defined(SIG_SETMASK)
    sigprocmask (SIG_SETMASK, &oset, (sigset_t *)NULL);
#else
#ifdef USE_SIGMASK
    (void) sigsetmask(mask);
#else
    signal(SIGINT, intsave);
    signal(SIGQUIT, quitsave);
    signal(SIGHUP, hupsave);
#endif
#endif
    if (wait_pid == -1)
	return -1;
    return 0;
}

procbuf::~procbuf()
{
    close();
}
