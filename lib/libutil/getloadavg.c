/*-
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	$Id: getloadavg.c,v 1.2 1994/02/23 09:56:44 rgrimes Exp $
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getloadavg.c	6.2 (Berkeley) 6/29/90";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
#include <nlist.h>

static struct nlist nl[] = {
	{ "_averunnable" },
#define	X_AVERUNNABLE	0
	{ "_fscale" },
#define	X_FSCALE	1
	{ "" },
};

/*
 *  getloadavg() -- Get system load averages.
 *
 *  Put `nelem' samples into `loadavg' array.
 *  Return number of samples retrieved, or -1 on error.
 */
getloadavg(loadavg, nelem)
	double loadavg[];
	int nelem;
{
	static int need_nlist = 1;
	fixpt_t	averunnable[3];
	int fscale, kmemfd, i;
	int alreadyopen;

	if ((alreadyopen = kvm_openfiles(NULL, NULL, NULL)) == -1)
		return (-1);
	/* 
	 * cache nlist 
	 */
	if (need_nlist) {
		if (kvm_nlist(nl) != 0)
			goto bad;
		need_nlist = 0;
	}
	if (kvm_read((off_t)nl[X_AVERUNNABLE].n_value, (char *)averunnable, 
	    sizeof(averunnable)) != sizeof(averunnable))
		goto bad;
	if (kvm_read( (off_t)nl[X_FSCALE].n_value, (char *)&fscale, 
	    sizeof(fscale)) != sizeof(fscale))
		goto bad;
	nelem = MIN(nelem, sizeof(averunnable) / sizeof(averunnable[0]));
	for (i = 0; i < nelem; i++)
		loadavg[i] = (double) averunnable[i] / fscale;
	if (!alreadyopen)
		kvm_close();
	return (nelem);

bad:
	if (!alreadyopen)
		kvm_close();
	return (-1);
}
