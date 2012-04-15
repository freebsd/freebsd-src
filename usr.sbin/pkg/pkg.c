/*-
 * Copyright (c) 2012 Baptiste Daroussin <bapt@FreeBSD.org>
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
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/elf_common.h>
#include <sys/endian.h>

#include <archive.h>
#include <archive_entry.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fetch.h>

#include "elf_tables.h"

#define _LOCALBASE "/usr/local"
#define _PKGS_URL "http://pkgbeta.FreeBSD.org"
#define _DEFAULT_TMP "/tmp"

static const char *
elf_corres_to_string(struct _elf_corres* m, int e)
{
	int i;

	for (i = 0; m[i].string != NULL; i++)
		if (m[i].elf_nb == e)
			return (m[i].string);

	return ("unknown");
}

static int
pkg_get_myabi(char *dest, size_t sz)
{
	Elf *elf;
	GElf_Ehdr elfhdr;
	GElf_Shdr shdr;
	Elf_Data *data;
	Elf_Note note;
	Elf_Scn *scn;
	char *src, *osname;
	const char *abi;
	int fd, i, ret;
	uint32_t version;

	version = 0;
	ret = 0;
	scn = NULL;
	abi = NULL;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		warnx("ELF library initialization failed: %s", elf_errmsg(-1));
		return -1;
	}

	if ((fd = open("/bin/sh", O_RDONLY)) < 0) {
		warn("open()");
		return -1;
	}

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		ret = -1;
		warnx("elf_begin() failed: %s.", elf_errmsg(-1));
		goto cleanup;
	}

	if (gelf_getehdr(elf, &elfhdr) == NULL) {
		ret = -1;
		warn("getehdr() failed: %s.", elf_errmsg(-1));
		goto cleanup;
	}

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			ret = -1;
			warn("getshdr() failed: %s.", elf_errmsg(-1));
			goto cleanup;
		}

		if (shdr.sh_type == SHT_NOTE)
			break;
	}

	if (scn == NULL) {
		ret = -1;
		warn("fail to get the note section");
		goto cleanup;
	}

	data = elf_getdata(scn, NULL);
	src = data->d_buf;
	for (;;) {
		memcpy(&note, src, sizeof(Elf_Note));
		src += sizeof(Elf_Note);
		if (note.n_type == NT_VERSION)
			break;
		src += note.n_namesz + note.n_descsz;
	}
	osname = src;
	src += note.n_namesz;
	if (elfhdr.e_ident[EI_DATA] == ELFDATA2MSB)
		version = be32dec(src);
	else
		version = le32dec(src);

	for (i = 0; osname[i] != '\0'; i++)
		osname[i] = (char)tolower(osname[i]);

	snprintf(dest, sz, "%s:%d:%s:%s",
	    osname,
	    version / 100000,
	    elf_corres_to_string(mach_corres, (int) elfhdr.e_machine),
	    elf_corres_to_string(wordsize_corres,
	        (int)elfhdr.e_ident[EI_CLASS]));

	switch (elfhdr.e_machine) {
		case EM_ARM:
			snprintf(dest + strlen(dest), sz - strlen(dest),
			    ":%s:%s:%s",
			    elf_corres_to_string(endian_corres,
			        (int) elfhdr.e_ident[EI_DATA]),
			    (elfhdr.e_flags & EF_ARM_NEW_ABI) > 0 ?
			        "eabi" : "oabi",
			    (elfhdr.e_flags & EF_ARM_VFP_FLOAT) > 0 ?
			        "softfp" : "vfp");
			break;
		case EM_MIPS:
			/*
			 * this is taken from binutils sources:
			 * include/elf/mips.h
			 * mapping is figured out from binutils:
			 * gas/config/tc-mips.c
			 */
			switch (elfhdr.e_flags & EF_MIPS_ABI) {
				case E_MIPS_ABI_O32:
					abi = "o32";
					break;
				case E_MIPS_ABI_N32:
					abi = "n32";
					break;
				default:
					if (elfhdr.e_ident[EI_DATA] ==
					    ELFCLASS32)
						abi = "o32";
					else if (elfhdr.e_ident[EI_DATA] ==
					    ELFCLASS64)
						abi = "n64";
					break;
			}
			snprintf(dest + strlen(dest), sz - strlen(dest),
			    ":%s:%s",
			    elf_corres_to_string(endian_corres,
			        (int) elfhdr.e_ident[EI_DATA]),
			    abi);
			break;
	}

cleanup:
	if (elf != NULL)
		elf_end(elf);

	close(fd);
	return (ret);
}

