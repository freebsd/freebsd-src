/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)swapon.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mdioctl.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <vm/vm_param.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <fstab.h>
#include <libgen.h>
#include <libutil.h>
#include <limits.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void);
static const char *swap_on_off(char *, int, char *);
static const char *swap_on_off_gbde(char *, int);
static const char *swap_on_off_geli(char *, char *, int);
static const char *swap_on_off_md(char *, char *, int);
static const char *swap_on_off_sfile(char *, int);
static void swaplist(int, int, int);
static int run_cmd(int *, const char *, ...) __printflike(2, 3);

static enum { SWAPON, SWAPOFF, SWAPCTL } orig_prog, which_prog = SWAPCTL;

static int qflag;

int
main(int argc, char **argv)
{
	struct fstab *fsp;
	const char *swfile;
	char *ptr;
	int ret;
	int ch, doall;
	int sflag = 0, lflag = 0, late = 0, hflag = 0;
	const char *etc_fstab;

	if ((ptr = strrchr(argv[0], '/')) == NULL)
		ptr = argv[0];
	if (strstr(ptr, "swapon"))
		which_prog = SWAPON;
	else if (strstr(ptr, "swapoff"))
		which_prog = SWAPOFF;
	orig_prog = which_prog;
	
	doall = 0;
	etc_fstab = NULL;
	while ((ch = getopt(argc, argv, "AadghklLmqsUF:")) != -1) {
		switch(ch) {
		case 'A':
			if (which_prog == SWAPCTL) {
				doall = 1;
				which_prog = SWAPON;
			} else {
				usage();
			}
			break;
		case 'a':
			if (which_prog == SWAPON || which_prog == SWAPOFF)
				doall = 1;
			else
				which_prog = SWAPON;
			break;
		case 'd':
			if (which_prog == SWAPCTL)
				which_prog = SWAPOFF;
			else
				usage();
			break;
		case 'g':
			hflag = 'G';
			break;
		case 'h':
			hflag = 'H';
			break;
		case 'k':
			hflag = 'K';
			break;
		case 'l':
			lflag = 1;
			break;
		case 'L':
			late = 1;
			break;
		case 'm':
			hflag = 'M';
			break;
		case 'q':
			if (which_prog == SWAPON || which_prog == SWAPOFF)
				qflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'U':
			if (which_prog == SWAPCTL) {
				doall = 1;
				which_prog = SWAPOFF;
			} else {
				usage();
			}
			break;
		case 'F':
			etc_fstab = optarg;
			break;
		case '?':
		default:
			usage();
		}
	}
	argv += optind;

	ret = 0;
	swfile = NULL;
	if (etc_fstab != NULL)
		setfstab(etc_fstab);
	if (which_prog == SWAPON || which_prog == SWAPOFF) {
		if (doall) {
			while ((fsp = getfsent()) != NULL) {
				if (strcmp(fsp->fs_type, FSTAB_SW))
					continue;
				if (strstr(fsp->fs_mntops, "noauto"))
					continue;
				if (which_prog != SWAPOFF &&
				    strstr(fsp->fs_mntops, "late") &&
				    !late)
					continue;
				swfile = swap_on_off(fsp->fs_spec, 1,
				    fsp->fs_mntops);
				if (swfile == NULL) {
					ret = 1;
					continue;
				}
				if (!qflag) {
					printf("%s: %sing %s as swap device\n",
					    getprogname(),
					    (which_prog == SWAPOFF) ?
					    "remov" : "add", swfile);
				}
			}
		}
		else if (!*argv)
			usage();
		for (; *argv; ++argv) {
			swfile = swap_on_off(*argv, 0, NULL);
			if (swfile == NULL) {
				ret = 1;
				continue;
			}
			if (orig_prog == SWAPCTL) {
				printf("%s: %sing %s as swap device\n",
				    getprogname(),
				    (which_prog == SWAPOFF) ? "remov" : "add",
				    swfile);
			}
		}
	} else {
		if (lflag || sflag)
			swaplist(lflag, sflag, hflag);
		else 
			usage();
	}
	exit(ret);
}

