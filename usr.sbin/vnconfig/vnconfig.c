/*
 * Copyright (c) 1993 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *
 * from: Utah $Hdr: vnconfig.c 1.1 93/12/15$
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)vnconfig.c	8.1 (Berkeley) 12/15/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/linker.h>
#include <sys/mount.h>
#include <sys/module.h>
#include <sys/stat.h>
#include <sys/vnioctl.h>
#include <ufs/ufs/ufsmount.h>

#define MAXVNDISK	16
#define LINESIZE	1024
#define ZBUFSIZE	32768

struct vndisk {
	char	*dev;
	char	*file;
	char	*autolabel;
	int	flags;
	int	size;
	char	*oarg;
} vndisks[MAXVNDISK];

#define VN_CONFIG	0x01
#define VN_UNCONFIG	0x02
#define VN_ENABLE	0x04
#define VN_DISABLE	0x08
#define	VN_SWAP		0x10
#define VN_MOUNTRO	0x20
#define VN_MOUNTRW	0x40
#define VN_IGNORE	0x80
#define VN_SET		0x100
#define VN_RESET	0x200
#define VN_TRUNCATE	0x400
#define VN_ZERO		0x800

int nvndisks;

int all = 0;
int verbose = 0;
int global = 0;
u_long setopt = 0;
u_long resetopt = 0;
char *configfile;

int config __P((struct vndisk *));
void getoptions __P((struct vndisk *, char *));
char *rawdevice __P((char *));
void readconfig __P((int));
static void usage __P((void));
static int getsize(const char *arg);
static void do_autolabel(const char *dev, const char *label);
int what_opt __P((char *, u_long *));

int
main(argc, argv)
	char **argv;
{
	register int i, rv;
	int flags = 0;
	int size = 0;
	char *autolabel = NULL;
	char *s;

	configfile = _PATH_VNTAB;
	while ((i = getopt(argc, argv, "acdef:gr:s:S:TZL:uv")) != -1)
		switch (i) {

		/* all -- use config file */
		case 'a':
			all++;
			break;

		/* configure */
		case 'c':
			flags |= VN_CONFIG;
			flags &= ~VN_UNCONFIG;
			break;

		/* disable */
		case 'd':
			flags |= VN_DISABLE;
			flags &= ~VN_ENABLE;
			break;

		/* enable */
		case 'e':
			flags |= (VN_ENABLE|VN_CONFIG);
			flags &= ~(VN_DISABLE|VN_UNCONFIG);
			break;

		/* alternate config file */
		case 'f':
			configfile = optarg;
			break;

		/* fiddle global options */
		case 'g':
			global = 1 - global;
			break;

		/* reset options */
		case 'r':
			for (s = strtok(optarg, ","); s; s = strtok(NULL, ",")) {
				if (what_opt(s, &resetopt))
					errx(1, "invalid options '%s'", s);
			}
			flags |= VN_RESET;
			break;

		/* set options */
		case 's':
			for (s = strtok(optarg, ","); s; s = strtok(NULL, ",")) {
				if (what_opt(s, &setopt))
					errx(1, "invalid options '%s'", s);
			}
			flags |= VN_SET;
			break;

		/* unconfigure */
		case 'u':
			flags |= (VN_DISABLE|VN_UNCONFIG);
			flags &= ~(VN_ENABLE|VN_CONFIG);
			break;

		/* verbose */
		case 'v':
			verbose++;
			break;

		case 'S':
			size = getsize(optarg);
			break;

		case 'T':
			flags |= VN_TRUNCATE;
			break;

		case 'Z':
			flags |= VN_ZERO;
			break;

		case 'L':
			autolabel = optarg;
			break;

		default:
			usage();
		}

	if (modfind("vn") < 0)
		if (kldload("vn") < 0 || modfind("vn") < 0)
			warnx( "cannot find or load \"vn\" kernel module");

	if (flags == 0)
		flags = VN_CONFIG;
	if (all) {
		readconfig(flags);
	} else {
		if (argc < optind + 1)
			usage();
		vndisks[0].dev = argv[optind++];
		vndisks[0].file = argv[optind++];	/* may be NULL */
		vndisks[0].flags = flags;
		vndisks[0].size = size;
		vndisks[0].autolabel = autolabel;
		if (optind < argc)
			getoptions(&vndisks[0], argv[optind]);
		nvndisks = 1;
	}
	rv = 0;
	for (i = 0; i < nvndisks; i++)
		rv += config(&vndisks[i]);
	exit(rv);
}

