/* wait.h -- POSIX macros for evaluating exit statuses
   Copyright (C) 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef HAVE_SYS_WAIT_H
#include <sys/types.h>		/* For pid_t. */
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>	/* for rusage */
#endif
#include <sys/wait.h>
#else
#define WIFSTOPPED(w) (((w) & 0xff) == 0x7f)
#define WIFSIGNALED(w) (((w) & 0xff) != 0x7f && ((w) & 0xff) != 0)
#define WIFEXITED(w) (((w) & 0xff) == 0)

#define WSTOPSIG(w) (((w) >> 8) & 0xff)
#define WTERMSIG(w) ((w) & 0x7f)
#define WEXITSTATUS(w) (((w) >> 8) & 0xff)
#endif
