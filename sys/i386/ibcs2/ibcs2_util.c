/*
 * Copyright (c) 1994 Christos Zoulas
 * Copyright (c) 1995 Frank van der Linden
 * Copyright (c) 1995 Scott Bartram
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: svr4_util.c,v 1.5 1995/01/22 23:44:50 christos Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/vnode.h>

#include <i386/ibcs2/ibcs2_util.h>


const char      ibcs2_emul_path[] = "/compat/ibcs2";

/*
 * Search an alternate path before passing pathname arguments on
 * to system calls. Useful for keeping a separate 'emulation tree'.
 *
 * If cflag is set, we check if an attempt can be made to create
 * the named file, i.e. we check if the directory it should
 * be in exists.
 */
int
ibcs2_emul_find(td, sgp, prefix, path, pbuf, cflag)
	struct thread	 *td;
	caddr_t		 *sgp;		/* Pointer to stackgap memory */
	const char	 *prefix;
	char		 *path;
	char		**pbuf;
	int		  cflag;
{
	struct nameidata	 nd;
	struct nameidata	 ndroot;
	struct vattr		 vat;
	struct vattr		 vatroot;
	int			 error;
	char			*ptr, *buf, *cp;
	size_t			 sz, len;

	buf = (char *) malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	*pbuf = path;

	for (ptr = buf; (*ptr = *prefix) != '\0'; ptr++, prefix++)
		continue;

	sz = MAXPATHLEN - (ptr - buf);

	/* 
	 * If sgp is not given then the path is already in kernel space
	 */
	if (sgp == NULL)
		error = copystr(path, ptr, sz, &len);
	else
		error = copyinstr(path, ptr, sz, &len);

	if (error) {
		free(buf, M_TEMP);
		return error;
	}

	if (*ptr != '/') {
		free(buf, M_TEMP);
		return EINVAL;
	}

	/*
	 * We know that there is a / somewhere in this pathname.
	 * Search backwards for it, to find the file's parent dir
	 * to see if it exists in the alternate tree. If it does,
	 * and we want to create a file (cflag is set). We don't
	 * need to worry about the root comparison in this case.
	 */

	if (cflag) {
		for (cp = &ptr[len] - 1; *cp != '/'; cp--);
		*cp = '\0';

		NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, buf, td);

		if ((error = namei(&nd)) != 0) {
			free(buf, M_TEMP);
			return error;
		}

		*cp = '/';
	}
	else {
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, buf, td);

		if ((error = namei(&nd)) != 0) {
			free(buf, M_TEMP);
			return error;
		}

		/*
		 * We now compare the vnode of the ibcs2_root to the one
		 * vnode asked. If they resolve to be the same, then we
		 * ignore the match so that the real root gets used.
		 * This avoids the problem of traversing "../.." to find the
		 * root directory and never finding it, because "/" resolves
		 * to the emulation root directory. This is expensive :-(
		 */
		NDINIT(&ndroot, LOOKUP, FOLLOW, UIO_SYSSPACE, ibcs2_emul_path,
		       td);

		if ((error = namei(&ndroot)) != 0) {
			/* Cannot happen! */
			free(buf, M_TEMP);
			NDFREE(&nd, NDF_ONLY_PNBUF);
			vrele(nd.ni_vp);
			return error;
		}

		if ((error = VOP_GETATTR(nd.ni_vp, &vat, td->td_ucred, td)) != 0) {
			goto done;
		}

		if ((error = VOP_GETATTR(ndroot.ni_vp, &vatroot, td->td_ucred, td))
		    != 0) {
			goto done;
		}

		if (vat.va_fsid == vatroot.va_fsid &&
		    vat.va_fileid == vatroot.va_fileid) {
			error = ENOENT;
			goto done;
		}

	}
	if (sgp == NULL)
		*pbuf = buf;
	else {
		sz = &ptr[len] - buf;
		if ((*pbuf = stackgap_alloc(sgp, sz + 1)) != NULL)
			error = copyout(buf, *pbuf, sz);
		else
			error = ENAMETOOLONG;
		free(buf, M_TEMP);
	}


done:
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vrele(nd.ni_vp);
	if (!cflag) {
		NDFREE(&ndroot, NDF_ONLY_PNBUF);
		vrele(ndroot.ni_vp);
	}
	return error;
}
