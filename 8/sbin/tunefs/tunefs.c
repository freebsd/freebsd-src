/*
 * Copyright (c) 1983, 1993
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
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)tunefs.c	8.2 (Berkeley) 4/19/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * tunefs: change layout parameters to an existing file system.
 */
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/disklabel.h>
#include <sys/stat.h>

#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <fstab.h>
#include <libufs.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* the optimization warning string template */
#define	OPTWARN	"should optimize for %s with minfree %s %d%%"

struct uufsd disk;
#define	sblock disk.d_fs

void usage(void);
void printfs(void);

int
main(int argc, char *argv[])
{
	char *avalue, *Jvalue, *Lvalue, *lvalue, *Nvalue, *nvalue;
	char *tvalue;
	const char *special, *on;
	const char *name;
	int active;
	int Aflag, aflag, eflag, evalue, fflag, fvalue, Jflag, Lflag, lflag;
	int mflag, mvalue, Nflag, nflag, oflag, ovalue, pflag, sflag, svalue;
	int tflag;
	int ch, found_arg, i;
	const char *chg[2];
	struct ufs_args args;
	struct statfs stfs;

	if (argc < 3)
		usage();
	Aflag = aflag = eflag = fflag = Jflag = Lflag = lflag = mflag = 0;
	Nflag = nflag = oflag = pflag = sflag = tflag = 0;
	avalue = Jvalue = Lvalue = lvalue = Nvalue = nvalue = NULL;
	evalue = fvalue = mvalue = ovalue = svalue = 0;
	active = 0;
	found_arg = 0;		/* At least one arg is required. */
	while ((ch = getopt(argc, argv, "Aa:e:f:J:L:l:m:N:n:o:ps:t:")) != -1)
		switch (ch) {

		case 'A':
			found_arg = 1;
			Aflag++;
			break;

		case 'a':
			found_arg = 1;
			name = "POSIX.1e ACLs";
			avalue = optarg;
			if (strcmp(avalue, "enable") &&
			    strcmp(avalue, "disable")) {
				errx(10, "bad %s (options are %s)",
				    name, "`enable' or `disable'");
			}
			aflag = 1;
			break;

		case 'e':
			found_arg = 1;
			name = "maximum blocks per file in a cylinder group";
			evalue = atoi(optarg);
			if (evalue < 1)
				errx(10, "%s must be >= 1 (was %s)",
				    name, optarg);
			eflag = 1;
			break;

		case 'f':
			found_arg = 1;
			name = "average file size";
			fvalue = atoi(optarg);
			if (fvalue < 1)
				errx(10, "%s must be >= 1 (was %s)",
				    name, optarg);
			fflag = 1;
			break;

		case 'J':
			found_arg = 1;
			name = "gjournaled file system";
			Jvalue = optarg;
			if (strcmp(Jvalue, "enable") &&
			    strcmp(Jvalue, "disable")) {
				errx(10, "bad %s (options are %s)",
				    name, "`enable' or `disable'");
			}
			Jflag = 1;
			break;


		case 'L':
			found_arg = 1;
			name = "volume label";
			Lvalue = optarg;
			i = -1;
			while (isalnum(Lvalue[++i]));
			if (Lvalue[i] != '\0') {
				errx(10,
				"bad %s. Valid characters are alphanumerics.",
				    name);
			}
			if (strlen(Lvalue) >= MAXVOLLEN) {
				errx(10, "bad %s. Length is longer than %d.",
				    name, MAXVOLLEN - 1);
			}
			Lflag = 1;
			break;

		case 'l':
			found_arg = 1;
			name = "multilabel MAC file system";
			lvalue = optarg;
			if (strcmp(lvalue, "enable") &&
			    strcmp(lvalue, "disable")) {
				errx(10, "bad %s (options are %s)",
				    name, "`enable' or `disable'");
			}
			lflag = 1;
			break;

		case 'm':
			found_arg = 1;
			name = "minimum percentage of free space";
			mvalue = atoi(optarg);
			if (mvalue < 0 || mvalue > 99)
				errx(10, "bad %s (%s)", name, optarg);
			mflag = 1;
			break;

		case 'N':
			found_arg = 1;
			name = "NFSv4 ACLs";
			Nvalue = optarg;
			if (strcmp(Nvalue, "enable") &&
			    strcmp(Nvalue, "disable")) {
				errx(10, "bad %s (options are %s)",
				    name, "`enable' or `disable'");
			}
			Nflag = 1;
			break;

		case 'n':
			found_arg = 1;
			name = "soft updates";
			nvalue = optarg;
			if (strcmp(nvalue, "enable") != 0 &&
			    strcmp(nvalue, "disable") != 0) {
				errx(10, "bad %s (options are %s)",
				    name, "`enable' or `disable'");
			}
			nflag = 1;
			break;

		case 'o':
			found_arg = 1;
			name = "optimization preference";
			if (strcmp(optarg, "space") == 0)
				ovalue = FS_OPTSPACE;
			else if (strcmp(optarg, "time") == 0)
				ovalue = FS_OPTTIME;
			else
				errx(10,
				    "bad %s (options are `space' or `time')",
				    name);
			oflag = 1;
			break;

		case 'p':
			found_arg = 1;
			pflag = 1;
			break;

		case 's':
			found_arg = 1;
			name = "expected number of files per directory";
			svalue = atoi(optarg);
			if (svalue < 1)
				errx(10, "%s must be >= 1 (was %s)",
				    name, optarg);
			sflag = 1;
			break;

		case 't':
			found_arg = 1;
			name = "trim";
			tvalue = optarg;
			if (strcmp(tvalue, "enable") != 0 &&
			    strcmp(tvalue, "disable") != 0) {
				errx(10, "bad %s (options are %s)",
				    name, "`enable' or `disable'");
			}
			tflag = 1;
			break;

		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (found_arg == 0 || argc != 1)
		usage();

	on = special = argv[0];
	if (ufs_disk_fillout(&disk, special) == -1)
		goto err;
	if (disk.d_name != special) {
		special = disk.d_name;
		if (statfs(special, &stfs) == 0 &&
		    strcmp(special, stfs.f_mntonname) == 0)
			active = 1;
	}

	if (pflag) {
		printfs();
		exit(0);
	}
	if (Lflag) {
		name = "volume label";
		strlcpy(sblock.fs_volname, Lvalue, MAXVOLLEN);
	}
	if (aflag) {
		name = "POSIX.1e ACLs";
		if (strcmp(avalue, "enable") == 0) {
			if (sblock.fs_flags & FS_ACLS) {
				warnx("%s remains unchanged as enabled", name);
			} else if (sblock.fs_flags & FS_NFS4ACLS) {
				warnx("%s and NFSv4 ACLs are mutually "
				    "exclusive", name);
			} else {
				sblock.fs_flags |= FS_ACLS;
				warnx("%s set", name);
			}
		} else if (strcmp(avalue, "disable") == 0) {
			if ((~sblock.fs_flags & FS_ACLS) ==
			    FS_ACLS) {
				warnx("%s remains unchanged as disabled",
				    name);
			} else {
				sblock.fs_flags &= ~FS_ACLS;
				warnx("%s cleared", name);
			}
		}
	}
	if (eflag) {
		name = "maximum blocks per file in a cylinder group";
		if (sblock.fs_maxbpg == evalue)
			warnx("%s remains unchanged as %d", name, evalue);
		else {
			warnx("%s changes from %d to %d",
			    name, sblock.fs_maxbpg, evalue);
			sblock.fs_maxbpg = evalue;
		}
	}
	if (fflag) {
		name = "average file size";
		if (sblock.fs_avgfilesize == (unsigned)fvalue) {
			warnx("%s remains unchanged as %d", name, fvalue);
		}
		else {
			warnx("%s changes from %d to %d",
					name, sblock.fs_avgfilesize, fvalue);
			sblock.fs_avgfilesize = fvalue;
		}
	}
	if (Jflag) {
		name = "gjournal";
		if (strcmp(Jvalue, "enable") == 0) {
			if (sblock.fs_flags & FS_GJOURNAL) {
				warnx("%s remains unchanged as enabled", name);
			} else {
				sblock.fs_flags |= FS_GJOURNAL;
				warnx("%s set", name);
			}
		} else if (strcmp(Jvalue, "disable") == 0) {
			if ((~sblock.fs_flags & FS_GJOURNAL) ==
			    FS_GJOURNAL) {
				warnx("%s remains unchanged as disabled",
				    name);
			} else {
				sblock.fs_flags &= ~FS_GJOURNAL;
				warnx("%s cleared", name);
			}
		}
	}
	if (lflag) {
		name = "multilabel";
		if (strcmp(lvalue, "enable") == 0) {
			if (sblock.fs_flags & FS_MULTILABEL) {
				warnx("%s remains unchanged as enabled", name);
			} else {
				sblock.fs_flags |= FS_MULTILABEL;
				warnx("%s set", name);
			}
		} else if (strcmp(lvalue, "disable") == 0) {
			if ((~sblock.fs_flags & FS_MULTILABEL) ==
			    FS_MULTILABEL) {
				warnx("%s remains unchanged as disabled",
				    name);
			} else {
				sblock.fs_flags &= ~FS_MULTILABEL;
				warnx("%s cleared", name);
			}
		}
	}
	if (mflag) {
		name = "minimum percentage of free space";
		if (sblock.fs_minfree == mvalue)
			warnx("%s remains unchanged as %d%%", name, mvalue);
		else {
			warnx("%s changes from %d%% to %d%%",
				    name, sblock.fs_minfree, mvalue);
			sblock.fs_minfree = mvalue;
			if (mvalue >= MINFREE && sblock.fs_optim == FS_OPTSPACE)
				warnx(OPTWARN, "time", ">=", MINFREE);
			if (mvalue < MINFREE && sblock.fs_optim == FS_OPTTIME)
				warnx(OPTWARN, "space", "<", MINFREE);
		}
	}
	if (Nflag) {
		name = "NFSv4 ACLs";
		if (strcmp(Nvalue, "enable") == 0) {
			if (sblock.fs_flags & FS_NFS4ACLS) {
				warnx("%s remains unchanged as enabled", name);
			} else if (sblock.fs_flags & FS_ACLS) {
				warnx("%s and POSIX.1e ACLs are mutually "
				    "exclusive", name);
			} else {
				sblock.fs_flags |= FS_NFS4ACLS;
				warnx("%s set", name);
			}
		} else if (strcmp(Nvalue, "disable") == 0) {
			if ((~sblock.fs_flags & FS_NFS4ACLS) ==
			    FS_NFS4ACLS) {
				warnx("%s remains unchanged as disabled",
				    name);
			} else {
				sblock.fs_flags &= ~FS_NFS4ACLS;
				warnx("%s cleared", name);
			}
		}
	}
	if (nflag) {
 		name = "soft updates";
 		if (strcmp(nvalue, "enable") == 0) {
			if (sblock.fs_flags & FS_DOSOFTDEP)
				warnx("%s remains unchanged as enabled", name);
			else if (sblock.fs_clean == 0) {
				warnx("%s cannot be enabled until fsck is run",
				    name);
			} else {
 				sblock.fs_flags |= FS_DOSOFTDEP;
 				warnx("%s set", name);
			}
 		} else if (strcmp(nvalue, "disable") == 0) {
			if ((~sblock.fs_flags & FS_DOSOFTDEP) == FS_DOSOFTDEP)
				warnx("%s remains unchanged as disabled", name);
			else {
 				sblock.fs_flags &= ~FS_DOSOFTDEP;
 				warnx("%s cleared", name);
			}
 		}
	}
	if (oflag) {
		name = "optimization preference";
		chg[FS_OPTSPACE] = "space";
		chg[FS_OPTTIME] = "time";
		if (sblock.fs_optim == ovalue)
			warnx("%s remains unchanged as %s", name, chg[ovalue]);
		else {
			warnx("%s changes from %s to %s",
				    name, chg[sblock.fs_optim], chg[ovalue]);
			sblock.fs_optim = ovalue;
			if (sblock.fs_minfree >= MINFREE &&
			    ovalue == FS_OPTSPACE)
				warnx(OPTWARN, "time", ">=", MINFREE);
			if (sblock.fs_minfree < MINFREE && ovalue == FS_OPTTIME)
				warnx(OPTWARN, "space", "<", MINFREE);
		}
	}
	if (sflag) {
		name = "expected number of files per directory";
		if (sblock.fs_avgfpdir == (unsigned)svalue) {
			warnx("%s remains unchanged as %d", name, svalue);
		}
		else {
			warnx("%s changes from %d to %d",
					name, sblock.fs_avgfpdir, svalue);
			sblock.fs_avgfpdir = svalue;
		}
	}
	if (tflag) {
		name = "issue TRIM to the disk";
 		if (strcmp(tvalue, "enable") == 0) {
			if (sblock.fs_flags & FS_TRIM)
				warnx("%s remains unchanged as enabled", name);
			else {
 				sblock.fs_flags |= FS_TRIM;
 				warnx("%s set", name);
			}
 		} else if (strcmp(tvalue, "disable") == 0) {
			if ((~sblock.fs_flags & FS_TRIM) == FS_TRIM)
				warnx("%s remains unchanged as disabled", name);
			else {
 				sblock.fs_flags &= ~FS_TRIM;
 				warnx("%s cleared", name);
			}
 		}
	}

	if (sbwrite(&disk, Aflag) == -1)
		goto err;
	ufs_disk_close(&disk);
	if (active) {
		bzero(&args, sizeof(args));
		if (mount("ufs", on,
		    stfs.f_flags | MNT_UPDATE | MNT_RELOAD, &args) < 0)
			err(9, "%s: reload", special);
		warnx("file system reloaded");
	}
	exit(0);
err:
	if (disk.d_error != NULL)
		errx(11, "%s: %s", special, disk.d_error);
	else
		err(12, "%s", special);
}

void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n",
"usage: tunefs [-A] [-a enable | disable] [-e maxbpg] [-f avgfilesize]",
"              [-J enable | disable ] [-L volname] [-l enable | disable]",
"              [-m minfree] [-N enable | disable] [-n enable | disable]",
"              [-o space | time] [-p] [-s avgfpdir] [-t enable | disable]",
"              special | filesystem");
	exit(2);
}

