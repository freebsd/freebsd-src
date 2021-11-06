/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/wait.h>

#include <machine/elf.h>

#include <arpa/inet.h>

#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <rtld_paths.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * 32-bit ELF data structures can only be used if the system header[s] declare
 * them.  There is no official macro for determining whether they are declared,
 * so check for the existence of one of the 32-macros defined in elf(5).
 */
#ifdef ELF32_R_TYPE
#define	ELF32_SUPPORTED
#endif

#define	LDD_SETENV(name, value, overwrite) do {		\
	setenv("LD_" name, value, overwrite);		\
	setenv("LD_32_" name, value, overwrite);	\
} while (0)

#define	LDD_UNSETENV(name) do {		\
	unsetenv("LD_" name);		\
	unsetenv("LD_32_" name);	\
} while (0)

static int	is_executable(const char *fname, int fd, int *is_shlib,
		    int *type);
static void	usage(void);

#define	TYPE_UNKNOWN	0
#define	TYPE_ELF	1	/* Architecture default */
#if __ELF_WORD_SIZE > 32 && defined(ELF32_SUPPORTED)
#define	TYPE_ELF32	2	/* Explicit 32 bits on architectures >32 bits */

#define	_PATH_LDD32	"/usr/bin/ldd32"

static int
execldd32(char *file, char *fmt1, char *fmt2, int aflag)
{
	char *argv[9];
	int i, rval, status;

	LDD_UNSETENV("TRACE_LOADED_OBJECTS");
	rval = 0;
	i = 0;
	argv[i++] = strdup(_PATH_LDD32);
	if (aflag)
		argv[i++] = strdup("-a");
	if (fmt1 != NULL) {
		argv[i++] = strdup("-f");
		argv[i++] = strdup(fmt1);
	}
	if (fmt2 != NULL) {
		argv[i++] = strdup("-f");
		argv[i++] = strdup(fmt2);
	}
	argv[i++] = strdup(file);
	argv[i++] = NULL;

	switch (fork()) {
	case -1:
		err(1, "fork");
		break;
	case 0:
		execv(_PATH_LDD32, argv);
		warn("%s", _PATH_LDD32);
		_exit(127);
		break;
	default:
		if (wait(&status) < 0)
			rval = 1;
		else if (WIFSIGNALED(status))
			rval = 1;
		else if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
			rval = 1;
		break;
	}
	while (i--)
		free(argv[i]);
	LDD_SETENV("TRACE_LOADED_OBJECTS", "yes", 1);
	return (rval);
}
#endif

