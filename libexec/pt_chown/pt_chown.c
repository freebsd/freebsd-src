/*
 * Copyright (c) 2002 The FreeBSD Project, Inc.
 * All rights reserved.
 *
 * This software includes code contributed to the FreeBSD Project
 * by Ryan Younce of North Carolina State University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the FreeBSD Project nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE FREEBSD PROJECT OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__FBSDID("$FreeBSD$");
#endif /* not lint */

#include <sys/stat.h>

#include <grp.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

/*
 * pt_chown
 * Utility support routine for grantpt(3).
 *
 * According to IEEE Std 1003.1-2001, grantpt(3) changes ownership and
 * permission bits of a slave pseudo-terminal device associated with a
 * master.
 *
 * Since doing this if we are not the owner of the slave (which would
 * rarely happen) cannot be done by conventional methods, grantpt(3)
 * has to rely on this support program, which is setuid root, to change
 * the slave's owner, group, and permission mode attributes.  It's
 * a rather undesirable approach, but Digital Unix and Solaris also seem
 * to rely on this approach to pull this off.
 *
 * This program hangs around long enough to do just these things upon
 * its standard input (which is set up by grantpt(3) to be the master's
 * descriptor, the one passed to it).  The rationale behind this
 * approach not allowing somebody to modify ownership of another active
 * pseudo terminal is:
 *
 * 1)  This program only operates on its standard input.  If STDIN_FILENO
 *     is not open or is not a pseudo-terminal master, no action is
 *     taken and the program exits (ptsname() is called for a non-NULL
 *     return).
 * 2)  Only one active file description for a pseudo-terminal master
 *     can exist at a time (attempting to open an active PTY returns with
 *     EIO - I/O Error).  Thus, if the pseudo-terminal is already in
 *     use by somebody else, it could not have been opened to begin
 *     with, and thus this program would be useless in such situations.
 */
int
main(int argc, char *argv[])
{
	int retcode;
	char *slave;
	gid_t gid;
	struct group *grp;

	retcode = EX_OK;

	if ((slave = ptsname(STDIN_FILENO)) == NULL)
		retcode = EX_USAGE;
	else {
		gid = (grp = getgrnam("tty")) ? grp->gr_gid : -1;
		if (chown(slave, getuid(), gid) == 0 &&
		    chmod(slave, S_IRUSR | S_IWUSR | S_IWGRP) == 0)
			retcode = 0;
		else
			retcode = EX_NOPERM;
	}

	/*
	 * grantpt(3) checks the retcode for being either zero or
	 * nonzero.  Any nonzero return results in errno being set
	 * to EACCES.
	 */
	exit(retcode);
}
