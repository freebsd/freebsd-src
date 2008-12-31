/*-
 * Copyright (C) 2006 John Birrell <jb@freebsd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible 
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * $FreeBSD: src/sys/cddl/dev/dtrace/dtrace_clone.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

static void
dtrace_clone(void *arg, struct ucred *cred, char *name, int namelen, struct cdev **dev)
{
	int u = -1;
	size_t len;

	if (*dev != NULL)
		return;

	len = strlen(name);

	if (len != 6 && len != 13)
		return;

	if (bcmp(name,"dtrace",6) != 0)
		return;

	if (len == 13 && bcmp(name,"dtrace/dtrace",13) != 0)
		return;

	/* Clone the device to the new minor number. */
	if (clone_create(&dtrace_clones, &dtrace_cdevsw, &u, dev, 0) != 0)
		/* Create the /dev/dtrace/dtraceNN entry. */
		*dev = make_dev_cred(&dtrace_cdevsw, unit2minor(u), cred,
		     UID_ROOT, GID_WHEEL, 0600, "dtrace/dtrace%d", u);
	if (*dev != NULL) {
		dev_ref(*dev);
		(*dev)->si_flags |= SI_CHEAPCLONE;
	}
}
