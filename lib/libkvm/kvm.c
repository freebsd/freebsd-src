/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm.c	8.2 (Berkeley) 2/13/94";
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>

#define	_WANT_VNET

#include <sys/user.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/linker.h>
#include <sys/pcpu.h>

#include <net/vnet.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <machine/vmparam.h>

#include <ctype.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "kvm_private.h"

/* from src/lib/libc/gen/nlist.c */
int __fdnlist(int, struct nlist *);

char *
kvm_geterr(kd)
	kvm_t *kd;
{
	return (kd->errbuf);
}

#include <stdarg.h>

/*
 * Report an error using printf style arguments.  "program" is kd->program
 * on hard errors, and 0 on soft errors, so that under sun error emulation,
 * only hard errors are printed out (otherwise, programs like gdb will
 * generate tons of error messages when trying to access bogus pointers).
 */
void
_kvm_err(kvm_t *kd, const char *program, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (program != NULL) {
		(void)fprintf(stderr, "%s: ", program);
		(void)vfprintf(stderr, fmt, ap);
		(void)fputc('\n', stderr);
	} else
		(void)vsnprintf(kd->errbuf,
		    sizeof(kd->errbuf), (char *)fmt, ap);

	va_end(ap);
}

void
_kvm_syserr(kvm_t *kd, const char *program, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	if (program != NULL) {
		(void)fprintf(stderr, "%s: ", program);
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": %s\n", strerror(errno));
	} else {
		char *cp = kd->errbuf;

		(void)vsnprintf(cp, sizeof(kd->errbuf), (char *)fmt, ap);
		n = strlen(cp);
		(void)snprintf(&cp[n], sizeof(kd->errbuf) - n, ": %s",
		    strerror(errno));
	}
	va_end(ap);
}

void *
_kvm_malloc(kd, n)
	kvm_t *kd;
	size_t n;
{
	void *p;

	if ((p = calloc(n, sizeof(char))) == NULL)
		_kvm_err(kd, kd->program, "can't allocate %u bytes: %s",
			 n, strerror(errno));
	return (p);
}

static kvm_t *
_kvm_open(kd, uf, mf, flag, errout)
	kvm_t *kd;
	const char *uf;
	const char *mf;
	int flag;
	char *errout;
{
	struct stat st;

	kd->vmfd = -1;
	kd->pmfd = -1;
	kd->nlfd = -1;
	kd->vmst = 0;
	kd->procbase = 0;
	kd->argspc = 0;
	kd->argv = 0;

	if (uf == 0)
		uf = getbootfile();
	else if (strlen(uf) >= MAXPATHLEN) {
		_kvm_err(kd, kd->program, "exec file name too long");
		goto failed;
	}
	if (flag & ~O_RDWR) {
		_kvm_err(kd, kd->program, "bad flags arg");
		goto failed;
	}
	if (mf == 0)
		mf = _PATH_MEM;

	if ((kd->pmfd = open(mf, flag, 0)) < 0) {
		_kvm_syserr(kd, kd->program, "%s", mf);
		goto failed;
	}
	if (fstat(kd->pmfd, &st) < 0) {
		_kvm_syserr(kd, kd->program, "%s", mf);
		goto failed;
	}
	if (S_ISREG(st.st_mode) && st.st_size <= 0) {
		errno = EINVAL;
		_kvm_syserr(kd, kd->program, "empty file");
		goto failed;
	}
	if (fcntl(kd->pmfd, F_SETFD, FD_CLOEXEC) < 0) {
		_kvm_syserr(kd, kd->program, "%s", mf);
		goto failed;
	}
	if (S_ISCHR(st.st_mode)) {
		/*
		 * If this is a character special device, then check that
		 * it's /dev/mem.  If so, open kmem too.  (Maybe we should
		 * make it work for either /dev/mem or /dev/kmem -- in either
		 * case you're working with a live kernel.)
		 */
		if (strcmp(mf, _PATH_DEVNULL) == 0) {
			kd->vmfd = open(_PATH_DEVNULL, O_RDONLY);
			return (kd);
		} else if (strcmp(mf, _PATH_MEM) == 0) {
			if ((kd->vmfd = open(_PATH_KMEM, flag)) < 0) {
				_kvm_syserr(kd, kd->program, "%s", _PATH_KMEM);
				goto failed;
			}
			if (fcntl(kd->vmfd, F_SETFD, FD_CLOEXEC) < 0) {
				_kvm_syserr(kd, kd->program, "%s", _PATH_KMEM);
				goto failed;
			}
			return (kd);
		}
	}
	/*
	 * This is a crash dump.
	 * Initialize the virtual address translation machinery,
	 * but first setup the namelist fd.
	 */
	if ((kd->nlfd = open(uf, O_RDONLY, 0)) < 0) {
		_kvm_syserr(kd, kd->program, "%s", uf);
		goto failed;
	}
	if (fcntl(kd->nlfd, F_SETFD, FD_CLOEXEC) < 0) {
		_kvm_syserr(kd, kd->program, "%s", uf);
		goto failed;
	}
	if (strncmp(mf, _PATH_FWMEM, strlen(_PATH_FWMEM)) == 0)
		kd->rawdump = 1;
	if (_kvm_initvtop(kd) < 0)
		goto failed;
	return (kd);
failed:
	/*
	 * Copy out the error if doing sane error semantics.
	 */
	if (errout != 0)
		strlcpy(errout, kd->errbuf, _POSIX2_LINE_MAX);
	(void)kvm_close(kd);
	return (0);
}