static int
extract_pkg_static(int fd, char *p, int sz)
{
	struct archive *a;
	struct archive_entry *ae;
	char *end;
	int ret, r;

	ret = 0;
	a = archive_read_new();
	archive_read_support_compression_all(a);
	archive_read_support_format_tar(a);

	lseek(fd, 0, 0);

	if (archive_read_open_fd(a, fd, 4096) != ARCHIVE_OK) {
		warnx("archive_read_open_fd: %s",
		    archive_error_string(a));
		ret = -1;
		goto cleanup;
	}

	ae = NULL;
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK) {
		end = strrchr(archive_entry_pathname(ae), '/');
		if (end == NULL)
			continue;

		if (strcmp(end, "/pkg-static") == 0) {
			r = archive_read_extract(a, ae,
			    ARCHIVE_EXTRACT_OWNER |ARCHIVE_EXTRACT_PERM|
			    ARCHIVE_EXTRACT_TIME  |ARCHIVE_EXTRACT_ACL |
			    ARCHIVE_EXTRACT_FFLAGS|ARCHIVE_EXTRACT_XATTR);
			snprintf(p, sz, archive_entry_pathname(ae));
			break;
		}
	}

	if (r != ARCHIVE_OK) {
		warnx("fail to extract pkg-static");
		ret = -1;
	}

cleanup:
	archive_read_finish(a);
	return ret;

}

static int
install_pkg_static(char *path, char *pkgpath)
{
	int pstat;
	pid_t pid;

	switch ((pid = fork())) {
		case -1:
			return (-1);
		case 0:
			execl(path, "pkg-static", "add", pkgpath, (char *)NULL);
			_exit(1); /* NOT REACHED */
		default:
			break;
	}

	while (waitpid(pid, &pstat, 0) == -1) {
		if (errno != EINTR)
			return (-1);
	}

	return (WEXITSTATUS(pstat));
}

static int
bootstrap_pkg(void)
{
	struct url_stat st;
	FILE *remote;
	time_t begin_dl;
	time_t now;
	time_t last = 0;
	char url[MAXPATHLEN];
	char abi[BUFSIZ];
	char tmppkg[MAXPATHLEN];
	char buf[10240];
	char pkgstatic[MAXPATHLEN];
	int fd, retry, ret;
	off_t done, r;

	done = 0;
	ret = 0;
	retry = 3;
	remote = NULL;

	printf("Bootstraping pkg please wait\n");

	if (pkg_get_myabi(abi, MAXPATHLEN) != 0) {
		warnx("fail to determine my abi");
		return -1;
	}

	if (getenv("PACKAGESITE") != NULL) {
		snprintf(url, MAXPATHLEN, "%s/pkg.txz",
		    getenv("PACKAGESITE"));
	} else {
		snprintf(url, MAXPATHLEN, "%s/%s/latest/Latest/pkg.txz",
		    getenv("PACKAGEROOT") ? getenv("PACKAGEROOT") : _PKGS_URL,
		    getenv("ABI") ? getenv("ABI") : abi);
	}

	snprintf(tmppkg, MAXPATHLEN, "%s/pkg.txz.XXXXXX",
	    getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");

	if ((fd = mkstemp(tmppkg)) == -1) {
		warn("mkstemp()");
		return -1;
	}

	while (remote == NULL) {
		remote = fetchXGetURL(url, &st, "");
		if (remote == NULL) {
			--retry;
			if (retry == 0) {
				warnx("Error fetching %s: %s", url,
				    fetchLastErrString);
				ret = 1;
				goto cleanup;
			}
			sleep(1);
		}
	}

	begin_dl = time(NULL);
	while (done < st.size) {
		if ((r = fread(buf, 1, sizeof(buf), remote)) < 1)
			break;

		if (write(fd, buf, r) != r) {
			warn("write()");
			ret = -1;
			goto cleanup;
		}

		done += r;
		now = time(NULL);
		if (now > last || done == st.size) {
			last = now;
		}
	}

	if (ferror(remote)) {
		warnx("Error fetching %s: %s", url,
		    fetchLastErrString);
		ret = 1;
		goto cleanup;
	}

	if ((ret = extract_pkg_static(fd, pkgstatic, MAXPATHLEN)) == 0)
		ret = install_pkg_static(pkgstatic, tmppkg);

cleanup:
	close(fd);
	unlink(tmppkg);

	return 0;
}

int
main(__unused int argc, char * argv[])
{
	char pkgpath[MAXPATHLEN];

	snprintf(pkgpath, MAXPATHLEN, "%s/sbin/pkg",
	    getenv("LOCALBASE") ? getenv("LOCALBASE"): _LOCALBASE);

	if (access(pkgpath, X_OK) == -1)
		bootstrap_pkg();

	execv(pkgpath, argv);

	return (EXIT_SUCCESS);
}
