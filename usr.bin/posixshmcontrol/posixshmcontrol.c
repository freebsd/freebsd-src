/*-
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/filio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <jail.h>
#include <libutil.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
usage(void)
{

	fprintf(stderr, "Usage:\n"
	    "posixshmcontrol create [-m <mode>] [-l <largepage>] <path> ...\n"
	    "posixshmcontrol rm <path> ...\n"
	    "posixshmcontrol ls [-h] [-n] [-j jail]\n"
	    "posixshmcontrol dump <path> ...\n"
	    "posixshmcontrol stat [-h] [-n] <path> ...\n"
	    "posixshmcontrol truncate [-s <newlen>] <path> ...\n");
}

static int
create_one_shm(const char *path, long mode, int idx)
{
	int fd;

	if (idx == -1) {
		fd = shm_open(path, O_RDWR | O_CREAT, mode);
		if (fd == -1) {
			warn("create %s", path);
			return (1);
		}
	} else {
		fd = shm_create_largepage(path, O_RDWR, idx,
		    SHM_LARGEPAGE_ALLOC_DEFAULT, mode);
		if (fd == -1) {
			warn("shm_create_largepage %s psind %d", path, idx);
			return (1);
		}
	}
	close(fd);
	return (0);
}

static int
create_shm(int argc, char **argv)
{
	char *end;
	size_t *pagesizes;
	long mode;
	uint64_t pgsz;
	int c, i, idx, pn, ret, ret1;
	bool printed;

	mode = 0600;
	idx = -1;
	while ((c = getopt(argc, argv, "l:m:")) != -1) {
		switch (c) {
		case 'm':
			errno = 0;
			mode = strtol(optarg, &end, 0);
			if (mode == 0 && errno != 0)
				err(1, "mode");
			if (*end != '\0')
				errx(1, "non-integer mode");
			break;
		case 'l':
			if (expand_number(optarg, &pgsz) == -1)
				err(1, "size");
			pn = getpagesizes(NULL, 0);
			if (pn == -1)
				err(1, "getpagesizes");
			pagesizes = malloc(sizeof(size_t) * pn);
			if (pagesizes == NULL)
				err(1, "malloc");
			if (getpagesizes(pagesizes, pn) == -1)
				err(1, "gtpagesizes");
			for (idx = 0; idx < pn; idx++) {
				if (pagesizes[idx] == pgsz)
					break;
			}
			if (idx == pn) {
				fprintf(stderr,
    "pagesize should be superpagesize, supported sizes:");
				printed = false;
				for (i = 0; i < pn; i++) {
					if (pagesizes[i] == 0 ||
					    pagesizes[i] == (size_t)
					    getpagesize())
						continue;
					printed = true;
					fprintf(stderr, " %zu", pagesizes[i]);
				}
				if (!printed)
					fprintf(stderr, " none");
				fprintf(stderr, "\n");
				exit(1);
			}
			if (pgsz == (uint64_t)getpagesize())
				errx(1, "pagesize should be large");
			free(pagesizes);
			break;
		case '?':
		default:
			usage();
			return (2);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage();
		return (2);
	}

	ret = 0;
	for (i = 0; i < argc; i++) {
		ret1 = create_one_shm(argv[i], mode, idx);
		if (ret1 != 0 && ret == 0)
			ret = ret1;
	}
	return (ret);
}

static int
delete_one_shm(const char *path)
{
	int error, ret;

	error = shm_unlink(path);
	if (error != 0) {
		warn("unlink of %s failed", path);
		ret = 1;
	} else {
		ret = 0;
	}
	return (ret);
}

static int
delete_shm(int argc, char **argv)
{
	int i, ret, ret1;

	if (argc == 1) {
		usage();
		return (2);
	}

	ret = 0;
	for (i = 1; i < argc; i++) {
		ret1 = delete_one_shm(argv[i]);
		if (ret1 != 0 && ret == 0)
			ret = ret1;
	}
	return (ret);
}

static const char listmib[] = "kern.ipc.posix_shm_list";

static void
shm_decode_mode(mode_t m, char *str)
{
	int i;

	i = 0;
	str[i++] = (m & S_IRUSR) != 0 ? 'r' : '-';
	str[i++] = (m & S_IWUSR) != 0 ? 'w' : '-';
	str[i++] = (m & S_IXUSR) != 0 ? 'x' : '-';
	str[i++] = (m & S_IRGRP) != 0 ? 'r' : '-';
	str[i++] = (m & S_IWGRP) != 0 ? 'w' : '-';
	str[i++] = (m & S_IXGRP) != 0 ? 'x' : '-';
	str[i++] = (m & S_IROTH) != 0 ? 'r' : '-';
	str[i++] = (m & S_IWOTH) != 0 ? 'w' : '-';
	str[i++] = (m & S_IXOTH) != 0 ? 'x' : '-';
	str[i] = '\0';
}

static int
list_shm(int argc, char **argv)
{
	char *buf, *bp, *ep, jailpath[MAXPATHLEN], sizebuf[8], str[10];
	const char *jailparam;
	const struct kinfo_file *kif;
	struct stat st;
	int c, error, fd, jid, mib[3], ret;
	size_t len, jailpathlen, miblen;
	bool hsize, jailed, uname;

	hsize = false;
	jailed = false;
	uname = true;

	while ((c = getopt(argc, argv, "hj:n")) != -1) {
		switch (c) {
		case 'h':
			hsize = true;
			break;
		case 'n':
			uname = false;
			break;
		case 'j':
			jid = strtoul(optarg, &ep, 10);
			if (ep > optarg && !*ep) {
				jailparam = "jid";
				jailed = jid > 0;
			} else {
				jailparam = "name";
				jailed = true;
			}
			if (jailed) {
				if (jail_getv(0, jailparam, optarg, "path",
				    jailpath, NULL) < 0) {
					if (errno == ENOENT)
						warnx("no such jail: %s", optarg);
					else
						warnx("%s", jail_errmsg);
					return (1);
				}
				jailpathlen = strlen(jailpath);
				jailpath[jailpathlen] = '/';
			}
			break;
		default:
			usage();
			return (2);
		}
	}
	if (argc != optind) {
		usage();
		return (2);
	}

	miblen = nitems(mib);
	error = sysctlnametomib(listmib, mib, &miblen);
	if (error == -1) {
		warn("cannot translate %s", listmib);
		return (1);
	}
	len = 0;
	error = sysctl(mib, miblen, NULL, &len, NULL, 0);
	if (error == -1) {
		warn("cannot get %s length", listmib);
		return (1);
	}
	len = len * 4 / 3;
	buf = malloc(len);
	if (buf == NULL) {
		warn("malloc");
		return (1);
	}
	error = sysctl(mib, miblen, buf, &len, NULL, 0);
	if (error != 0) {
		warn("reading %s", listmib);
		ret = 1;
		goto out;
	}
	ret = 0;
	printf("MODE    \tOWNER\tGROUP\tSIZE\tPATH\n");
	for (bp = buf; bp < buf + len; bp += kif->kf_structsize) {
		kif = (const struct kinfo_file *)(void *)bp;
		if (kif->kf_structsize == 0)
			break;
		if (jailed && strncmp(kif->kf_path, jailpath, jailpathlen + 1))
			continue;
		fd = shm_open(kif->kf_path, O_RDONLY, 0);
		if (fd == -1) {
			if (errno != EACCES) {
				warn("open %s", kif->kf_path);
				ret = 1;
			}
			continue;
		}
		error = fstat(fd, &st);
		close(fd);
		if (error != 0) {
			warn("stat %s", kif->kf_path);
			ret = 1;
			continue;
		}
		shm_decode_mode(kif->kf_un.kf_file.kf_file_mode, str);
		printf("%s\t", str);
		if (uname) {
			printf("%s\t%s\t", user_from_uid(st.st_uid, 0),
			    group_from_gid(st.st_gid, 0));
		} else {
			printf("%d\t%d\t", st.st_uid, st.st_gid);
		}
		if (hsize) {
			humanize_number(sizebuf, sizeof(sizebuf),
			    kif->kf_un.kf_file.kf_file_size, "", HN_AUTOSCALE,
			    HN_NOSPACE);
			printf("%s\t", sizebuf);
		} else {
			printf("%jd\t",
			    (uintmax_t)kif->kf_un.kf_file.kf_file_size);
		}
		printf("%s\n", kif->kf_path);
	}
out:
	free(buf);
	return (ret);
}

static int
read_one_shm(const char *path)
{
	char buf[4096];
	ssize_t size, se;
	int fd, ret;

	ret = 1;
	fd = shm_open(path, O_RDONLY, 0);
	if (fd == -1) {
		warn("open %s", path);
		goto out;
	}
	for (;;) {
		size = read(fd, buf, sizeof(buf));
		if (size > 0) {
			se = fwrite(buf, 1, size, stdout);
			if (se < size) {
				warnx("short write to stdout");
				goto out;
			}
		}
		if (size == (ssize_t)sizeof(buf))
			continue;
		if (size >= 0 && size < (ssize_t)sizeof(buf)) {
			ret = 0;
			goto out;
		}
		warn("read from %s", path);
		goto out;
	}
out:
	close(fd);
	return (ret);
}

static int
read_shm(int argc, char **argv)
{
	int i, ret, ret1;

	if (argc == 1) {
		usage();
		return (2);
	}

	ret = 0;
	for (i = 1; i < argc; i++) {
		ret1 = read_one_shm(argv[i]);
		if (ret1 != 0 && ret == 0)
			ret = ret1;
	}
	return (ret);
}

static int
stat_one_shm(const char *path, bool hsize, bool uname)
{
	char sizebuf[8];
	struct stat st;
	int error, fd, ret;
	struct shm_largepage_conf conf_dummy;
	bool largepage;

	fd = shm_open(path, O_RDONLY, 0);
	if (fd == -1) {
		warn("open %s", path);
		return (1);
	}
	ret = 0;
	error = fstat(fd, &st);
	if (error == -1) {
		warn("stat %s", path);
		ret = 1;
	} else {
		printf("path\t%s\n", path);
		printf("inode\t%jd\n", (uintmax_t)st.st_ino);
		printf("mode\t%#o\n", st.st_mode);
		printf("nlink\t%jd\n", (uintmax_t)st.st_nlink);
		if (uname) {
			printf("owner\t%s\n", user_from_uid(st.st_uid, 0));
			printf("group\t%s\n", group_from_gid(st.st_gid, 0));
		} else {
			printf("uid\t%d\n", st.st_uid);
			printf("gid\t%d\n", st.st_gid);
		}
		if (hsize) {
			humanize_number(sizebuf, sizeof(sizebuf),
			    st.st_size, "", HN_AUTOSCALE, HN_NOSPACE);
			printf("size\t%s\n", sizebuf);
		} else {
			printf("size\t%jd\n", (uintmax_t)st.st_size);
		}
		printf("atime\t%ld.%09ld\n", (long)st.st_atime,
		    (long)st.st_atim.tv_nsec);
		printf("mtime\t%ld.%09ld\n", (long)st.st_mtime,
		    (long)st.st_mtim.tv_nsec);
		printf("ctime\t%ld.%09ld\n", (long)st.st_ctime,
		    (long)st.st_ctim.tv_nsec);
		printf("birth\t%ld.%09ld\n", (long)st.st_birthtim.tv_sec,
		    (long)st.st_birthtim.tv_nsec);
		error = ioctl(fd, FIOGSHMLPGCNF, &conf_dummy);
		largepage = error == 0;
		if (st.st_blocks != 0 && largepage)
			printf("pagesz\t%jd\n", roundup((uintmax_t)st.st_size,
			    PAGE_SIZE) / st.st_blocks);
		else
			printf("pages\t%jd\n", st.st_blocks);
	}
	close(fd);
	return (ret);
}

static int
stat_shm(int argc, char **argv)
{
	int c, i, ret, ret1;
	bool hsize, uname;

	hsize = false;
	uname = true;

	while ((c = getopt(argc, argv, "hn")) != -1) {
		switch (c) {
		case 'h':
			hsize = true;
			break;
		case 'n':
			uname = false;
			break;
		default:
			usage();
			return (2);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage();
		return (2);
	}

	ret = 0;
	for (i = 0; i < argc; i++) {
		ret1 = stat_one_shm(argv[i], hsize, uname);
		if (ret1 != 0 && ret == 0)
			ret = ret1;
	}
	return (ret);
}

static int
truncate_one_shm(const char *path, uint64_t newsize)
{
	int error, fd, ret;

	ret = 0;
	fd = shm_open(path, O_RDWR, 0);
	if (fd == -1) {
		warn("open %s", path);
		return (1);
	}
	error = ftruncate(fd, newsize);
	if (error == -1) {
		warn("truncate %s", path);
		ret = 1;
	}
	close(fd);
	return (ret);
}

static int
truncate_shm(int argc, char **argv)
{
	uint64_t newsize;
	int c, i, ret, ret1;

	newsize = 0;
	while ((c = getopt(argc, argv, "s:")) != -1) {
		switch (c) {
		case 's':
			if (expand_number(optarg, &newsize) == -1)
				err(1, "size");
			break;
		case '?':
		default:
			return (2);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage();
		return (2);
	}

	ret = 0;
	for (i = 0; i < argc; i++) {
		ret1 = truncate_one_shm(argv[i], newsize);
		if (ret1 != 0 && ret == 0)
			ret = ret1;
	}
	return (ret);
}

struct opmode {
	const char *cmd;
	int (*impl)(int argc, char **argv);
};

static const struct opmode opmodes[] = {
	{ .cmd = "create",	.impl = create_shm},
	{ .cmd = "rm",		.impl = delete_shm, },
	{ .cmd = "list",	.impl = list_shm },
	{ .cmd = "ls",		.impl = list_shm },
	{ .cmd = "dump",	.impl = read_shm, },
	{ .cmd = "stat",	.impl = stat_shm, },
	{ .cmd = "truncate",	.impl = truncate_shm, },
};

int
main(int argc, char *argv[])
{
	const struct opmode *opmode;
	int i, ret;

	ret = 0;
	opmode = NULL;

	if (argc < 2) {
		usage();
		exit(2);
	}
	for (i = 0; i < (int)nitems(opmodes); i++) {
		if (strcmp(argv[1], opmodes[i].cmd) == 0) {
			opmode = &opmodes[i];
			break;
		}
	}
	if (opmode == NULL) {
		usage();
		exit(2);
	}
	ret = opmode->impl(argc - 1, argv + 1);
	exit(ret);
}