kvm_t *
kvm_openfiles(uf, mf, sf, flag, errout)
	const char *uf;
	const char *mf;
	const char *sf __unused;
	int flag;
	char *errout;
{
	kvm_t *kd;

	if ((kd = calloc(1, sizeof(*kd))) == NULL) {
		(void)strlcpy(errout, strerror(errno), _POSIX2_LINE_MAX);
		return (0);
	}
	kd->program = 0;
	return (_kvm_open(kd, uf, mf, flag, errout));
}

kvm_t *
kvm_open(uf, mf, sf, flag, errstr)
	const char *uf;
	const char *mf;
	const char *sf __unused;
	int flag;
	const char *errstr;
{
	kvm_t *kd;

	if ((kd = calloc(1, sizeof(*kd))) == NULL) {
		if (errstr != NULL)
			(void)fprintf(stderr, "%s: %s\n",
				      errstr, strerror(errno));
		return (0);
	}
	kd->program = errstr;
	return (_kvm_open(kd, uf, mf, flag, NULL));
}

int
kvm_close(kd)
	kvm_t *kd;
{
	int error = 0;

	if (kd->pmfd >= 0)
		error |= close(kd->pmfd);
	if (kd->vmfd >= 0)
		error |= close(kd->vmfd);
	if (kd->nlfd >= 0)
		error |= close(kd->nlfd);
	if (kd->vmst)
		_kvm_freevtop(kd);
	if (kd->procbase != 0)
		free((void *)kd->procbase);
	if (kd->argbuf != 0)
		free((void *) kd->argbuf);
	if (kd->argspc != 0)
		free((void *) kd->argspc);
	if (kd->argv != 0)
		free((void *)kd->argv);
	free((void *)kd);

	return (0);
}

/*
 * Walk the list of unresolved symbols, generate a new list and prefix the
 * symbol names, try again, and merge back what we could resolve.
 */
