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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)kvm.c	8.2 (Berkeley) 2/13/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/swap_pager.h>

#include <machine/vmparam.h>

#include <ctype.h>
#include <db.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kvm_private.h"

static int kvm_dbopen __P((kvm_t *, const char *));

char *
kvm_geterr(kd)
	kvm_t *kd;
{
	return (kd->errbuf);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * Report an error using printf style arguments.  "program" is kd->program
 * on hard errors, and 0 on soft errors, so that under sun error emulation,
 * only hard errors are printed out (otherwise, programs like gdb will
 * generate tons of error messages when trying to access bogus pointers).
 */
void
#if __STDC__
_kvm_err(kvm_t *kd, const char *program, const char *fmt, ...)
#else
_kvm_err(kd, program, fmt, va_alist)
	kvm_t *kd;
	char *program, *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
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
#if __STDC__
_kvm_syserr(kvm_t *kd, const char *program, const char *fmt, ...)
#else
_kvm_syserr(kd, program, fmt, va_alist)
	kvm_t *kd;
	char *program, *fmt;
	va_dcl
#endif
{
	va_list ap;
	register int n;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (program != NULL) {
		(void)fprintf(stderr, "%s: ", program);
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": %s\n", strerror(errno));
	} else {
		register char *cp = kd->errbuf;

		(void)vsnprintf(cp, sizeof(kd->errbuf), (char *)fmt, ap);
		n = strlen(cp);
		(void)snprintf(&cp[n], sizeof(kd->errbuf) - n, ": %s",
		    strerror(errno));
	}
	va_end(ap);
}

void *
_kvm_malloc(kd, n)
	register kvm_t *kd;
	register size_t n;
{
	void *p;

	if ((p = malloc(n)) == NULL)
		_kvm_err(kd, kd->program, strerror(errno));
	memset(p, 0, n);
	return (p);
}

static kvm_t *
_kvm_open(kd, uf, mf, sf, flag, errout)
	register kvm_t *kd;
	const char *uf;
	const char *mf;
	const char *sf;
	int flag;
	char *errout;
{
	struct stat st;

	kd->vmfd = -1;
	kd->pmfd = -1;
	kd->swfd = -1;
	kd->nlfd = -1;
	kd->vmst = 0;
	kd->db = 0;
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
	if (sf == 0)
		sf = _PATH_DRUM;

	if ((kd->pmfd = open(mf, flag, 0)) < 0) {
		_kvm_syserr(kd, kd->program, "%s", mf);
		goto failed;
	}
	if (fstat(kd->pmfd, &st) < 0) {
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
		if (strcmp(mf, _PATH_MEM) != 0) {	/* XXX */
			_kvm_err(kd, kd->program,
				 "%s: not physical memory device", mf);
			goto failed;
		}
		if ((kd->vmfd = open(_PATH_KMEM, flag)) < 0) {
			_kvm_syserr(kd, kd->program, "%s", _PATH_KMEM);
			goto failed;
		}
		if ((kd->swfd = open(sf, flag, 0)) < 0) {
			_kvm_syserr(kd, kd->program, "%s", sf);
			goto failed;
		}
		/*
		 * Open kvm nlist database.  We go ahead and do this
		 * here so that we don't have to hold on to the kernel
		 * path name.  Since a kvm application will surely do
		 * a kvm_nlist(), this probably won't be a wasted effort.
		 * If the database cannot be opened, open the namelist
		 * argument so we revert to slow nlist() calls.
		 */
		if (kvm_dbopen(kd, uf) < 0 &&
		    (kd->nlfd = open(uf, O_RDONLY, 0)) < 0) {
			_kvm_syserr(kd, kd->program, "%s", uf);
			goto failed;
		}
	} else {
		/*
		 * This is a crash dump.
		 * Initalize the virtual address translation machinery,
		 * but first setup the namelist fd.
		 */
		if ((kd->nlfd = open(uf, O_RDONLY, 0)) < 0) {
			_kvm_syserr(kd, kd->program, "%s", uf);
			goto failed;
		}
		if (_kvm_initvtop(kd) < 0)
			goto failed;
	}
	return (kd);
failed:
	/*
	 * Copy out the error if doing sane error semantics.
	 */
	if (errout != 0)
		strcpy(errout, kd->errbuf);
	(void)kvm_close(kd);
	return (0);
}