int
what_opt(str,p)
	char *str;
	u_long *p;
{
	if (!strcmp(str,"reserve")) { *p |= VN_RESERVE; return 0; }
	if (!strcmp(str,"labels")) { *p |= VN_LABELS; return 0; }
	if (!strcmp(str,"follow")) { *p |= VN_FOLLOW; return 0; }
	if (!strcmp(str,"debug")) { *p |= VN_DEBUG; return 0; }
	if (!strcmp(str,"all")) { *p |= ~0; return 0; }
	if (!strcmp(str,"none")) { *p |= 0; return 0; }
	return 1;
}

int
config(vnp)
	struct vndisk *vnp;
{
	char *dev, *file, *oarg;
	int flags;
	struct vn_ioctl vnio;
	register int rv;
	char *rdev;
	FILE *f;
	u_long l;
	int pgsize = getpagesize();

	rv = 0;

	/*
	 * Prepend "/dev/" to the specified device name, if necessary.
	 * Operate on vnp->dev because it is used later.
	 */
	if (vnp->dev[0] != '/' && vnp->dev[0] != '.')
		(void)asprintf(&vnp->dev, "/dev/%s", vnp->dev);
	dev = vnp->dev;
	file = vnp->file;
	flags = vnp->flags;
	oarg = vnp->oarg;

	if (flags & VN_IGNORE)
		return(0);

	/*
	 * When a regular file has been specified, do any requested setup
	 * of the file.  Truncation (also creates the file if necessary),
	 * sizing, and zeroing.
	 */

	if (file && vnp->size != 0 && (flags & VN_CONFIG)) {
		int  fd;
		struct stat st;

		if (flags & VN_TRUNCATE)
			fd = open(file, O_RDWR|O_CREAT|O_TRUNC);
		else
			fd = open(file, O_RDWR);
		if (fd >= 0 && fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
			if (st.st_size < (off_t)vnp->size * pgsize)
				ftruncate(fd, (off_t)vnp->size * pgsize);
			if (vnp->size != 0)
				st.st_size = (off_t)vnp->size * pgsize;

			if (flags & VN_ZERO) {
				char *buf = malloc(ZBUFSIZE);
				bzero(buf, ZBUFSIZE);
				while (st.st_size > 0) {
					int n = (st.st_size > ZBUFSIZE) ?
					    ZBUFSIZE : (int)st.st_size;
					if (write(fd, buf, n) != n) {
						ftruncate(fd, 0);
						printf("Unable to ZERO file %s\n", file);
						return(0);
					}
					st.st_size -= (off_t)n;
				}
			}
			close(fd);
		} else {
			printf("Unable to open file %s\n", file);
			return(0);
		}
	}

	rdev = rawdevice(dev);
	f = fopen(rdev, "rw");
	if (f == NULL) {
		warn("%s", dev);
		return(1);
	}
	vnio.vn_file = file;
	vnio.vn_size = vnp->size;	/* non-zero only if swap backed */