int
main(int argc, char *argv[])
{
	char *fmt1, *fmt2;
	const char *rtld;
	int aflag, c, fd, rval, status, is_shlib, rv, type;

	aflag = 0;
	fmt1 = fmt2 = NULL;

	while ((c = getopt(argc, argv, "af:")) != -1) {
		switch (c) {
		case 'a':
			aflag++;
			break;
		case 'f':
			if (fmt1 != NULL) {
				if (fmt2 != NULL)
					errx(1, "too many formats");
				fmt2 = optarg;
			} else
				fmt1 = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc <= 0) {
		usage();
		/* NOTREACHED */
	}

	rval = 0;
	for (; argc > 0; argc--, argv++) {
		if ((fd = open(*argv, O_RDONLY, 0)) < 0) {
			warn("%s", *argv);
			rval |= 1;
			continue;
		}
		rv = is_executable(*argv, fd, &is_shlib, &type);
		close(fd);
		if (rv == 0) {
			rval |= 1;
			continue;
		}

		switch (type) {
		case TYPE_ELF:
			break;
#if __ELF_WORD_SIZE > 32 && defined(ELF32_SUPPORTED)
		case TYPE_ELF32:
			rval |= execldd32(*argv, fmt1, fmt2, aflag);
			continue;
#endif
		case TYPE_UNKNOWN:
		default:
			/*
			 * This shouldn't happen unless is_executable()
			 * is broken.
			 */
			errx(EDOOFUS, "unknown executable type");
		}

		/* ld.so magic */
		LDD_SETENV("TRACE_LOADED_OBJECTS", "yes", 1);
		if (fmt1 != NULL)
			LDD_SETENV("TRACE_LOADED_OBJECTS_FMT1", fmt1, 1);
		if (fmt2 != NULL)
			LDD_SETENV("TRACE_LOADED_OBJECTS_FMT2", fmt2, 1);

		LDD_SETENV("TRACE_LOADED_OBJECTS_PROGNAME", *argv, 1);
		if (aflag)
			LDD_SETENV("TRACE_LOADED_OBJECTS_ALL", "1", 1);
		else if (fmt1 == NULL && fmt2 == NULL)
			/* Default formats */
			printf("%s:\n", *argv);
		fflush(stdout);

		switch (fork()) {
		case -1:
			err(1, "fork");
			break;
		default:
			if (wait(&status) < 0) {
				warn("wait");
				rval |= 1;
			} else if (WIFSIGNALED(status)) {
				fprintf(stderr, "%s: signal %d\n", *argv,
				    WTERMSIG(status));
				rval |= 1;
			} else if (WIFEXITED(status) &&
			    WEXITSTATUS(status) != 0) {
				fprintf(stderr, "%s: exit status %d\n", *argv,
				    WEXITSTATUS(status));
				rval |= 1;
			}
			break;
		case 0:
			if (is_shlib == 0) {
				execl(*argv, *argv, (char *)NULL);
				warn("%s", *argv);
			} else if (fmt1 == NULL && fmt2 == NULL) {
				dlopen(*argv, RTLD_TRACE);
				warnx("%s: %s", *argv, dlerror());
			} else {
				rtld = _PATH_RTLD;
#if __ELF_WORD_SIZE > 32 && defined(ELF32_SUPPORTED)
				if (type == TYPE_ELF32)
					rtld = _COMPAT32_PATH_RTLD;
#endif
				execl(rtld, rtld, "-d", "--",
				    *argv, (char *)NULL);
			}
			_exit(1);
		}
	}

	return (rval);
}

static void
usage(void)
{

	fprintf(stderr, "usage: ldd [-a] [-f format] program ...\n");
	exit(1);
}

static bool
has_freebsd_abi_tag(const char *fname, Elf *elf, GElf_Ehdr *ehdr, off_t offset,
    size_t len)
{
	Elf_Data dst, src;
	const Elf_Note *note;
	char *buf;
	const char *name;
	void *copy;
	size_t namesz, descsz;
	bool has_abi_tag;

	buf = elf_rawfile(elf, NULL);
	if (buf == NULL) {
		warnx("%s: %s", fname, elf_errmsg(0));
		return (false);
	}

	memset(&src, 0, sizeof(src));
	src.d_buf = buf + offset;
	src.d_size = len;
	src.d_type = ELF_T_NOTE;
	src.d_version = EV_CURRENT;

	memset(&dst, 0, sizeof(dst));
	dst.d_buf = copy = malloc(len);
	dst.d_size = len;
	dst.d_type = ELF_T_NOTE;
	dst.d_version = EV_CURRENT;

	if (gelf_xlatetom(elf, &dst, &src, ehdr->e_ident[EI_DATA]) == NULL) {
		warnx("%s: failed to parse notes: %s", fname, elf_errmsg(0));
		free(copy);
		return (false);
	}

	buf = copy;
	has_abi_tag = false;
	for (;;) {
		if (len < sizeof(*note))
			break;

		note = (const void *)buf;
		buf += sizeof(*note);
		len -= sizeof(*note);

		namesz = roundup2(note->n_namesz, sizeof(uint32_t));
		descsz = roundup2(note->n_descsz, sizeof(uint32_t));
		if (len < namesz + descsz)
			break;

		name = buf;
		if (note->n_namesz == sizeof(ELF_NOTE_FREEBSD) &&
		    strncmp(name, ELF_NOTE_FREEBSD, note->n_namesz) == 0 &&
		    note->n_type == NT_FREEBSD_ABI_TAG &&
		    note->n_descsz == sizeof(uint32_t)) {
			has_abi_tag = true;
			break;
		}

		buf += namesz + descsz;
		len -= namesz + descsz;
	}

	free(copy);
	return (has_abi_tag);
}

static bool
is_pie(const char *fname, Elf *elf, GElf_Ehdr *ehdr, off_t offset, size_t len)
{
	Elf_Data dst, src;
	char *buf;
	void *copy;
	const GElf_Dyn *dyn;
	size_t dynsize;
	u_int count, i;
	bool pie;

	buf = elf_rawfile(elf, NULL);
	if (buf == NULL) {
		warnx("%s: %s", fname, elf_errmsg(0));
		return (false);
	}

	dynsize = gelf_fsize(elf, ELF_T_DYN, 1, EV_CURRENT);
	if (dynsize == 0) {
		warnx("%s: %s", fname, elf_errmsg(0));
		return (false);
	}
	count = len / dynsize;

	memset(&src, 0, sizeof(src));
	src.d_buf = buf + offset;
	src.d_size = len;
	src.d_type = ELF_T_DYN;
	src.d_version = EV_CURRENT;

	memset(&dst, 0, sizeof(dst));
	dst.d_buf = copy = malloc(count * sizeof(*dyn));
	dst.d_size = count * sizeof(*dyn);
	dst.d_type = ELF_T_DYN;
	dst.d_version = EV_CURRENT;

	if (gelf_xlatetom(elf, &dst, &src, ehdr->e_ident[EI_DATA]) == NULL) {
		warnx("%s: failed to parse .dynamic: %s", fname, elf_errmsg(0));
		free(copy);
		return (false);
	}

	dyn = copy;
	pie = false;
	for (i = 0; i < count; i++) {
		if (dyn[i].d_tag != DT_FLAGS_1)
			continue;

		pie = (dyn[i].d_un.d_val & DF_1_PIE) != 0;
		break;
	}

	free(copy);
	return (pie);
}

static int
is_executable(const char *fname, int fd, int *is_shlib, int *type)
{
	Elf *elf;
	GElf_Ehdr ehdr;
	GElf_Phdr phdr;
	bool dynamic, freebsd, pie;
	int i;

	*is_shlib = 0;
	*type = TYPE_UNKNOWN;
	dynamic = false;
	freebsd = false;
	pie = false;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		warnx("unsupported libelf");
		return (0);
	}
	elf = elf_begin(fd, ELF_C_READ, NULL);
	if (elf == NULL) {
		warnx("%s: %s", fname, elf_errmsg(0));
		return (0);
	}
	if (elf_kind(elf) != ELF_K_ELF) {
		elf_end(elf);
		warnx("%s: not a dynamic ELF executable", fname);
		return (0);
	}
	if (gelf_getehdr(elf, &ehdr) == NULL) {
		warnx("%s: %s", fname, elf_errmsg(0));
		elf_end(elf);
		return (0);
	}

	*type = TYPE_ELF;
#if __ELF_WORD_SIZE > 32 && defined(ELF32_SUPPORTED)
	if (gelf_getclass(elf) == ELFCLASS32) {
		*type = TYPE_ELF32;
	}
#endif

	freebsd = ehdr.e_ident[EI_OSABI] == ELFOSABI_FREEBSD;
	for (i = 0; i < ehdr.e_phnum; i++) {
		if (gelf_getphdr(elf, i, &phdr) == NULL) {
			warnx("%s: %s", fname, elf_errmsg(0));
			elf_end(elf);
			return (0);
		}
		switch (phdr.p_type) {
		case PT_NOTE:
			if (ehdr.e_ident[EI_OSABI] == ELFOSABI_NONE && !freebsd)
				freebsd = has_freebsd_abi_tag(fname, elf, &ehdr,
				    phdr.p_offset, phdr.p_filesz);
			break;
		case PT_DYNAMIC:
			dynamic = true;
			if (ehdr.e_type == ET_DYN)
				pie = is_pie(fname, elf, &ehdr, phdr.p_offset,
				    phdr.p_filesz);
			break;
		}
	}

	if (!dynamic) {
		elf_end(elf);
		warnx("%s: not a dynamic ELF executable", fname);
		return (0);
	}

	if (ehdr.e_type == ET_DYN && !pie) {
		*is_shlib = 1;

		if (!freebsd) {
			elf_end(elf);
			warnx("%s: not a FreeBSD ELF shared object", fname);
			return (0);
		}
	}

	elf_end(elf);
	return (1);
}