kvm_t *
kvm_openfiles(uf, mf, sf, flag, errout)
	const char *uf;
	const char *mf;
	const char *sf;
	int flag;
	char *errout;
{
	register kvm_t *kd;

	if ((kd = malloc(sizeof(*kd))) == NULL) {
		(void)strcpy(errout, strerror(errno));
		return (0);
	}
	memset(kd, 0, sizeof(*kd));
	kd->program = 0;
	return (_kvm_open(kd, uf, mf, sf, flag, errout));
}

kvm_t *
kvm_open(uf, mf, sf, flag, errstr)
	const char *uf;
	const char *mf;
	const char *sf;
	int flag;
	const char *errstr;
{
	register kvm_t *kd;

	if ((kd = malloc(sizeof(*kd))) == NULL) {
		if (errstr != NULL)
			(void)fprintf(stderr, "%s: %s\n",
				      errstr, strerror(errno));
		return (0);
	}
	memset(kd, 0, sizeof(*kd));
	kd->program = errstr;
	return (_kvm_open(kd, uf, mf, sf, flag, NULL));
}

int
kvm_close(kd)
	kvm_t *kd;
{
	register int error = 0;

	if (kd->pmfd >= 0)
		error |= close(kd->pmfd);
	if (kd->vmfd >= 0)
		error |= close(kd->vmfd);
	if (kd->nlfd >= 0)
		error |= close(kd->nlfd);
	if (kd->swfd >= 0)
		error |= close(kd->swfd);
	if (kd->db != 0)
		error |= (kd->db->close)(kd->db);
	if (kd->vmst)
		_kvm_freevtop(kd);
	if (kd->procbase != 0)
		free((void *)kd->procbase);
	if (kd->argv != 0)
		free((void *)kd->argv);
	free((void *)kd);

	return (0);
}

/*
 * Set up state necessary to do queries on the kernel namelist
 * data base.  If the data base is out-of-data/incompatible with
 * given executable, set up things so we revert to standard nlist call.
 * Only called for live kernels.  Return 0 on success, -1 on failure.
 */
static int
kvm_dbopen(kd, uf)
	kvm_t *kd;
	const char *uf;
{
	char *cp;
	DBT rec;
	int dbversionlen;
	struct nlist nitem;
	char dbversion[_POSIX2_LINE_MAX];
	char kversion[_POSIX2_LINE_MAX];
	char dbname[MAXPATHLEN];

	if ((cp = rindex(uf, '/')) != 0)
		uf = cp + 1;

	(void)snprintf(dbname, sizeof(dbname), "%skvm_%s.db", _PATH_VARDB, uf);
	kd->db = dbopen(dbname, O_RDONLY, 0, DB_HASH, NULL);
	if (kd->db == 0)
		return (-1);
	/*
	 * read version out of database
	 */
	rec.data = VRS_KEY;
	rec.size = sizeof(VRS_KEY) - 1;
	if ((kd->db->get)(kd->db, (DBT *)&rec, (DBT *)&rec, 0))
		goto close;
	if (rec.data == 0 || rec.size > sizeof(dbversion))
		goto close;

	bcopy(rec.data, dbversion, rec.size);
	dbversionlen = rec.size;
	/*
	 * Read version string from kernel memory.
	 * Since we are dealing with a live kernel, we can call kvm_read()
	 * at this point.
	 */
	rec.data = VRS_SYM;
	rec.size = sizeof(VRS_SYM) - 1;
	if ((kd->db->get)(kd->db, (DBT *)&rec, (DBT *)&rec, 0))
		goto close;
	if (rec.data == 0 || rec.size != sizeof(struct nlist))
		goto close;
	bcopy((char *)rec.data, (char *)&nitem, sizeof(nitem));
	if (kvm_read(kd, (u_long)nitem.n_value, kversion, dbversionlen) !=
	    dbversionlen)
		goto close;
	/*
	 * If they match, we win - otherwise clear out kd->db so
	 * we revert to slow nlist().
	 */
	if (bcmp(dbversion, kversion, dbversionlen) == 0)
		return (0);
close:
	(void)(kd->db->close)(kd->db);
	kd->db = 0;

	return (-1);
}

