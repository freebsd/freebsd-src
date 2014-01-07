/*-
 * Copyright (c) 2012, 2013 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/cheri.h>
#include <machine/cpuregs.h>
#include <machine/sysarch.h>

#include <cheri/sandbox.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <magic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "minifile.h"

enum _sbtype {
	SB_NONE = 0,
	SB_CAPSICUM,
	SB_CHERI
} sbtype = SB_NONE;

int dotimings;

#define MAGIC_FILE	"/usr/share/misc/magic.mgc"

static void
usage(void)
{

	errx(1, "usage: minifile [-t] [-s <sandbox type>] <file> ...\n");
}

/*
 * prep_fds() moves the file descriptors in curfds around such that they
 * are at the corresponding target fd values in preperation for an exec
 * into a capsicum or similar sandbox.  All fd's above the largest value
 * specified in the targetfds array are closed.  It is expected that the
 * values will fall within the range (3 .. <nfds + 2>).  Otherwise an
 * unpredictiable set of files may remain open and information leaks may
 * result unless specifc descriptors were already placed in the other
 * slots.  The programmer should seperately ensure that fds 0, 1, and 2
 * are either the usual stdin, stdout, and stderror or fd's to /dev/null.
 */
int
prep_fds(int *curfds, int *targetfds, int nfds)
{
	int i, maxfd = -1, tmpfd;

	/* Find the largest fd in either the current or target lists */
	for (i = 0; i < nfds; i++)
		if (maxfd < curfds[i])
			maxfd = curfds[i];
	for (i = 0; i < nfds; i++)
		if (maxfd < targetfds[i])
			maxfd = targetfds[i];

	/* Move all the fds up above the largest one */
	for (i = 0; i < nfds; i++) {
		tmpfd = maxfd + 1 + i;
		if (dup2(curfds[i], tmpfd) == -1)
			return (-1);
		close(curfds[i]);
	}

	/* Move them all into their assigned locations. */
	for (i = 0; i < nfds; i++) {
		tmpfd = maxfd + 1 + i;
		if (dup2(tmpfd, targetfds[i]) == -1)
			return (-1);
		close(tmpfd);
	}

	/* Close everything above are new maximum descriptor */
	maxfd = -1;
	for (i = 0; i < nfds; i++)
		if (maxfd < targetfds[i])
			maxfd = targetfds[i];
	closefrom(maxfd + 1);

	return (0);
}

const char *
capsicum_magic_descriptor(int mfd, int fd)
{
	int status;
	pid_t pid;
	ssize_t rlen;
	static char buf[4096];
	char *type, *ttype;
	int pfd[2];
	int curfds[3], targetfds[3];
	uint32_t start, preinvoke, *invoke, postinvoke, end;

	if (dotimings)
		start = sysarch(MIPS_GET_COUNT, NULL);

	if (pipe(pfd) == -1)
		err(1, "pipe()");
	if (dotimings)
		preinvoke = sysarch(MIPS_GET_COUNT, NULL);
	pid = fork();
	if (pid < 0)
		err(1, "fork()");
	else if (pid == 0) {
		close(pfd[0]);
		
		/* XXX: use cap_new() to limit further */
		curfds[0] = fd;
		targetfds[0] = MINIFILE_FILE_FD;
		curfds[1] = mfd;
		targetfds[1] = MINIFILE_MAGIC_FD;
		curfds[2] = pfd[1];
		targetfds[2] = MINIFILE_OUT_FD;

		if (prep_fds(curfds, targetfds, 3) == -1)
			err(1, "pred_fds()");

		execl("/usr/libexec/minifile-capsicum-helper", "minifile-capsicum-helper",
		    NULL);
		err(1, "exec /usr/libexec/minifile-capsicum-helper");
	} else {
		close(pfd[1]);
		while (wait4(pid, &status, 0, NULL) == -1)
			if (errno != EINTR)
				err(1, "wait4()");
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
			warnx("child exited with %d", WEXITSTATUS(status));
			close(pfd[0]);
			type = "badmagic";
		} else if(WIFSIGNALED(status)) {
			warn("child killed by signal %d", WTERMSIG(status));
			close(pfd[0]);
			type = "badmagic";
		} else {
			rlen = read(pfd[0], buf, 128 + sizeof(uint32_t) * 4);
			close(pfd[0]);
			if (dotimings)
				postinvoke = sysarch(MIPS_GET_COUNT, NULL);
			if (rlen == -1)
				type = "read error";
			else if (rlen <= sizeof(uint32_t) * 4 + 1)
				type = "unknown";
			else {
				invoke = (uint32_t*)buf;
				/* Don't trust the result */
				ttype = buf + rlen + sizeof(uint32_t) * 4;
				strvisx(ttype, buf + sizeof(uint32_t) * 4,
				    rlen - sizeof(uint32_t) * 4, 0);
				type = ttype;
			}
		}
	}

	if (dotimings) {
		end = sysarch(MIPS_GET_COUNT, NULL);

		printf("counts: %u %u %u %u %u %u %u %u\n", start, preinvoke,
		    invoke[0], invoke[1], invoke[2], invoke[3], postinvoke, end);
	}

	return type;
}

static struct sandbox *sandbox;
static struct chericap file_cap, magic_cap, out_cap, timing_cap;