static const char *
swap_on_off(char *name, int doingall, char *mntops)
{
	char base[PATH_MAX];

	/* Swap on vnode-backed md(4) device. */
	if (mntops != NULL &&
	    (fnmatch(_PATH_DEV MD_NAME "[0-9]*", name, 0) != FNM_NOMATCH ||
	     fnmatch(MD_NAME "[0-9]*", name, 0) != FNM_NOMATCH ||
	     strncmp(_PATH_DEV MD_NAME, name,
		sizeof(_PATH_DEV) + sizeof(MD_NAME)) == 0 ||
	     strncmp(MD_NAME, name, sizeof(MD_NAME)) == 0))
		return (swap_on_off_md(name, mntops, doingall));

	/* Swap on encrypted device by GEOM_BDE. */
	basename_r(name, base);
	if (fnmatch("*.bde", base, 0) != FNM_NOMATCH)
		return (swap_on_off_gbde(name, doingall));

	/* Swap on encrypted device by GEOM_ELI. */
	if (fnmatch("*.eli", base, 0) != FNM_NOMATCH)
		return (swap_on_off_geli(name, mntops, doingall));

	/* Swap on special file. */
	return (swap_on_off_sfile(name, doingall));
}

static const char *
swap_on_off_gbde(char *name, int doingall)
{
	const char *ret;
	char pass[64 * 2 + 1], bpass[64];
	char *devname, *p;
	int i, fd, error;

	devname = strdup(name);
	p = strrchr(devname, '.');
	if (p == NULL) {
		warnx("%s: Malformed device name", name);
		return (NULL);
	}
	*p = '\0';

	fd = -1;
	switch (which_prog) {
	case SWAPON:
		arc4random_buf(bpass, sizeof(bpass));
		for (i = 0; i < (int)sizeof(bpass); i++)
			sprintf(&pass[2 * i], "%02x", bpass[i]);
		pass[sizeof(pass) - 1] = '\0';

		error = run_cmd(&fd, "%s init %s -P %s", _PATH_GBDE,
		    devname, pass);
		if (error) {
			/* bde device found.  Ignore it. */
			close(fd);
			if (!qflag)
				warnx("%s: Device already in use", name);
			return (NULL);
		}
		close(fd);
		error = run_cmd(&fd, "%s attach %s -p %s", _PATH_GBDE,
		    devname, pass);
		if (error) {
			close(fd);
			warnx("gbde (attach) error: %s", name);
			return (NULL);
		}
		break;
	case SWAPOFF:
		break;
	default:
		return (NULL);
		break;
	}
	if (fd != -1)
		close(fd);
	ret = swap_on_off_sfile(name, doingall);

	fd = -1;
	switch (which_prog) {
	case SWAPOFF:
		error = run_cmd(&fd, "%s detach %s", _PATH_GBDE, devname);
		if (error) {
			/* bde device not found.  Ignore it. */
			if (!qflag)
				warnx("%s: Device not found", devname);
			return (NULL);
		}
		break;
	default:
		return (NULL);
		break;
	}

	if (fd != -1)
		close(fd);
	return (ret);
}

