/* wait.h -- POSIX macros for evaluating exit statuses
   Copyright (C) 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#ifdef HAVE_SYS_WAIT_H
#include <sys/types.h>		/* For pid_t. */
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>	/* for rusage */
#endif
#include <sys/wait.h>
#endif
#ifndef WIFSTOPPED
#define WIFSTOPPED(w) (((w) & 0xff) == 0x7f)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(w) (((w) & 0xff) != 0x7f && ((w) & 0xff) != 0)
#endif
#ifndef WIFEXITED
#define WIFEXITED(w) (((w) & 0xff) == 0)
#endif
#ifndef WCOREDUMP	/* not POSIX, but common and useful */
#define WCOREDUMP(w) (((w) & 0x80) != 0)
#endif

#ifndef WSTOPSIG
#define WSTOPSIG(w) (((w) >> 8) & 0xff)
#endif
#ifndef WTERMSIG
#define WTERMSIG(w) ((w) & 0x7f)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(w) (((w) >> 8) & 0xff)
#endif
