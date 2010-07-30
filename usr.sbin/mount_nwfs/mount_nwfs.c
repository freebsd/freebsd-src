/*
 * Copyright (c) 1999, Boris Popov
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>

#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <err.h>
#include <sysexits.h>
#include <time.h>

#include <netncp/ncp_lib.h>
#include <netncp/ncp_rcfile.h>
#include <fs/nwfs/nwfs_mount.h>
#include "mntopts.h"

#define	NWFS_VFSNAME	"nwfs"

static char mount_point[MAXPATHLEN + 1];
static void usage(void);
static int parsercfile(struct ncp_conn_loginfo *li, struct nwfs_args *mdata);

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_END
};

static int
parsercfile(struct ncp_conn_loginfo *li __unused,
    struct nwfs_args *mdata __unused)
{
	return 0;
}

int
main(int argc, char *argv[])
{
	NWCONN_HANDLE connHandle;
	struct nwfs_args mdata;
	struct ncp_conn_loginfo li;
	struct stat st;
	struct nw_entry_info einfo;
	struct tm *tm;
	time_t ltime;
	int opt, error, mntflags, nlsopt, wall_clock;
	int uid_set, gid_set;
	size_t len;
	char *p, *p1, tmp[1024];
	u_char *pv;

	if (argc < 2)
		usage();
	if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0) {
			usage();
		} else if (strcmp(argv[1], "-v") == 0) {
			errx(EX_OK, "version %d.%d.%d", NWFS_VERSION / 100000,
			    (NWFS_VERSION % 10000) / 1000,
			    (NWFS_VERSION % 1000) / 100);
		}
	}

	if(ncp_initlib()) exit(1);

	mntflags = error = 0;
	bzero(&mdata,sizeof(mdata));
	gid_set = uid_set = 0;
	nlsopt = 0;

	if (ncp_li_init(&li, argc, argv)) return 1;
	/*
	 * A little bit weird, but I should figure out which server/user to use
	 * _before_ reading .rc file
	 */
	if (argc >= 3 && argv[argc-1][0] != '-' && argv[argc-2][0] != '-' &&
	    argv[argc-2][0] == '/') {
		p = argv[argc-2];
		error = 1;
		do {
			if (*p++ != '/') break;
			p1 = tmp;
			while (*p != ':' && *p != 0) *p1++ = *p++;
			if (*p++ == 0) break;
			*p1 = 0;
			if (ncp_li_setserver(&li, tmp)) break;
			p1 = tmp;
			while (*p != '/' && *p != 0) *p1++ = *p++;
			if (*p++ == 0) break;
			*p1 = 0;
			if (ncp_li_setuser(&li, tmp)) break;
			p1 = tmp;
			while (*p != '/' && *p != 0) *p1++ = *p++;
			*p1 = 0;
			if (strlen(tmp) > NCP_VOLNAME_LEN) {
				warnx("volume name too long: %s", tmp);
				break;
			}
			ncp_str_upper(strcpy(mdata.mounted_vol,tmp));
			if (*p == '/')
				p++;
			p1 = mdata.root_path + 2;
			pv = mdata.root_path + 1;
			for(;*p;) {
				*pv = 0;
				while (*p != '/' && *p) {
					*p1++ = *p++;
					(*pv)++;
				}
				if (*pv) {
					ncp_nls_mem_u2n(pv + 1, pv + 1, *pv);
					pv += (*pv) + 1;
					mdata.root_path[0]++;
				}
				if (*p++ == 0) break;
				p1++;
			}
			error = 0;
		} while(0);
		if (error)
			errx(EX_DATAERR, 
			    "an error occurred while parsing '%s'",
			    argv[argc - 2]);
	}
	if (ncp_li_readrc(&li)) return 1;
	if (ncp_rc) {
		parsercfile(&li,&mdata);
		rc_close(ncp_rc);
	}
	while ((opt = getopt(argc, argv, STDPARAM_OPT"V:c:d:f:g:l:n:o:u:w:")) != -1) {
		switch (opt) {
		    case STDPARAM_ARGS:
			if (ncp_li_arg(&li, opt, optarg)) {	
				return 1;
			}
			break;
		    case 'V':
			if (strlen(optarg) > NCP_VOLNAME_LEN)
				errx(EX_DATAERR, "volume too long: %s", optarg);
			ncp_str_upper(strcpy(mdata.mounted_vol,optarg));
			break;
		    case 'u': {
			struct passwd *pwd;

			pwd = isdigit(optarg[0]) ?
			    getpwuid(atoi(optarg)) : getpwnam(optarg);
			if (pwd == NULL)
				errx(EX_NOUSER, "unknown user '%s'", optarg);
			mdata.uid = pwd->pw_uid;
			uid_set = 1;
			break;
		    }
		    case 'g': {
			struct group *grp;

			grp = isdigit(optarg[0]) ?
			    getgrgid(atoi(optarg)) : getgrnam(optarg);
			if (grp == NULL)
				errx(EX_NOUSER, "unknown group '%s'", optarg);
			mdata.gid = grp->gr_gid;
			gid_set = 1;
			break;
		    }
		    case 'd':
			errno = 0;
			mdata.dir_mode = strtol(optarg, &p, 8);
			if (errno || *p != 0)
				errx(EX_DATAERR, "invalid value for directory mode");
			break;
		    case 'f':
			errno = 0;
			mdata.file_mode = strtol(optarg, &p, 8);
			if (errno || *p != 0)
				errx(EX_DATAERR, "invalid value for file mode");
			break;
		    case '?':
			usage();
			/*NOTREACHED*/
		    case 'n': {
			char *inp, *nsp;

			nsp = inp = optarg;
			while ((nsp = strsep(&inp, ",;:")) != NULL) {
				if (strcasecmp(nsp, "OS2") == 0)
					mdata.flags |= NWFS_MOUNT_NO_OS2;
				else if (strcasecmp(nsp, "LONG") == 0)
					mdata.flags |= NWFS_MOUNT_NO_LONG;
				else if (strcasecmp(nsp, "NFS") == 0)
					mdata.flags |= NWFS_MOUNT_NO_NFS;
				else
					errx(EX_DATAERR, "unknown namespace '%s'", nsp);
			}
			break;
		    };
		    case 'l':
			if (ncp_nls_setlocale(optarg) != 0) return 1;
			mdata.flags |= NWFS_MOUNT_HAVE_NLS;
			break;
		    case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			break;
		    case 'c':
			switch (optarg[0]) {
			    case 'l':
				nlsopt |= NWHP_LOWER;
				break;
			    case 'u':
				nlsopt |= NWHP_UPPER;
				break;
			    case 'n':
				nlsopt |= NWHP_LOWER | NWHP_UPPER;
				break;
			    case 'L':
				nlsopt |= NWHP_LOWER | NWHP_NOSTRICT;
				break;
			    case 'U':
				nlsopt |= NWHP_UPPER | NWHP_NOSTRICT;
				break;
			    default:
		    		errx(EX_DATAERR, "invalid suboption '%c' for -c",
				    optarg[0]);
			}
			break;
		    case 'w':
			if (ncp_nls_setrecodebyname(optarg) != 0)
				return 1;
			mdata.flags |= NWFS_MOUNT_HAVE_NLS;
			break;
		    default:
			usage();
		}
	}

	if (optind == argc - 2) {
		optind++;
	} else if (mdata.mounted_vol[0] == 0)
		errx(EX_USAGE, "volume name should be specified");
	
	if (optind != argc - 1)
		usage();
	realpath(argv[optind], mount_point);

	if (stat(mount_point, &st) == -1)
		err(EX_OSERR, "could not find mount point %s", mount_point);
	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		err(EX_OSERR, "can't mount on %s", mount_point);
	}
	if (ncp_geteinfo(mount_point, &einfo) == 0)
		errx(EX_OSERR, "can't mount on %s twice", mount_point);

	if (uid_set == 0) {
		mdata.uid = st.st_uid;
	}
	if (gid_set == 0) {
		mdata.gid = st.st_gid;
	}
	if (mdata.file_mode == 0 ) {
		mdata.file_mode = st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
	}
	if (mdata.dir_mode == 0) {
		mdata.dir_mode = mdata.file_mode;
		if ((mdata.dir_mode & S_IRUSR) != 0)
			mdata.dir_mode |= S_IXUSR;
		if ((mdata.dir_mode & S_IRGRP) != 0)
			mdata.dir_mode |= S_IXGRP;
		if ((mdata.dir_mode & S_IROTH) != 0)
			mdata.dir_mode |= S_IXOTH;
	}
	if (li.access_mode == 0) {
		li.access_mode = mdata.dir_mode;
	}