static const char *
swap_on_off_geli(char *name, char *mntops, int doingall)
{
	const char *ops, *aalgo, *ealgo, *keylen_str, *sectorsize_str;
	char *devname, *p;
	char args[4096];
	struct stat sb;
	int fd, error, keylen, sectorsize;
	u_long ul;

	devname = strdup(name);
	p = strrchr(devname, '.');
	if (p == NULL) {
		warnx("%s: Malformed device name", name);
		return (NULL);
	}
	*p = '\0';

	ops = strdup(mntops);

	/* Default parameters for geli(8). */
	aalgo = "hmac/sha256";
	ealgo = "aes";
	keylen = 256;
	sectorsize = 4096;

	if ((p = strstr(ops, "aalgo=")) != NULL) {
		aalgo = p + sizeof("aalgo=") - 1;
		p = strchr(aalgo, ',');
		if (p != NULL)
			*p = '\0';
	}
	if ((p = strstr(ops, "ealgo=")) != NULL) {
		ealgo = p + sizeof("ealgo=") - 1;
		p = strchr(ealgo, ',');
		if (p != NULL)
			*p = '\0';
	}
	if ((p = strstr(ops, "keylen=")) != NULL) {
		keylen_str = p + sizeof("keylen=") - 1;
		p = strchr(keylen_str, ',');
		if (p != NULL)
			*p = '\0';
		errno = 0;
		ul = strtoul(keylen_str, &p, 10);
		if (errno == 0) {
			if (*p != '\0' || ul > INT_MAX)
				errno = EINVAL;
		}
		if (errno) {
			warn("Invalid keylen: %s", keylen_str);
			return (NULL);
		}
		keylen = (int)ul;
	}
	if ((p = strstr(ops, "sectorsize=")) != NULL) {
		sectorsize_str = p + sizeof("sectorsize=") - 1;
		p = strchr(sectorsize_str, ',');
		if (p != NULL)
			*p = '\0';
		errno = 0;
		ul = strtoul(sectorsize_str, &p, 10);
		if (errno == 0) {
			if (*p != '\0' || ul > INT_MAX)
				errno = EINVAL;
		}
		if (errno) {
			warn("Invalid sectorsize: %s", sectorsize_str);
			return (NULL);
		}
		sectorsize = (int)ul;
	}
	snprintf(args, sizeof(args), "-a %s -e %s -l %d -s %d -d",
	    aalgo, ealgo, keylen, sectorsize);
	args[sizeof(args) - 1] = '\0';
	free((void *)ops);

	fd = -1;
	switch (which_prog) {
	case SWAPON:
		error = run_cmd(&fd, "%s onetime %s %s", _PATH_GELI, args,
		    devname);
		if (error) {
			/* eli device found.  Ignore it. */
			close(fd);
			if (!qflag)
				warnx("%s: Device already in use "
				    "or invalid parameters", name);
			return (NULL);
		}
		break;
	case SWAPOFF:
		if (stat(name, &sb) == -1 && errno == ENOENT) {
			if (!qflag)
				warnx("%s: Device not found", name);
			return (NULL);
		}
		break;
	default:
		return (NULL);
		break;
	}
	if (fd != -1)
		close(fd);

	return (swap_on_off_sfile(name, doingall));
}