int
kvm_nlist(kd, nl)
	kvm_t *kd;
	struct nlist *nl;
{
	register struct nlist *p;
	register int nvalid;

	/*
	 * If we can't use the data base, revert to the
	 * slow library call.
	 */
	if (kd->db == 0)
		return (__fdnlist(kd->nlfd, nl));

	/*
	 * We can use the kvm data base.  Go through each nlist entry
	 * and look it up with a db query.
	 */
	nvalid = 0;
	for (p = nl; p->n_name && p->n_name[0]; ++p) {
		register int len;
		DBT rec;

		if ((len = strlen(p->n_name)) > 4096) {
			/* sanity */
			_kvm_err(kd, kd->program, "symbol too large");
			return (-1);
		}
		rec.data = p->n_name;
		rec.size = len;
		if ((kd->db->get)(kd->db, (DBT *)&rec, (DBT *)&rec, 0))
			continue;
		if (rec.data == 0 || rec.size != sizeof(struct nlist))
			continue;
		++nvalid;
		/*
		 * Avoid alignment issues.
		 */
		bcopy((char *)&((struct nlist *)rec.data)->n_type,
		      (char *)&p->n_type,
		      sizeof(p->n_type));
		bcopy((char *)&((struct nlist *)rec.data)->n_value,
		      (char *)&p->n_value,
		      sizeof(p->n_value));
	}
	/*
	 * Return the number of entries that weren't found.
	 */
	return ((p - nl) - nvalid);
}

ssize_t
kvm_read(kd, kva, buf, len)
	kvm_t *kd;
	register u_long kva;
	register void *buf;
	register size_t len;
{
	register int cc;
	register void *cp;

	if (ISALIVE(kd)) {
		/*
		 * We're using /dev/kmem.  Just read straight from the
		 * device and let the active kernel do the address translation.
		 */
		errno = 0;
		if (lseek(kd->vmfd, (off_t)kva, 0) == -1 && errno != 0) {
			_kvm_err(kd, 0, "invalid address (%x)", kva);
			return (0);
		}
		cc = read(kd->vmfd, buf, len);
		if (cc < 0) {
			_kvm_syserr(kd, 0, "kvm_read");
			return (0);
		} else if (cc < len)
			_kvm_err(kd, kd->program, "short read");
		return (cc);
	} else {
		cp = buf;
		while (len > 0) {
			u_long pa;

			cc = _kvm_kvatop(kd, kva, &pa);
			if (cc == 0)
				return (0);
			if (cc > len)
				cc = len;
			errno = 0;
			if (lseek(kd->pmfd, (off_t)pa, 0) == -1 && errno != 0) {
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
			(char *)cp += cc;
			kva += cc;
			len -= cc;
		}
		return ((char *)cp - (char *)buf);
	}
	/* NOTREACHED */
}

ssize_t
kvm_write(kd, kva, buf, len)
	kvm_t *kd;
	register u_long kva;
	register const void *buf;
	register size_t len;
{
	register int cc;

	if (ISALIVE(kd)) {
		/*
		 * Just like kvm_read, only we write.
		 */
		errno = 0;
		if (lseek(kd->vmfd, (off_t)kva, 0) == -1 && errno != 0) {
			_kvm_err(kd, 0, "invalid address (%x)", kva);
			return (0);
		}
		cc = write(kd->vmfd, buf, len);
		if (cc < 0) {
			_kvm_syserr(kd, 0, "kvm_write");
			return (0);
		} else if (cc < len)
			_kvm_err(kd, kd->program, "short write");
		return (cc);
	} else {
		_kvm_err(kd, kd->program,
		    "kvm_write not implemented for dead kernels");
		return (0);
	}
	/* NOTREACHED */
}
