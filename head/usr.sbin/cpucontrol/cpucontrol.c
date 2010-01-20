/*-
 * Copyright (c) 2008 Stanislav Sedov <stas@FreeBSD.org>.
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

/*
 * This utility provides userland access to the cpuctl(4) pseudo-device
 * features.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <sysexits.h>
#include <dirent.h>

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/cpuctl.h>

#include "cpucontrol.h"
#include "amd.h"
#include "intel.h"

int	verbosity_level = 0;

#define	DEFAULT_DATADIR	"/usr/local/share/cpucontrol"

#define	FLAG_I	0x01
#define	FLAG_M	0x02
#define	FLAG_U	0x04

#define	OP_INVAL	0x00
#define	OP_READ		0x01
#define	OP_WRITE	0x02
#define	OP_OR		0x04
#define	OP_AND		0x08

#define	HIGH(val)	(uint32_t)(((val) >> 32) & 0xffffffff)
#define	LOW(val)	(uint32_t)((val) & 0xffffffff)

/*
 * Macros for freeing SLISTs, probably must be in /sys/queue.h
 */
#define	SLIST_FREE(head, field, freef) do {				\
		typeof(SLIST_FIRST(head)) __elm0;			\
		typeof(SLIST_FIRST(head)) __elm;			\
		SLIST_FOREACH_SAFE(__elm, (head), field, __elm0)	\
			(void)(freef)(__elm);				\
} while(0);

struct datadir {
	const char		*path;
	SLIST_ENTRY(datadir)	next;
};
static SLIST_HEAD(, datadir) datadirs = SLIST_HEAD_INITIALIZER(datadirs);

struct ucode_handler {
	ucode_probe_t *probe;
	ucode_update_t *update;
} handlers[] = {
	{ intel_probe, intel_update },
	{ amd_probe, amd_update },
};
#define NHANDLERS (sizeof(handlers) / sizeof(*handlers))

static void	usage(void);
static int	isdir(const char *path);
static int	do_cpuid(const char *cmdarg, const char *dev);
static int	do_msr(const char *cmdarg, const char *dev);
static int	do_update(const char *dev);
static void	datadir_add(const char *path);

static void __dead2
usage(void)
{
	const char *name;

	name = getprogname();
	if (name == NULL)
		name = "cpuctl";
	fprintf(stderr, "Usage: %s [-vh] [-d datadir] [-m msr[=value] | "
	    "-i level | -u] device\n", name);
	exit(EX_USAGE);
}

static int
isdir(const char *path)
{
	int error;
	struct stat st;

	error = stat(path, &st);
	if (error < 0) {
		WARN(0, "stat(%s)", path);
		return (error);
	}
	return (st.st_mode & S_IFDIR);
}

static int
do_cpuid(const char *cmdarg, const char *dev)
{
	unsigned int level;
	cpuctl_cpuid_args_t args;
	int fd, error;
	char *endptr;

	assert(cmdarg != NULL);
	assert(dev != NULL);

	level = strtoul(cmdarg, &endptr, 16);
	if (*cmdarg == '\0' || *endptr != '\0') {
		WARNX(0, "incorrect operand: %s", cmdarg);
		usage();
		/* NOTREACHED */
	}

	/*
	 * Fill ioctl argument structure.
	 */
	args.level = level;
	fd = open(dev, O_RDONLY);
	if (fd < 0) {
		WARN(0, "error opening %s for reading", dev);
		return (1);
	}
	error = ioctl(fd, CPUCTL_CPUID, &args);
	if (error < 0) {
		WARN(0, "ioctl(%s, CPUCTL_CPUID)", dev);
		close(fd);
		return (error);
	}
	fprintf(stdout, "cpuid level 0x%x: 0x%.8x 0x%.8x 0x%.8x 0x%.8x\n",
	    level, args.data[0], args.data[1], args.data[2], args.data[3]);
	close(fd);
	return (0);
}