	/*
	 * Disable the device
	 */
	if (flags & VN_DISABLE) {
		if (flags & (VN_MOUNTRO|VN_MOUNTRW)) {
			rv = unmount(oarg, 0);
			if (rv) {
				if (errno == EBUSY)
					flags &= ~VN_UNCONFIG;
				if ((flags & VN_UNCONFIG) == 0)
					warn("umount");
			} else if (verbose)
				printf("%s: unmounted\n", dev);
		}
	}
	/*
	 * Clear (un-configure) the device
	 */
	if (flags & VN_UNCONFIG) {
		rv = ioctl(fileno(f), VNIOCDETACH, &vnio);
		if (rv) {
			if (errno == ENODEV) {
				if (verbose)
					printf("%s: not configured\n", dev);
				rv = 0;
			} else
				warn("VNIOCDETACH");
		} else if (verbose)
			printf("%s: cleared\n", dev);
	}
	/*
	 * Set specified options
	 */
	if (flags & VN_SET) {
		l = setopt;
		if (global)
			rv = ioctl(fileno(f), VNIOCGSET, &l);
		else
			rv = ioctl(fileno(f), VNIOCUSET, &l);
		if (rv) {
			warn("VNIO[GU]SET");
		} else if (verbose)
			printf("%s: flags now=%08x\n",dev,l);
	}
	/*
	 * Reset specified options
	 */
	if (flags & VN_RESET) {
		l = resetopt;
		if (global)
			rv = ioctl(fileno(f), VNIOCGCLEAR, &l);
		else
			rv = ioctl(fileno(f), VNIOCUCLEAR, &l);
		if (rv) {
			warn("VNIO[GU]CLEAR");
		} else if (verbose)
			printf("%s: flags now=%08x\n",dev,l);
	}
	/*
	 * Configure the device
	 */
	if (flags & VN_CONFIG) {
		rv = ioctl(fileno(f), VNIOCATTACH, &vnio);
		if (rv) {
			warn("VNIOCATTACH");
			flags &= ~VN_ENABLE;
		} else {
			if (verbose) {
				printf(
				    "%s: %d bytes on %s\n",
				    dev, vnio.vn_size, file
				);
			}
			/*
			 * autolabel
			 */
			if (vnp->autolabel) {
				do_autolabel(vnp->dev, vnp->autolabel);
			}
		}
	}
	/*
	 * Set an option
	 */
	if (flags & VN_SET) {
		l = setopt;
		if (global)
			rv = ioctl(fileno(f), VNIOCGSET, &l);
		else
			rv = ioctl(fileno(f), VNIOCUSET, &l);
		if (rv) {
			warn("VNIO[GU]SET");
		} else if (verbose)
			printf("%s: flags now=%08lx\n",dev,l);
	}
	/*
	 * Reset an option
	 */
	if (flags & VN_RESET) {
		l = resetopt;
		if (global)
			rv = ioctl(fileno(f), VNIOCGCLEAR, &l);
		else
			rv = ioctl(fileno(f), VNIOCUCLEAR, &l);
		if (rv) {
			warn("VNIO[GU]CLEAR");
		} else if (verbose)
			printf("%s: flags now=%08lx\n",dev,l);
	}

	/*
	 * Enable special functions on the device
	 */
	if (flags & VN_ENABLE) {
		if (flags & VN_SWAP) {
			rv = swapon(dev);
			if (rv)
				warn("swapon");
			else if (verbose)
				printf("%s: swapping enabled\n", dev);
		}
		if (flags & (VN_MOUNTRO|VN_MOUNTRW)) {
			struct ufs_args args;
			int mflags;

			args.fspec = dev;
			mflags = (flags & VN_MOUNTRO) ? MNT_RDONLY : 0;
			rv = mount("ufs", oarg, mflags, &args);
			if (rv)
				warn("mount");
			else if (verbose)
				printf("%s: mounted on %s\n", dev, oarg);
		}
	}
/* done: */
	fclose(f);
	fflush(stdout);
	return(rv < 0);
}

#define EOL(c)		((c) == '\0' || (c) == '\n')
#define WHITE(c)	((c) == ' ' || (c) == '\t' || (c) == '\n')

