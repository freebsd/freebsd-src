/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
 *
 * Quite extensively rewritten by Poul-Henning Kamp of the FreeBSD
 * project, to make these variables more userfriendly.
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
 *
 *	@(#)kern_sysctl.c	8.4 (Berkeley) 4/14/94
 * $Id: kern_mib.c,v 1.4.2.1 1997/08/17 15:17:26 joerg Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/unistd.h>

#include <machine/cpu.h>

SYSCTL_NODE(, 0,	  sysctl, CTLFLAG_RW, 0,
	"Sysctl internal magic");
SYSCTL_NODE(, CTL_KERN,	  kern,   CTLFLAG_RW, 0,
	"High kernel, proc, limits &c");
SYSCTL_NODE(, CTL_VM,	  vm,     CTLFLAG_RW, 0,
	"Virtual memory");
SYSCTL_NODE(, CTL_VFS,	  vfs,     CTLFLAG_RW, 0,
	"File system");
SYSCTL_NODE(, CTL_NET,	  net,    CTLFLAG_RW, 0,
	"Network, (see socket.h)");
SYSCTL_NODE(, CTL_DEBUG,  debug,  CTLFLAG_RW, 0,
	"Debugging");
SYSCTL_NODE(, CTL_HW,	  hw,     CTLFLAG_RW, 0,
	"hardware");
SYSCTL_NODE(, CTL_MACHDEP, machdep, CTLFLAG_RW, 0,
	"machine dependent");
SYSCTL_NODE(, CTL_USER,	  user,   CTLFLAG_RW, 0,
	"user-level");

SYSCTL_STRING(_kern, KERN_OSRELEASE, osrelease, CTLFLAG_RD, osrelease, 0, "");

SYSCTL_INT(_kern, KERN_OSREV, osrevision, CTLFLAG_RD, 0, BSD, "");

SYSCTL_STRING(_kern, KERN_VERSION, version, CTLFLAG_RD, version, 0, "");

SYSCTL_STRING(_kern, KERN_OSTYPE, ostype, CTLFLAG_RD, ostype, 0, "");

extern int osreldate;
SYSCTL_INT(_kern, KERN_OSRELDATE, osreldate, CTLFLAG_RD, &osreldate, 0, "");

SYSCTL_INT(_kern, KERN_MAXPROC, maxproc, CTLFLAG_RW, &maxproc, 0, "");

SYSCTL_INT(_kern, KERN_MAXPROCPERUID, maxprocperuid,
	CTLFLAG_RW, &maxprocperuid, 0, "");

SYSCTL_INT(_kern, KERN_ARGMAX, argmax, CTLFLAG_RD, 0, ARG_MAX, "");

SYSCTL_INT(_kern, KERN_POSIX1, posix1version, CTLFLAG_RD, 0, _POSIX_VERSION, "");

SYSCTL_INT(_kern, KERN_NGROUPS, ngroups, CTLFLAG_RD, 0, NGROUPS_MAX, "");

SYSCTL_INT(_kern, KERN_JOB_CONTROL, job_control, CTLFLAG_RD, 0, 1, "");

#ifdef _POSIX_SAVED_IDS
SYSCTL_INT(_kern, KERN_SAVED_IDS, saved_ids, CTLFLAG_RD, 0, 1, "");
#else
SYSCTL_INT(_kern, KERN_SAVED_IDS, saved_ids, CTLFLAG_RD, 0, 0, "");
#endif

char kernelname[MAXPATHLEN] = "/kernel";	/* XXX bloat */

SYSCTL_STRING(_kern, KERN_BOOTFILE, bootfile,
	CTLFLAG_RW, kernelname, sizeof kernelname, "");

SYSCTL_INT(_hw, HW_NCPU, ncpu, CTLFLAG_RD, 0, 1, "");

SYSCTL_INT(_hw, HW_BYTEORDER, byteorder, CTLFLAG_RD, 0, BYTE_ORDER, "");

SYSCTL_INT(_hw, HW_PAGESIZE, pagesize, CTLFLAG_RD, 0, PAGE_SIZE, "");

static char	machine_arch[] = MACHINE_ARCH;
SYSCTL_STRING(_hw, HW_MACHINE_ARCH, machine_arch, CTLFLAG_RD,
			  machine_arch, 0, "");

char hostname[MAXHOSTNAMELEN];

SYSCTL_STRING(_kern, KERN_HOSTNAME, hostname, CTLFLAG_RW,
	hostname, sizeof(hostname), "");

int securelevel = -1;

static int
sysctl_kern_securelvl SYSCTL_HANDLER_ARGS
{
		int error, level;

		level = securelevel;
		error = sysctl_handle_int(oidp, &level, 0, req);
		if (error || !req->newptr)
			return (error);
		if (level < securelevel)
			return (EPERM);
		securelevel = level;
		return (error);
}

SYSCTL_PROC(_kern, KERN_SECURELVL, securelevel, CTLTYPE_INT|CTLFLAG_RW,
	0, 0, sysctl_kern_securelvl, "I", "");

char domainname[MAXHOSTNAMELEN];
SYSCTL_STRING(_kern, KERN_NISDOMAINNAME, domainname, CTLFLAG_RW,
	&domainname, sizeof(domainname), "");

long hostid;
/* Some trouble here, if sizeof (int) != sizeof (long) */
SYSCTL_INT(_kern, KERN_HOSTID, hostid, CTLFLAG_RW, &hostid, 0, "");

/*
 * This is really cheating.  These actually live in the libc, something
 * which I'm not quite sure is a good idea anyway, but in order for 
 * getnext and friends to actually work, we define dummies here.
 */
SYSCTL_STRING(_user, USER_CS_PATH, cs_path, CTLFLAG_RD, "", 0, "");
SYSCTL_INT(_user, USER_BC_BASE_MAX, bc_base_max, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_BC_DIM_MAX, bc_dim_max, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_BC_SCALE_MAX, bc_scale_max, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_BC_STRING_MAX, bc_string_max, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_COLL_WEIGHTS_MAX, coll_weights_max, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_EXPR_NEST_MAX, expr_nest_max, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_LINE_MAX, line_max, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_RE_DUP_MAX, re_dup_max, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_POSIX2_VERSION, posix2_version, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_POSIX2_C_BIND, posix2_c_bind, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_POSIX2_C_DEV, posix2_c_dev, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_POSIX2_CHAR_TERM, posix2_char_term, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_POSIX2_FORT_DEV, posix2_fort_dev, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_POSIX2_FORT_RUN, posix2_fort_run, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_POSIX2_LOCALEDEF, posix2_localedef, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_POSIX2_SW_DEV, posix2_sw_dev, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_POSIX2_UPE, posix2_upe, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_STREAM_MAX, stream_max, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_TZNAME_MAX, tzname_max, CTLFLAG_RD, 0, 0, "");