static int
do_msr(const char *cmdarg, const char *dev)
{
	unsigned int msr;
	cpuctl_msr_args_t args;
	size_t len;
	uint64_t data = 0;
	unsigned long command;
	int do_invert = 0, op;
	int fd, error;
	char *endptr;
	char *p;

	assert(cmdarg != NULL);
	assert(dev != NULL);
	len = strlen(cmdarg);
	if (len == 0) {
		WARNX(0, "MSR register expected");
		usage();
		/* NOTREACHED */
	}

	/*
	 * Parse command string.
	 */
	msr = strtoul(cmdarg, &endptr, 16);
	switch (*endptr) {
	case '\0':
		op = OP_READ;
		break;
	case '=':
		op = OP_WRITE;
		break;
	case '&':
		op = OP_AND;
		endptr++;
		break;
	case '|':
		op = OP_OR;
		endptr++;
		break;
	default:
		op = OP_INVAL;
	}
	if (op != OP_READ) {	/* Complex operation. */
		if (*endptr != '=')
			op = OP_INVAL;
		else {
			p = ++endptr;
			if (*p == '~') {
				do_invert = 1;
				p++;
			}
			data = strtoull(p, &endptr, 16);
			if (*p == '\0' || *endptr != '\0') {
				WARNX(0, "argument required: %s", cmdarg);
				usage();
				/* NOTREACHED */
			}
		}
	}
	if (op == OP_INVAL) {
		WARNX(0, "invalid operator: %s", cmdarg);
		usage();
		/* NOTREACHED */
	}

	/*
	 * Fill ioctl argument structure.
	 */
	args.msr = msr;
	if ((do_invert != 0) ^ (op == OP_AND))
		args.data = ~data;
	else
		args.data = data;
	switch (op) {
	case OP_READ:
		command = CPUCTL_RDMSR;
		break;
	case OP_WRITE:
		command = CPUCTL_WRMSR;
		break;
	case OP_OR:
		command = CPUCTL_MSRSBIT;
		break;
	case OP_AND:
		command = CPUCTL_MSRCBIT;
		break;
	default:
		abort();
	}
	fd = open(dev, op == OP_READ ? O_RDONLY : O_WRONLY);
	if (fd < 0) {
		WARN(0, "error opening %s for %s", dev,
		    op == OP_READ ? "reading" : "writing");
		return (1);
	}
	error = ioctl(fd, command, &args);
	if (error < 0) {
		WARN(0, "ioctl(%s, %lu)", dev, command);
		close(fd);
		return (1);
	}
	if (op == OP_READ)
		fprintf(stdout, "MSR 0x%x: 0x%.8x 0x%.8x\n", msr,
		    HIGH(args.data), LOW(args.data));
	close(fd);
	return (0);
}

static int
do_update(const char *dev)
{
	int fd;
	unsigned int i;
	int error;
	struct ucode_handler *handler;
	struct datadir *dir;
	DIR *dirfd;
	struct dirent *direntry;
	char buf[MAXPATHLEN];

	fd = open(dev, O_RDONLY);
	if (fd < 0) {
		WARN(0, "error opening %s for reading", dev);
		return (1);
	}

	/*
	 * Find the appropriate handler for device.
	 */
	for (i = 0; i < NHANDLERS; i++)
		if (handlers[i].probe(fd) == 0)
			break;
	if (i < NHANDLERS)
		handler = &handlers[i];
	else {
		WARNX(0, "cannot find the appropriate handler for device");
		close(fd);
		return (1);
	}
	close(fd);

	/*
	 * Process every image in specified data directories.
	 */
	SLIST_FOREACH(dir, &datadirs, next) {
		dirfd  = opendir(dir->path);
		if (dirfd == NULL) {
			WARNX(1, "skipping directory %s: not accessible", dir->path);
			continue;
		}
		while ((direntry = readdir(dirfd)) != NULL) {
			if (direntry->d_namlen == 0)
				continue;
			error = snprintf(buf, sizeof(buf), "%s/%s", dir->path,
			    direntry->d_name);
			if ((unsigned)error >= sizeof(buf))
				WARNX(0, "skipping %s, buffer too short",
				    direntry->d_name);
			if (isdir(buf) != 0) {
				WARNX(2, "skipping %s: is a directory", buf);
				continue;
			}
			handler->update(dev, buf);
		}
		error = closedir(dirfd);
		if (error != 0)
			WARN(0, "closedir(%s)", dir->path);
	}
	return (0);
}

/*
 * Add new data directory to the search list.
 */
static void
datadir_add(const char *path)
{
	struct datadir *newdir;

	newdir = (struct datadir *)malloc(sizeof(*newdir));
	if (newdir == NULL)
		err(EX_OSERR, "cannot allocate memory");
	newdir->path = path;
	SLIST_INSERT_HEAD(&datadirs, newdir, next);
}

int
main(int argc, char *argv[])
{
	int c, flags;
	const char *cmdarg;
	const char *dev;
	int error;

	flags = 0;
	error = 0;
	cmdarg = "";	/* To keep gcc3 happy. */

	/*
	 * Add all default data dirs to the list first.
	 */
	datadir_add(DEFAULT_DATADIR);
	while ((c = getopt(argc, argv, "d:hi:m:uv")) != -1) {
		switch (c) {
		case 'd':
			datadir_add(optarg);
			break;
		case 'i':
			flags |= FLAG_I;
			cmdarg = optarg;
			break;
		case 'm':
			flags |= FLAG_M;
			cmdarg = optarg;
			break;
		case 'u':
			flags |= FLAG_U;
			break;
		case 'v':
			verbosity_level++;
			break;
		case 'h':
			/* FALLTHROUGH */
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 1) {
		usage();
		/* NOTREACHED */
	}
	dev = argv[0];
	c = flags & (FLAG_I | FLAG_M | FLAG_U);
	switch (c) {
		case FLAG_I:
			error = do_cpuid(cmdarg, dev);
			break;
		case FLAG_M:
			error = do_msr(cmdarg, dev);
			break;
		case FLAG_U:
			error = do_update(dev);
			break;
		default:
			usage();	/* Only one command can be selected. */
	}
	SLIST_FREE(&datadirs, next, free);
	return (error);
}