static const char *
swap_on_off_md(char *name, char *mntops, int doingall)
{
	FILE *sfd;
	int fd, mdunit, error;
	const char *ret;
	char mdpath[PATH_MAX], linebuf[PATH_MAX];
	char *p, *vnodefile;
	size_t linelen;
	u_long ul;

	fd = -1;
	sfd = NULL;
	if (strlen(name) == (sizeof(MD_NAME) - 1))
		mdunit = -1;
	else {
		errno = 0;
		ul = strtoul(name + 2, &p, 10);
		if (errno == 0) {
			if (*p != '\0' || ul > INT_MAX)
				errno = EINVAL;
		}
		if (errno) {
			warn("Bad device unit: %s", name);
			return (NULL);
		}
		mdunit = (int)ul;
	}

	vnodefile = NULL;
	if ((p = strstr(mntops, "file=")) != NULL) {
		vnodefile = strdup(p + sizeof("file=") - 1);
		p = strchr(vnodefile, ',');
		if (p != NULL)
			*p = '\0';
	}
	if (vnodefile == NULL) {
		warnx("file option not found for %s", name);
		return (NULL);
	}

	switch (which_prog) {
	case SWAPON:
		if (mdunit == -1) {
			error = run_cmd(&fd, "%s -l -n -f %s",
			    _PATH_MDCONFIG, vnodefile);
			if (error == 0) {
				/* md device found.  Ignore it. */
				close(fd);
				if (!qflag)
					warnx("%s: Device already in use",
					    vnodefile);
				return (NULL);
			}
			error = run_cmd(&fd, "%s -a -t vnode -n -f %s",
			    _PATH_MDCONFIG, vnodefile);
			if (error) {
				warnx("mdconfig (attach) error: file=%s",
				    vnodefile);
				return (NULL);
			}
			sfd = fdopen(fd, "r");
			if (sfd == NULL) {
				warn("mdconfig (attach) fdopen error");
				ret = NULL;
				goto err;
			}
			p = fgetln(sfd, &linelen);
			if (p == NULL &&
			    (linelen < 2 || linelen > sizeof(linebuf))) {
				warn("mdconfig (attach) unexpected output");
				ret = NULL;
				goto err;
			}
			strncpy(linebuf, p, linelen);
			linebuf[linelen - 1] = '\0';
			errno = 0;
			ul = strtoul(linebuf, &p, 10);
			if (errno == 0) {
				if (*p != '\0' || ul > INT_MAX)
					errno = EINVAL;
			}
			if (errno) {
				warn("mdconfig (attach) unexpected output: %s",
				    linebuf);
				ret = NULL;
				goto err;
			}
			mdunit = (int)ul;
		} else {
			error = run_cmd(&fd, "%s -l -n -f %s -u %d",
			    _PATH_MDCONFIG, vnodefile, mdunit);
			if (error == 0) {
				/* md device found.  Ignore it. */
				close(fd);
				if (!qflag)
					warnx("md%d on %s: Device already "
					    "in use", mdunit, vnodefile);
				return (NULL);
			}
			error = run_cmd(NULL, "%s -a -t vnode -u %d -f %s",
			    _PATH_MDCONFIG, mdunit, vnodefile);
			if (error) {
				warnx("mdconfig (attach) error: "
				    "md%d on file=%s", mdunit, vnodefile);
				return (NULL);
			}
		}
		break;
	case SWAPOFF:
		if (mdunit == -1) {
			error = run_cmd(&fd, "%s -l -n -f %s",
			    _PATH_MDCONFIG, vnodefile);
			if (error) {
				/* md device not found.  Ignore it. */
				close(fd);
				if (!qflag)
					warnx("md on %s: Device not found",
					    vnodefile);
				return (NULL);
			}
			sfd = fdopen(fd, "r");
			if (sfd == NULL) {
				warn("mdconfig (list) fdopen error");
				ret = NULL;
				goto err;
			}
			p = fgetln(sfd, &linelen);
			if (p == NULL &&
			    (linelen < 2 || linelen > sizeof(linebuf) - 1)) {
				warn("mdconfig (list) unexpected output");
				ret = NULL;
				goto err;
			}
			strncpy(linebuf, p, linelen);
			linebuf[linelen - 1] = '\0';
			p = strchr(linebuf, ' ');
			if (p != NULL)
				*p = '\0';
			errno = 0;
			ul = strtoul(linebuf, &p, 10);
			if (errno == 0) {
				if (*p != '\0' || ul > INT_MAX)
					errno = EINVAL;
			}
			if (errno) {
				warn("mdconfig (list) unexpected output: %s",
				    linebuf);
				ret = NULL;
				goto err;
			}
			mdunit = (int)ul;
		} else {
			error = run_cmd(&fd, "%s -l -n -f %s -u %d",
			    _PATH_MDCONFIG, vnodefile, mdunit);
			if (error) {
				/* md device not found.  Ignore it. */
				close(fd);
				if (!qflag)
					warnx("md%d on %s: Device not found",
					    mdunit, vnodefile);
				return (NULL);
			}
		}
		break;
	default:
		return (NULL);
	}
	snprintf(mdpath, sizeof(mdpath), "%s%s%d", _PATH_DEV,
	    MD_NAME, mdunit);
	mdpath[sizeof(mdpath) - 1] = '\0';
	ret = swap_on_off_sfile(mdpath, doingall);

	switch (which_prog) {
	case SWAPOFF:
		if (ret != NULL) {
			error = run_cmd(NULL, "%s -d -u %d",
			    _PATH_MDCONFIG, mdunit);
			if (error)
				warn("mdconfig (detach) detach failed: %s%s%d",
				    _PATH_DEV, MD_NAME, mdunit);
		}
		break;
	default:
		break;
	}
err:
	if (sfd != NULL)
		fclose(sfd);
	if (fd != -1)
		close(fd);
	return (ret);
}