void
printfs(void)
{
	warnx("POSIX.1e ACLs: (-a)                                %s",
		(sblock.fs_flags & FS_ACLS)? "enabled" : "disabled");
	warnx("NFSv4 ACLs: (-N)                                   %s",
		(sblock.fs_flags & FS_NFS4ACLS)? "enabled" : "disabled");
	warnx("MAC multilabel: (-l)                               %s",
		(sblock.fs_flags & FS_MULTILABEL)? "enabled" : "disabled");
	warnx("soft updates: (-n)                                 %s", 
		(sblock.fs_flags & FS_DOSOFTDEP)? "enabled" : "disabled");
	warnx("gjournal: (-J)                                     %s",
		(sblock.fs_flags & FS_GJOURNAL)? "enabled" : "disabled");
	warnx("trim: (-t)                                         %s", 
		(sblock.fs_flags & FS_TRIM)? "enabled" : "disabled");
	warnx("maximum blocks per file in a cylinder group: (-e)  %d",
	      sblock.fs_maxbpg);
	warnx("average file size: (-f)                            %d",
	      sblock.fs_avgfilesize);
	warnx("average number of files in a directory: (-s)       %d",
	      sblock.fs_avgfpdir);
	warnx("minimum percentage of free space: (-m)             %d%%",
	      sblock.fs_minfree);
	warnx("optimization preference: (-o)                      %s",
	      sblock.fs_optim == FS_OPTSPACE ? "space" : "time");
	if (sblock.fs_minfree >= MINFREE &&
	    sblock.fs_optim == FS_OPTSPACE)
		warnx(OPTWARN, "time", ">=", MINFREE);
	if (sblock.fs_minfree < MINFREE &&
	    sblock.fs_optim == FS_OPTTIME)
		warnx(OPTWARN, "space", "<", MINFREE);
	warnx("volume label: (-L)                                 %s",
		sblock.fs_volname);
}
