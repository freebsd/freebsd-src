/*-
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
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/vnode.h>

#include <machine/stdarg.h>

#include <compat/linux/linux_util.h>

const char      linux_emul_path[] = "/compat/linux";

/*
 * Search an alternate path before passing pathname arguments on
 * to system calls. Useful for keeping a separate 'emulation tree'.
 *
 * If cflag is set, we check if an attempt can be made to create
 * the named file, i.e. we check if the directory it should
 * be in exists.
 */
int
linux_emul_find(td, sgp, path, pbuf, cflag)
	struct thread	 *td;
	caddr_t		 *sgp;		/* Pointer to stackgap memory */
	char		 *path;
	char		**pbuf;
	int		  cflag;
{
	char *newpath;
	size_t sz;
	int error;

	error = linux_emul_convpath(td, path, (sgp == NULL) ? UIO_SYSSPACE :
	    UIO_USERSPACE, &newpath, cflag);
	if (newpath == NULL)
		return (error);

	if (sgp == NULL) {
		*pbuf = newpath;
		return (error);
	}

	sz = strlen(newpath);
	*pbuf = stackgap_alloc(sgp, sz + 1);
	if (*pbuf != NULL)
		error = copyout(newpath, *pbuf, sz + 1);
	else
		error = ENAMETOOLONG;
	free(newpath, M_TEMP);

	return (error);
}

int
linux_emul_convpath(td, path, pathseg, pbuf, cflag)
	struct thread	 *td;
	char		 *path;
	enum uio_seg	  pathseg;
	char		**pbuf;
	int		  cflag;
{
	struct nameidata	 nd;
	struct nameidata	 ndroot;
	int			 error;
	const char		*prefix;
	char			*ptr, *buf, *cp;
	size_t			 len, sz;

	GIANT_REQUIRED;

	buf = (char *) malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	*pbuf = buf;

	prefix = linux_emul_path;
	for (ptr = buf; (*ptr = *prefix) != '\0'; ptr++, prefix++)
		continue;
	sz = MAXPATHLEN - (ptr - buf);

	if (pathseg == UIO_SYSSPACE)
		error = copystr(path, ptr, sz, &len);
	else
		error = copyinstr(path, ptr, sz, &len);

	if (error) {
		*pbuf = NULL;
		free(buf, M_TEMP);
		return error;
	}

	if (*ptr != '/') {
		error = EINVAL;
		goto keeporig;
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
		error = namei(&nd);
		*cp = '/';
		if (error != 0)
			goto keeporig;
	}
	else {
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, buf, td);

		if ((error = namei(&nd)) != 0)
			goto keeporig;

		/*
		 * We now compare the vnode of the linux_root to the one
		 * vnode asked. If they resolve to be the same, then we
		 * ignore the match so that the real root gets used.
		 * This avoids the problem of traversing "../.." to find the
		 * root directory and never finding it, because "/" resolves
		 * to the emulation root directory. This is expensive :-(
		 */
		NDINIT(&ndroot, LOOKUP, FOLLOW, UIO_SYSSPACE, linux_emul_path,
		       td);

		if ((error = namei(&ndroot)) != 0) {
			/* Cannot happen! */
			NDFREE(&nd, NDF_ONLY_PNBUF);
			vrele(nd.ni_vp);
			goto keeporig;
		}

		if (nd.ni_vp == ndroot.ni_vp) {
			error = ENOENT;
			goto bad;
		}

	}

	NDFREE(&nd, NDF_ONLY_PNBUF);
	vrele(nd.ni_vp);
	if (!cflag) {
		NDFREE(&ndroot, NDF_ONLY_PNBUF);
		vrele(ndroot.ni_vp);
	}
	return error;

bad:
	NDFREE(&ndroot, NDF_ONLY_PNBUF);
	vrele(ndroot.ni_vp);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vrele(nd.ni_vp);
keeporig:
	/* Keep the original path; copy it back to the start of the buffer. */
	bcopy(ptr, buf, len);
	return error;
}

void
linux_msg(const struct thread *td, const char *fmt, ...)
{
	va_list ap;
	struct proc *p;

	p = td->td_proc;
	printf("linux: pid %d (%s): ", (int)p->p_pid, p->p_comm);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}