static int
run_cmd(int *ofd, const char *cmdline, ...)
{
	va_list ap;
	char **argv, **argvp, *cmd, *p;
	int argc, pid, status, rv;
	int pfd[2], nfd, dup2dn;

	va_start(ap, cmdline);
	rv = vasprintf(&cmd, cmdline, ap);
	if (rv == -1) {
		warn("%s", __func__);
		return (rv);
	}
	va_end(ap);

	for (argc = 1, p = cmd; (p = strchr(p, ' ')) != NULL; p++)
		argc++;
	argv = (char **)malloc(sizeof(*argv) * (argc + 1));
	for (p = cmd, argvp = argv; (*argvp = strsep(&p, " ")) != NULL;)
		if (**argvp != '\0' && (++argvp > &argv[argc])) {
			*argvp = NULL;
			break;
		}
	/* The argv array ends up NULL-terminated here. */
#if 0
	{
		int i;

		fprintf(stderr, "DEBUG: running:");
		/* Should be equivalent to 'cmd' (before strsep, of course). */
		for (i = 0; argv[i] != NULL; i++)
			fprintf(stderr, " %s", argv[i]);
		fprintf(stderr, "\n");
	}
#endif
	dup2dn = 1;
	if (ofd != NULL) {
		if (pipe(&pfd[0]) == -1) {
			warn("%s: pipe", __func__);
			return (-1);
		}
		*ofd = pfd[0];
		dup2dn = 0;
	}
	pid = fork();
	switch (pid) {
	case 0:
		/* Child process. */
		if (ofd != NULL)
			if (dup2(pfd[1], STDOUT_FILENO) < 0)
				err(1, "dup2 in %s", __func__);
		nfd = open(_PATH_DEVNULL, O_RDWR);
		if (nfd == -1)
			err(1, "%s: open %s", __func__, _PATH_DEVNULL);
		if (dup2(nfd, STDIN_FILENO) < 0)
			err(1, "%s: dup2", __func__);
		if (dup2dn && dup2(nfd, STDOUT_FILENO) < 0)
			err(1, "%s: dup2", __func__);
		if (dup2(nfd, STDERR_FILENO) < 0)
			err(1, "%s: dup2", __func__);
		execv(argv[0], argv);
		warn("exec: %s", argv[0]);
		_exit(-1);
	case -1:
		err(1, "%s: fork", __func__);
	}
	free(cmd);
	free(argv);
	while (waitpid(pid, &status, 0) != pid)
		;
	return (WEXITSTATUS(status));
}