/*	if (mdata.flags & NWFS_MOUNT_HAVE_NLS) {*/
		mdata.nls = ncp_nls;
/*	}*/
	mdata.nls.opt = nlsopt;

	len = sizeof(wall_clock);
	if (sysctlbyname("machdep.wall_cmos_clock", &wall_clock, &len, NULL, 0) == -1)
		err(EX_OSERR, "get wall_clock");
	if (wall_clock == 0) {
		time(&ltime);
		tm = localtime(&ltime);
		mdata.tz = -(tm->tm_gmtoff / 60);
	}

	error = ncp_li_check(&li);
	if (error)
		return 1;
	li.opt |= NCP_OPT_WDOG;
	/* well, now we can try to login, or use already established connection */
	error = ncp_li_login(&li, &connHandle);
	if (error) {
		ncp_error("cannot login to server %s", error, li.server);
		exit(1);
	}
	error = ncp_conn2ref(connHandle, &mdata.connRef);
	if (error) {
		ncp_error("could not convert handle to reference", error);
		ncp_disconnect(connHandle);
		exit(1);
	}
	strcpy(mdata.mount_point,mount_point);
	mdata.version = NWFS_VERSION;
	error = mount(NWFS_VFSNAME, mdata.mount_point, mntflags, (void*)&mdata);
	if (error) {
		ncp_error("mount error: %s", error, mdata.mount_point);
		ncp_disconnect(connHandle);
		exit(1);
	}
	/*
	 * I'm leave along my handle, but kernel should keep own ...
	 */
	ncp_disconnect(connHandle);
	/* we are done ?, impossible ... */
	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n",
	"usage: mount_nwfs [-Chv] -S server -U user [-connection options]",
	"                  -V volume [-M mode] [-c case] [-d mode] [-f mode]",
	"                  [-g gid] [-l locale] [-n os2] [-u uid] [-w scheme]",
	"                  node",
	"       mount_nwfs [-options] /server:user/volume[/path] node");

	exit (1);
}