static int
kvm_fdnlist_prefix(kvm_t *kd, struct nlist *nl, int missing, const char *prefix,
    uintptr_t (*validate_fn)(kvm_t *, uintptr_t))
{
	struct nlist *n, *np, *p;
	char *cp, *ce;
	size_t len;
	int unresolved;

	/*
	 * Calculate the space we need to malloc for nlist and names.
	 * We are going to store the name twice for later lookups: once
	 * with the prefix and once the unmodified name delmited by \0.
	 */
	len = 0;
	unresolved = 0;
	for (p = nl; p->n_name && p->n_name[0]; ++p) {
		if (p->n_type != N_UNDF)
			continue;
		len += sizeof(struct nlist) + strlen(prefix) +
		    2 * (strlen(p->n_name) + 1);
		unresolved++;
	}
	if (unresolved == 0)
		return (unresolved);
	/* Add space for the terminating nlist entry. */
	len += sizeof(struct nlist);
	unresolved++;

	/* Alloc one chunk for (nlist, [names]) and setup pointers. */
	n = np = malloc(len);
	bzero(n, len);
	if (n == NULL)
		return (missing);
	cp = ce = (char *)np;
	cp += unresolved * sizeof(struct nlist);
	ce += len;

	/* Generate shortened nlist with special prefix. */
	unresolved = 0;
	for (p = nl; p->n_name && p->n_name[0]; ++p) {
		if (p->n_type != N_UNDF)
			continue;
		bcopy(p, np, sizeof(struct nlist));
		/* Save the new\0orig. name so we can later match it again. */
		len = snprintf(cp, ce - cp, "%s%s%c%s", prefix,
		    (prefix[0] != '\0' && p->n_name[0] == '_') ?
			(p->n_name + 1) : p->n_name, '\0', p->n_name);
		if (len >= ce - cp)
			continue;
		np->n_name = cp;
		cp += len + 1;
		np++;
		unresolved++;
	}

	/* Do lookup on the reduced list. */
	np = n;
	unresolved = __fdnlist(kd->nlfd, np);

	/* Check if we could resolve further symbols and update the list. */
	if (unresolved >= 0 && unresolved < missing) {
		/* Find the first freshly resolved entry. */
		for (; np->n_name && np->n_name[0]; np++)
			if (np->n_type != N_UNDF)
				break;
		/*
		 * The lists are both in the same order,
		 * so we can walk them in parallel.
		 */
		for (p = nl; np->n_name && np->n_name[0] &&
		    p->n_name && p->n_name[0]; ++p) {
			if (p->n_type != N_UNDF)
				continue;
			/* Skip expanded name and compare to orig. one. */
			cp = np->n_name + strlen(np->n_name) + 1;
			if (strcmp(cp, p->n_name))
				continue;
			/* Update nlist with new, translated results. */
			p->n_type = np->n_type;
			p->n_other = np->n_other;
			p->n_desc = np->n_desc;
			if (validate_fn)
				p->n_value = (*validate_fn)(kd, np->n_value);
			else
				p->n_value = np->n_value;
			missing--;
			/* Find next freshly resolved entry. */
			for (np++; np->n_name && np->n_name[0]; np++)
				if (np->n_type != N_UNDF)
					break;
		}
	}
	/* We could assert missing = unresolved here. */

	free(n);
	return (unresolved);
}

int
_kvm_nlist(kvm_t *kd, struct nlist *nl, int initialize)
{
	struct nlist *p;
	int nvalid;
	struct kld_sym_lookup lookup;
	int error;
	char *prefix = "", symname[1024]; /* XXX-BZ symbol name length limit? */
	int tried_vnet, tried_dpcpu;

	/*
	 * If we can't use the kld symbol lookup, revert to the
	 * slow library call.
	 */
	if (!ISALIVE(kd)) {
		error = __fdnlist(kd->nlfd, nl);
		if (error <= 0)			/* Hard error or success. */
			return (error);

		if (_kvm_vnet_initialized(kd, initialize))
			error = kvm_fdnlist_prefix(kd, nl, error,
			    VNET_SYMPREFIX, _kvm_vnet_validaddr);

		if (error > 0 && _kvm_dpcpu_initialized(kd, initialize))
			error = kvm_fdnlist_prefix(kd, nl, error,
			    DPCPU_SYMPREFIX, _kvm_dpcpu_validaddr);

		return (error);
	}

	/*
	 * We can use the kld lookup syscall.  Go through each nlist entry
	 * and look it up with a kldsym(2) syscall.
	 */
	nvalid = 0;
	tried_vnet = 0;
	tried_dpcpu = 0;
again:
	for (p = nl; p->n_name && p->n_name[0]; ++p) {
		if (p->n_type != N_UNDF)
			continue;

		lookup.version = sizeof(lookup);
		lookup.symvalue = 0;
		lookup.symsize = 0;

		error = snprintf(symname, sizeof(symname), "%s%s", prefix,
		    (prefix[0] != '\0' && p->n_name[0] == '_') ?
			(p->n_name + 1) : p->n_name);
		if (error >= sizeof(symname))
			continue;

		lookup.symname = symname;
		if (lookup.symname[0] == '_')
			lookup.symname++;

		if (kldsym(0, KLDSYM_LOOKUP, &lookup) != -1) {
			p->n_type = N_TEXT;
			p->n_other = 0;
			p->n_desc = 0;
			if (_kvm_vnet_initialized(kd, initialize) &&
			    !strcmp(prefix, VNET_SYMPREFIX))
				p->n_value =
				    _kvm_vnet_validaddr(kd, lookup.symvalue);
			else if (_kvm_dpcpu_initialized(kd, initialize) &&
			    !strcmp(prefix, DPCPU_SYMPREFIX))
				p->n_value =
				    _kvm_dpcpu_validaddr(kd, lookup.symvalue);
			else
				p->n_value = lookup.symvalue;
			++nvalid;
			/* lookup.symsize */
		}
	}

	/*
	 * Check the number of entries that weren't found. If they exist,
	 * try again with a prefix for virtualized or DPCPU symbol names.
	 */
	error = ((p - nl) - nvalid);
	if (error && _kvm_vnet_initialized(kd, initialize) && !tried_vnet) {
		tried_vnet = 1;
		prefix = VNET_SYMPREFIX;
		goto again;
	}
	if (error && _kvm_dpcpu_initialized(kd, initialize) && !tried_dpcpu) {
		tried_dpcpu = 1;
		prefix = DPCPU_SYMPREFIX;
		goto again;
	}

	/*
	 * Return the number of entries that weren't found. If they exist,
	 * also fill internal error buffer.
	 */
	error = ((p - nl) - nvalid);
	if (error)
		_kvm_syserr(kd, kd->program, "kvm_nlist");
	return (error);
}