static const char *
swap_on_off_sfile(char *name, int doingall)
{
	int error;

	switch (which_prog) {
	case SWAPON:
		error = swapon(name);
		break;
	case SWAPOFF:
		error = swapoff(name);
		break;
	default:
		error = 0;
		break;
	}
	if (error == -1) {
		switch (errno) {
		case EBUSY:
			if (!doingall)
				warnx("%s: Device already in use", name);
			break;
		case EINVAL:
			if (which_prog == SWAPON)
				warnx("%s: NSWAPDEV limit reached", name);
			else if (!doingall)
				warn("%s", name);
			break;
		default:
			warn("%s", name);
			break;
		}
		return (NULL);
	}
	return (name);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s ", getprogname());
	switch(orig_prog) {
	case SWAPON:
	case SWAPOFF:
	    fprintf(stderr, "[-F fstab] -aLq | file ...\n");
	    break;
	case SWAPCTL:
	    fprintf(stderr, "[-AghklmsU] [-a file ... | -d file ...]\n");
	    break;
	}
	exit(1);
}

static void
sizetobuf(char *buf, size_t bufsize, int hflag, long long val, int hlen,
    long blocksize)
{

	if (hflag == 'H') {
		char tmp[16];

		humanize_number(tmp, 5, (int64_t)val, "", HN_AUTOSCALE,
		    HN_B | HN_NOSPACE | HN_DECIMAL);
		snprintf(buf, bufsize, "%*s", hlen, tmp);
	} else {
		snprintf(buf, bufsize, "%*lld", hlen, val / blocksize);
	}
}

static void
swaplist(int lflag, int sflag, int hflag)
{
	size_t mibsize, size;
	struct xswdev xsw;
	int hlen, mib[16], n, pagesize;
	long blocksize;
	long long total = 0;
	long long used = 0;
	long long tmp_total;
	long long tmp_used;
	char buf[32];
	
	pagesize = getpagesize();
	switch(hflag) {
	case 'G':
	    blocksize = 1024 * 1024 * 1024;
	    strlcpy(buf, "1GB-blocks", sizeof(buf));
	    hlen = 10;
	    break;
	case 'H':
	    blocksize = -1;
	    strlcpy(buf, "Bytes", sizeof(buf));
	    hlen = 10;
	    break;
	case 'K':
	    blocksize = 1024;
	    strlcpy(buf, "1kB-blocks", sizeof(buf));
	    hlen = 10;
	    break;
	case 'M':
	    blocksize = 1024 * 1024;
	    strlcpy(buf, "1MB-blocks", sizeof(buf));
	    hlen = 10;
	    break;
	default:
	    getbsize(&hlen, &blocksize);
	    snprintf(buf, sizeof(buf), "%ld-blocks", blocksize);
	    break;
	}
	
	mibsize = sizeof mib / sizeof mib[0];
	if (sysctlnametomib("vm.swap_info", mib, &mibsize) == -1)
		err(1, "sysctlnametomib()");
	
	if (lflag) {
		printf("%-13s %*s %*s\n",
		    "Device:", 
		    hlen, buf,
		    hlen, "Used:");
	}
	
	for (n = 0; ; ++n) {
		mib[mibsize] = n;
		size = sizeof xsw;
		if (sysctl(mib, mibsize + 1, &xsw, &size, NULL, 0) == -1)
			break;
		if (xsw.xsw_version != XSWDEV_VERSION)
			errx(1, "xswdev version mismatch");
		
		tmp_total = (long long)xsw.xsw_nblks * pagesize;
		tmp_used  = (long long)xsw.xsw_used * pagesize;
		total += tmp_total;
		used  += tmp_used;
		if (lflag) {
			sizetobuf(buf, sizeof(buf), hflag, tmp_total, hlen,
			    blocksize);
			printf("/dev/%-8s %s ", devname(xsw.xsw_dev, S_IFCHR),
			    buf);
			sizetobuf(buf, sizeof(buf), hflag, tmp_used, hlen,
			    blocksize);
			printf("%s\n", buf);
		}
	}
	if (errno != ENOENT)
		err(1, "sysctl()");
	
	if (sflag) {
		sizetobuf(buf, sizeof(buf), hflag, total, hlen, blocksize);
		printf("Total:        %s ", buf);
		sizetobuf(buf, sizeof(buf), hflag, used, hlen, blocksize);
		printf("%s\n", buf);
	}
}