void
readconfig(flags)
	int flags;
{
	char buf[LINESIZE];
	FILE *f;
	register char *cp, *sp;
	register int ix;

	f = fopen(configfile, "r");
	if (f == NULL)
		err(1, "%s", configfile);
	ix = 0;
	while (fgets(buf, LINESIZE, f) != NULL) {
		cp = buf;
		if (*cp == '#')
			continue;
		while (!EOL(*cp) && WHITE(*cp))
			cp++;
		if (EOL(*cp))
			continue;
		sp = cp;
		while (!EOL(*cp) && !WHITE(*cp))
			cp++;
		if (EOL(*cp))
			continue;
		*cp++ = '\0';
		vndisks[ix].dev = malloc(cp - sp);
		strcpy(vndisks[ix].dev, sp);
		while (!EOL(*cp) && WHITE(*cp))
			cp++;
		if (EOL(*cp))
			continue;
		sp = cp;
		while (!EOL(*cp) && !WHITE(*cp))
			cp++;
		*cp++ = '\0';

		if (*sp == '%' && strtol(sp + 1, NULL, 0) > 0) {
			vndisks[ix].size = getsize(sp + 1);
		} else {
			vndisks[ix].file = malloc(cp - sp);
			strcpy(vndisks[ix].file, sp);
		}

		while (!EOL(*cp) && WHITE(*cp))
			cp++;
		vndisks[ix].flags = flags;
		if (!EOL(*cp)) {
			sp = cp;
			while (!EOL(*cp) && !WHITE(*cp))
				cp++;
			*cp++ = '\0';
			getoptions(&vndisks[ix], sp);
		}
		nvndisks++;
		ix++;
	}
}

void
getoptions(vnp, fstr)
	struct vndisk *vnp;
	char *fstr;
{
	int flags = 0;
	char *oarg = NULL;

	if (strcmp(fstr, "swap") == 0)
		flags |= VN_SWAP;
	else if (strncmp(fstr, "mount=", 6) == 0) {
		flags |= VN_MOUNTRW;
		oarg = &fstr[6];
	} else if (strncmp(fstr, "mountrw=", 8) == 0) {
		flags |= VN_MOUNTRW;
		oarg = &fstr[8];
	} else if (strncmp(fstr, "mountro=", 8) == 0) {
		flags |= VN_MOUNTRO;
		oarg = &fstr[8];
	} else if (strcmp(fstr, "ignore") == 0)
		flags |= VN_IGNORE;
	vnp->flags |= flags;
	if (oarg) {
		vnp->oarg = malloc(strlen(oarg) + 1);
		strcpy(vnp->oarg, oarg);
	} else
		vnp->oarg = NULL;
}

char *
rawdevice(dev)
	char *dev;
{
	register char *rawbuf, *dp, *ep;
	struct stat sb;
	int len;

	len = strlen(dev);
	rawbuf = malloc(len + 2);
	strcpy(rawbuf, dev);
	if (stat(rawbuf, &sb) != 0 || !S_ISCHR(sb.st_mode)) {
		dp = rindex(rawbuf, '/');
		if (dp) {
			for (ep = &rawbuf[len]; ep > dp; --ep)
				*(ep+1) = *ep;
			*++ep = 'r';
		}
	}
	return (rawbuf);
}

static void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n",
		"usage: vnconfig [-cdeguv] [-s option] [-r option] [-S value] special_file",
		"                [regular_file] [feature]",
		"       vnconfig -a [-cdeguv] [-s option] [-r option] [-f config_file]");
	exit(1);
}

static int
getsize(const char *arg)
{
	char *ptr;
	int pgsize = getpagesize();
	quad_t size = strtoq(arg, &ptr, 0);

	switch(tolower(*ptr)) {
	case 't':
		/*
		 * GULP!  Terrabytes.  It's actually possible to create 
		 * a 7.9 TB VN device, though newfs can't handle any single
		 * filesystem larger then 1 TB.
		 */
		size *= 1024;
		/* fall through */
	case 'g':
		size *= 1024;
		/* fall through */
	default:
	case 'm':
		size *= 1024;
		/* fall through */
	case 'k':
		size *= 1024;
		/* fall through */
	case 'c':
		break;
	}
	size = (size + pgsize - 1) / pgsize;
	return((int)size);
}

/*
 * DO_AUTOLABEL
 *
 *	Automatically label the device.  This will wipe any preexisting
 *	label.
 */

static void
do_autolabel(const char *dev, const char *label)
{
	/* XXX not yet implemented */
	fprintf(stderr, "autolabel not yet implemented, sorry\n");
	exit(1);
}