int
kvm_nlist(kd, nl)
	kvm_t *kd;
	struct nlist *nl;
{

	/*
	 * If called via the public interface, permit intialization of
	 * further virtualized modules on demand.
	 */
	return (_kvm_nlist(kd, nl, 1));
}

ssize_t
kvm_read(kd, kva, buf, len)
	kvm_t *kd;
	u_long kva;
	void *buf;
	size_t len;
{
	int cc;
	char *cp;

	if (ISALIVE(kd)) {
		/*
		 * We're using /dev/kmem.  Just read straight from the
		 * device and let the active kernel do the address translation.
		 */
		errno = 0;
		if (lseek(kd->vmfd, (off_t)kva, 0) == -1 && errno != 0) {
			_kvm_err(kd, 0, "invalid address (%x)", kva);
			return (-1);
		}
		cc = read(kd->vmfd, buf, len);
		if (cc < 0) {
			_kvm_syserr(kd, 0, "kvm_read");
			return (-1);
		} else if (cc < len)
			_kvm_err(kd, kd->program, "short read");
		return (cc);
	} else {
		cp = buf;
		while (len > 0) {
			off_t pa;

			cc = _kvm_kvatop(kd, kva, &pa);
			if (cc == 0)
				return (-1);
			if (cc > len)
				cc = len;
			errno = 0;
			if (lseek(kd->pmfd, pa, 0) == -1 && errno != 0) {
				_kvm_syserr(kd, 0, _PATH_MEM);
				break;
			}
			cc = read(kd->pmfd, cp, cc);
			if (cc < 0) {
				_kvm_syserr(kd, kd->program, "kvm_read");
				break;
			}
			/*
			 * If kvm_kvatop returns a bogus value or our core
			 * file is truncated, we might wind up seeking beyond
			 * the end of the core file in which case the read will
			 * return 0 (EOF).
			 */
			if (cc == 0)
				break;
			cp += cc;
			kva += cc;
			len -= cc;
		}
		return (cp - (char *)buf);
	}
	/* NOTREACHED */
}

ssize_t
kvm_write(kd, kva, buf, len)
	kvm_t *kd;
	u_long kva;
	const void *buf;
	size_t len;
{
	int cc;

	if (ISALIVE(kd)) {
		/*
		 * Just like kvm_read, only we write.
		 */
		errno = 0;
		if (lseek(kd->vmfd, (off_t)kva, 0) == -1 && errno != 0) {
			_kvm_err(kd, 0, "invalid address (%x)", kva);
			return (-1);
		}
		cc = write(kd->vmfd, buf, len);
		if (cc < 0) {
			_kvm_syserr(kd, 0, "kvm_write");
			return (-1);
		} else if (cc < len)
			_kvm_err(kd, kd->program, "short write");
		return (cc);
	} else {
		_kvm_err(kd, kd->program,
		    "kvm_write not implemented for dead kernels");
		return (-1);
	}
	/* NOTREACHED */
}