const char *
cheri_magic_descriptor(void *magicbuf, size_t magicsize, int fd)
{
	register_t v;
	size_t outsize, filesize;
	char *filebuf = NULL;
	struct stat filesb, magicsb;
	static char outbuf[4096];
	const char *type;
	char *ttype;
	uint32_t start, preinvoke, invoke[4], postinvoke, end;

	type = "badfile";

	if (dotimings)
		start = sysarch(MIPS_GET_COUNT, NULL);

	outsize = 128;
	CHERI_CINCBASE(10, 0, outbuf);
	CHERI_CSETLEN(10, 10, outsize);
	CHERI_CANDPERM(10, 10, CHERI_PERM_STORE);
	CHERI_CSC(10, 0, &out_cap, 0);

	CHERI_CINCBASE(10, 0, magicbuf);
	CHERI_CSETLEN(10, 10, magicsize);
	CHERI_CANDPERM(10, 10, CHERI_PERM_LOAD);
	CHERI_CSC(10, 0, &magic_cap, 0);

	if (fstat(fd, &filesb) == -1)
		err(1, "fstat input fd");
	filesize = MIN(MINIFILE_BUF_MAX, filesb.st_size);
	if ((filebuf = mmap(NULL, filesize, PROT_READ, 0, fd, 0)) ==
	    MAP_FAILED) {
		warn("mmap input fd");
		goto error;
	}
	CHERI_CINCBASE(10, 0, filebuf);
	CHERI_CSETLEN(10, 10, filesize);
	CHERI_CANDPERM(10, 10, CHERI_PERM_LOAD);
	CHERI_CSC(10, 0, &file_cap, 0);

	CHERI_CINCBASE(10, 0, invoke);
	CHERI_CSETLEN(10, 10, sizeof(uint32_t) * 4);
	CHERI_CANDPERM(10, 10, CHERI_PERM_STORE);
	CHERI_CSC(10, 0, &timing_cap, 0);

	if (dotimings)
		preinvoke = sysarch(MIPS_GET_COUNT, NULL);

	v = sandbox_invoke(sandbox, outsize, magicsize, filesize, dotimings,
	    &out_cap, &magic_cap, &file_cap, &timing_cap, NULL, NULL, NULL,
	    NULL);

	if (dotimings)
		postinvoke = sysarch(MIPS_GET_COUNT, NULL);

	outsize = strnlen(outbuf, outsize);
	if (v == 0) {
		ttype = outbuf + outsize;
		strvisx(ttype, outbuf, outsize, 0);
		type = ttype;
	} else
		type = "badmagic";

error:
	if (munmap(filebuf, filesize) == -1)
		warn("munmap filebuf");

	if (dotimings) {
		end = sysarch(MIPS_GET_COUNT, NULL);

		printf("counts: %u %u %u %u %u %u %u %u\n", start, preinvoke,
		    invoke[0], invoke[1], invoke[2], invoke[3], postinvoke, end);
	}

	return type;
}

int
main(int argc, char **argv)
{
	char ch;
	void *magicbuf;
	const char *fname;
	int mfd, fd;
	size_t magicsize;
	const char *type;
	struct magic_set *magic;
	struct stat magicsb;

	while ((ch = getopt(argc, argv, "s:t")) != -1) {
		switch(ch) {
		case 's':
			if (strcmp(optarg, "none") == 0)
				sbtype = SB_NONE;
			else if(strcmp(optarg, "capsicum") == 0)
				sbtype = SB_CAPSICUM;
			else if(strcmp(optarg, "cheri") == 0)
				sbtype = SB_CHERI;
			else {
				warnx("invalid sandbox type %s", optarg);
				usage();
			}
			break;
		case 't':
			dotimings = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc <= 0)
		usage();

	/* Open the magic file */
	mfd = open(MAGIC_FILE, O_RDONLY);
	if (mfd == -1)
		err(1, "open(%s)", MAGIC_FILE);

	/* For the NONE and CHERI cases, pre-map the file */
	if (sbtype == SB_NONE || sbtype == SB_CHERI) {
		if (fstat(mfd, &magicsb) == -1) {
			warn("fstat(%s)", MAGIC_FILE);
			exit(1);
		}
		magicsize = magicsb.st_size;
		if ((magicbuf = mmap(NULL, magicsize, PROT_READ|PROT_WRITE,
		    MAP_PRIVATE, mfd, 0)) == MAP_FAILED) {
			warn("mmap(%s)", MAGIC_FILE);
			magic_close(magic);
			exit(1);
		}
	}

	if (sbtype == SB_NONE) {
		magic = magic_open(MAGIC_MIME_TYPE);
		if (magic == NULL)
			errx(1, "magic_open()");
		if (magic_load_buffers(magic, &magicbuf, &magicsize, 1) == -1) {
			warnx("magic_load() %s", magic_error(magic));
			magic_close(magic);
			exit(1);
		}
	}

	if (sbtype == SB_CHERI)
		if (sandbox_setup("/usr/libexec/minifile-cheri-helper.bin", 8*1024*1024,
		    &sandbox) < 0)
			err(1, "can't create cheri sandbox");

	for (; argc >= 1; argc--, argv++) {
		fname = argv[0];
		fd = open(fname, O_RDONLY);
		if (fd == -1)
			err(1, "open(%s)", fname);
		switch (sbtype) {
		case SB_NONE:
			type = magic_descriptor(magic, fd);
			if (type == NULL)
				errx(1, "magic_file(): %s", magic_error(magic));
			break;
		case SB_CAPSICUM:
			type = capsicum_magic_descriptor(mfd, fd);
			if (type == NULL)
				errx(1, "capsicum_magic_descriptor()");
			break;
		case SB_CHERI:
			type = cheri_magic_descriptor(magicbuf, magicsize, fd);
			if (type == NULL)
				errx(1, "cheri_magic_descriptor()");
			break;
		default:
			errx(1, "invalid sandbox type");
		}
		close(fd);
		printf("%s: %s\n", fname, type);
	}

	if (sbtype == SB_CHERI)
		sandbox_destroy(sandbox);
}	
