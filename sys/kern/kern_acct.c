/*
 * Copyright (c) UNIX System Laboratories, Inc.  All or some portions
 * of this file are derived from material licensed to the
 * University of California by American Telephone and Telegraph Co.
 * or UNIX System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 */
/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 *
 *	from: @(#)kern_acct.c	7.18 (Berkeley) 5/11/91
 *	$Id: kern_acct.c,v 1.10 1994/05/04 08:26:46 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "namei.h"
#include "resourcevar.h"
#include "proc.h"
#include "ioctl.h"
#include "termios.h"
#include "tty.h"
#include "vnode.h"
#include "mount.h"
#include "kernel.h"
#include "file.h"
#include "acct.h"
#include "syslog.h"

#include "vm/vm.h"
#include "vm/vm_param.h"

/*
 * Values associated with enabling and disabling accounting
 */
int	acctsuspend = 2;	/* stop accounting when < 2% free space left */
int	acctresume = 4;		/* resume when free space risen to > 4% */
struct  timeval chk;            /* frequency to check space for accounting */
struct  vnode *acctp = NULL;	/* file to which to do accounting */
struct  vnode *savacctp = NULL;	/* file to which to do accounting when space */

static void acctwatch(caddr_t, int);

/*
 * Enable or disable process accounting.
 *
 * If a non-null filename is given, that file is used to store accounting
 * records on process exit. If a null filename is given process accounting
 * is suspended. If accounting is enabled, the system checks the amount
 * of freespace on the filesystem at timeval intervals. If the amount of
 * freespace is below acctsuspend percent, accounting is suspended. If
 * accounting has been suspended, and freespace rises above acctresume,
 * accounting is resumed.
 */

/* Mark Tinguely (tinguely@plains.NoDak.edu) 8/10/93 */

struct sysacct_args {
	char	*fname;
};

/* ARGSUSED */
int
sysacct(p, uap, retval)
	struct proc *p;
	struct sysacct_args *uap;
	int *retval;
{

	register struct nameidata *ndp;
	struct nameidata nd;
	struct vattr attr;
	int rv;

	if (p->p_ucred->cr_uid != 0)
		return(EPERM);		/* must be root */

	/*
	 * Step 1. turn off accounting (if on). exit if fname is nil
	 */

	rv = 0;				/* just in case nothing is open */
	if (acctp != NULL) {
		rv = vn_close(acctp, FWRITE, p->p_ucred, p);
		untimeout(acctwatch, (caddr_t) &chk);	/* turn off disk check */
		acctp = NULL;
	}
	else if (savacctp != NULL ) {
		rv = vn_close(savacctp, FWRITE, p->p_ucred, p);
		untimeout(acctwatch, (caddr_t) &chk);	/* turn off disk check */
		savacctp = NULL;
	}
	
	if (uap->fname == NULL)		/* accounting stopping complete */
		return(rv);

	/*
	 * Step 2. open accounting filename for writing.
	 */

	nd.ni_segflg = UIO_USERSPACE;
	nd.ni_dirp = uap->fname;

	/* is it there? */
	if (rv = vn_open(&nd, p, FWRITE, 0))
		return (rv);

	/* Step 2. Check the attributes on accounting file */
	rv = VOP_GETATTR(nd.ni_vp, &attr, p->p_ucred, p);
	if (rv)
		goto acct_fail;

	/* is filesystem writable, do I have permission to write and is
	 * a regular file?
	 */
        if (nd.ni_vp->v_mount->mnt_flag & MNT_RDONLY) {
		rv = EROFS;	/* to be consistant with man page */
		goto acct_fail;
	}

	if ((VOP_ACCESS(nd.ni_vp, VWRITE, p->p_ucred, p)) ||
	    (attr.va_type != VREG)) {
		rv = EACCES;	/* permission denied error */
		goto acct_fail;
	}

	/* Step 3. Save the accounting file vnode, schedule freespace watch. */

	acctp  = nd.ni_vp;
	savacctp = NULL;
	VOP_UNLOCK(acctp);
	acctwatch((caddr_t)&chk, 0); /* look for full system */
	return(0);		/* end successfully */

acct_fail:

	vn_close(nd.ni_vp, FWRITE, p->p_ucred, p);
	return(rv);
}

/*
 * Periodically check the file system to see if accounting
 * should be turned on or off.
 */
static void
acctwatch(arg1, arg2)
	caddr_t arg1;
	int arg2;
{
	struct timeval *resettime = (struct timeval *)arg1;
	struct statfs sb;
	int s;

	if (savacctp) {
		(void)VFS_STATFS(savacctp->v_mount, &sb, (struct proc *)0);
		if (sb.f_bavail > acctresume * sb.f_blocks / 100) {
			acctp = savacctp;
			savacctp = NULL;
			log(LOG_NOTICE, "Accounting resumed\n");
			return;
		}
	}
	if (acctp == NULL)
		return;
	(void)VFS_STATFS(acctp->v_mount, &sb, (struct proc *)0);
	if (sb.f_bavail <= acctsuspend * sb.f_blocks / 100) {
		savacctp = acctp;
		acctp = NULL;
		log(LOG_NOTICE, "Accounting suspended\n");
	}
	s = splhigh(); *resettime = time; splx(s);
	resettime->tv_sec += 15;
	timeout(acctwatch, (caddr_t)resettime, hzto(resettime));
}

/*
 * This routine calculates an accounting record for a process and,
 * if accounting is enabled, writes it to the accounting file.
 */

/* Mark Tinguely (tinguely@plains.NoDak.edu) 8/10/93 */

void
acct(p)
	register struct proc *p;
{

	struct acct acct;
	struct rusage *r;
	int rv;
	long i;
	u_int cnt;
	char *c;
	comp_t int2comp();


	if (acctp == NULL)	/* accounting not turned on */
		return;

	/* Step 1. Get command name (remove path if necessary) */

	strncpy(acct.ac_comm, p->p_comm, sizeof(acct.ac_comm));

	/* Step 2. Get rest of information */

	acct.ac_utime = int2comp((unsigned) p->p_utime.tv_sec * 1000000 + p->p_utime.tv_usec);
	acct.ac_stime = int2comp((unsigned) p->p_stime.tv_sec * 1000000 + p->p_stime.tv_usec);
	acct.ac_btime = p->p_stats->p_start.tv_sec;
			/* elapse time = current - start */
	i = (time.tv_sec - p->p_stats->p_start.tv_sec) * 1000000 +
	    (time.tv_usec - p->p_stats->p_start.tv_usec);
	acct.ac_etime = int2comp((unsigned) i);

	acct.ac_uid = p->p_cred->p_ruid;
	acct.ac_gid = p->p_cred->p_rgid;

	r = &p->p_stats->p_ru;
	if (i = (p->p_utime.tv_sec + p->p_stime.tv_sec) * hz +
	        (p->p_utime.tv_usec + p->p_stime.tv_usec) / tick)
		acct.ac_mem = (r->ru_ixrss + r->ru_idrss + r->ru_isrss) / i;
	else
		acct.ac_mem = 0;
	acct.ac_io = int2comp((unsigned) (r->ru_inblock + r->ru_oublock) * 1000000);

	if ((p->p_flag & SCTTY) && p->p_pgrp->pg_session->s_ttyp)
		acct.ac_tty = p->p_pgrp->pg_session->s_ttyp->t_dev;
	else
		acct.ac_tty = NODEV;
	acct.ac_flag = p->p_acflag;

	/* Step 3. Write record to file */


	rv = vn_rdwr(UIO_WRITE, acctp, (caddr_t) &acct, sizeof (acct), 
	    (off_t)0, UIO_SYSSPACE, IO_APPEND|IO_UNIT, p->p_ucred, (int *) NULL,
	    p);
}

/*  int2comp converts from ticks in a microsecond to ticks in 1/AHZ second
 * 
 * comp_t is a psuedo-floating point number with 13 bits of
 * mantissa and 3 bits of base 8 exponent and has resolution
 * of 1/AHZ seconds.
 *
 * notice I already converted the incoming values into microseconds
 * I need to convert back into AHZ ticks.
 */

/* Mark Tinguely (tinguely@plains.NoDak.edu) 8/10/93 */


#define RES 13
#define EXP 3
#define MAXFRACT 1<<RES

comp_t
int2comp(mantissa)
unsigned int mantissa;
{
	comp_t exp=0;

	mantissa = mantissa * AHZ / 1000000;	/* convert back to AHZ ticks */
	while (mantissa > MAXFRACT) {
		mantissa >>= EXP;	/* base 8 exponent */
		exp++;
	}
	exp <<= RES;		/* move the exponent */
	exp += mantissa;	/* add on the manissa */
	return (exp);
}
