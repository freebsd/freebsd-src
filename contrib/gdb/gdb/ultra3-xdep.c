/* Host-dependent code for GDB, for NYU Ultra3 running Sym1 OS.
   Copyright (C) 1988, 1989, 1991, 1992 Free Software Foundation, Inc.
   Contributed by David Wood (wood@nyu.edu) at New York University.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define DEBUG
#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "value.h"

#include <sys/types.h>
#include <sys/param.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>  

#include "gdbcore.h"

#include <sys/file.h>
#include "gdb_stat.h"

/* Assorted operating system circumventions */

#ifdef SYM1

/* FIXME: Kludge this for now. It really should be system call. */
int
getpagesize()
{ return(8192); }

/* FIXME: Fake out the fcntl() call, which we don't have.  */
fcntl(fd, cmd, arg)
int fd, cmd, arg;
{

  switch (cmd) {
	case F_GETFL: return(O_RDONLY);	break;
	default:	
		printf_unfiltered("Ultra3's fcntl() failing, cmd = %d.\n",cmd);
		return(-1);
  }
}


/* 
 * 4.2 Signal support, requires linking with libjobs.
 */
static int	_SigMask;
#define sigbit(s)       (1L << ((s)-1))

init_SigMask()
{
	/* Taken from the sym1 kernel in machdep.c:startup() */
	_SigMask = sigbit (SIGTSTP) | sigbit (SIGTTOU) | sigbit (SIGTTIN) |
                        sigbit (SIGCHLD) | sigbit (SIGTINT);
}

sigmask(signo)
	int signo;
{
	return (1 << (signo-1));
}

sigsetmask(sigmask)
unsigned int sigmask;
{
	int i, mask = 1;
	int lastmask = _SigMask;

	for (i=0 ; i<NSIG ; i++) {
		if (sigmask & mask) { 
			if (!(_SigMask & mask)) {
				sighold(i+1);
				_SigMask |= mask;
			}
		} else if (_SigMask & mask) {
			sigrelse(i+1);
			_SigMask &= ~mask;
		}
		mask <<= 1;
	}
	return (lastmask);
}

sigblock(sigmask)
unsigned int sigmask;
{
	int i, mask = 1;
	int lastmask = _SigMask;

	for (i=0 ; i<NSIG ; i++) {
		if ((sigmask & mask) && !(_SigMask & mask)) {
			sighold(i+1);
			_SigMask |= mask;
		}
		mask <<= 1;
	}
	return (lastmask);
}
#endif /* SYM1 */


/* Initialization code for this module.  */

void
_initialize_ultra3 ()
{
#ifdef SYM1
	init_SigMask();
#endif
}
