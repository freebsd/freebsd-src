/*-
 * Copyright (c) 1999, 2001, 2002 Robert N M Watson
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
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
/*
 * getfacl -- POSIX.1e utility to extract ACLs from files and directories
 * and send the results to stdout
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/acl.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int more_than_one = 0;

static const struct option long_options[] =
{
	{ "default",		no_argument,	NULL,	'd' },
	{ "numeric",		no_argument,	NULL,	'n' },
	{ "omit-header",	no_argument,	NULL,	'q' },
	{ "skip-base",		no_argument,	NULL,	's' },
	{ NULL,			no_argument,	NULL,	0 },
};

static void
usage(void)
{

	fprintf(stderr, "getfacl [-dhnqv] [file ...]\n");
}

static char *
getuname(uid_t uid)
{
	struct passwd *pw;
	static char uids[10];

	if ((pw = getpwuid(uid)) == NULL) {
		(void)snprintf(uids, sizeof(uids), "%u", uid);
		return (uids);
	} else
		return (pw->pw_name);
}

static char *
getgname(gid_t gid)
{
	struct group *gr;
	static char gids[10];

	if ((gr = getgrgid(gid)) == NULL) {
		(void)snprintf(gids, sizeof(gids), "%u", gid);
		return (gids);
	} else
		return (gr->gr_name);
}

static int
print_acl(char *path, acl_type_t type, int hflag, int iflag, int nflag,
    int qflag, int vflag, int sflag)
{
	struct stat	sb;
	acl_t	acl;
	char	*acl_text;
	int	error, flags = 0, ret;

	if (hflag)
		error = lstat(path, &sb);
	else
		error = stat(path, &sb);
	if (error == -1) {
		warn("%s: stat() failed", path);
		return(-1);
	}

	if (hflag)
		ret = lpathconf(path, _PC_ACL_NFS4);
	else
		ret = pathconf(path, _PC_ACL_NFS4);
	if (ret > 0) {
		if (type == ACL_TYPE_DEFAULT) {
			warnx("%s: there are no default entries in NFSv4 ACLs",
			    path);
			return (-1);
		}
		type = ACL_TYPE_NFS4;
	} else if (ret < 0 && errno != EINVAL) {
		warn("%s: pathconf(..., _PC_ACL_NFS4) failed", path);
		return (-1);
	}

	if (hflag)
		acl = acl_get_link_np(path, type);
	else
		acl = acl_get_file(path, type);

	if (!acl && errno != EOPNOTSUPP) {
		warn("%s", path);
		return(-1);
	}

	if (sflag) {
		int trivial;

		/*
		 * With the -s flag, we shouldn't synthesize a trivial ACL if
		 * they aren't supported as we do below.
		 */
		if (!acl)
			return(0);

		/*
		 * We also shouldn't render anything for this path if it's a
		 * trivial ACL.  If we error out, we'll issue a warning but
		 * proceed with this file to err on the side of caution.
		 */
		error = acl_is_trivial_np(acl, &trivial);
		if (error != 0) {
			warn("%s: acl_is_trivial_np failed", path);
		} else if (trivial) {
			(void)acl_free(acl);
			return(0);
		}
	}

	if (more_than_one)
		printf("\n");
	else
		more_than_one++;
	if (!qflag)
		printf("# file: %s\n# owner: %s\n# group: %s\n", path,
		    getuname(sb.st_uid), getgname(sb.st_gid));

	if (!acl) {
		if (type == ACL_TYPE_DEFAULT)
			return(0);
		acl = acl_from_mode_np(sb.st_mode);
		if (!acl) {
			warn("%s: acl_from_mode() failed", path);
			return(-1);
		}
	}

	if (iflag)
		flags |= ACL_TEXT_APPEND_ID;

	if (nflag)
		flags |= ACL_TEXT_NUMERIC_IDS;

	if (vflag)
		flags |= ACL_TEXT_VERBOSE;

	acl_text = acl_to_text_np(acl, 0, flags);
	if (!acl_text) {
		warn("%s: acl_to_text_np() failed", path);
		(void)acl_free(acl);
		return(-1);
	}

	printf("%s", acl_text);

	(void)acl_free(acl);
	(void)acl_free(acl_text);

	return(0);
}

static int
print_acl_from_stdin(acl_type_t type, int hflag, int iflag, int nflag,
    int qflag, int vflag, int sflag)
{
	char	*p, pathname[PATH_MAX];
	int	carried_error = 0;

	while (fgets(pathname, (int)sizeof(pathname), stdin)) {
		if ((p = strchr(pathname, '\n')) != NULL)
			*p = '\0';
		if (print_acl(pathname, type, hflag, iflag, nflag,
		    qflag, vflag, sflag) == -1) {
			carried_error = -1;
		}
	}

	return(carried_error);
}

int
main(int argc, char *argv[])
{
	acl_type_t	type = ACL_TYPE_ACCESS;
	int	carried_error = 0;
	int	ch, error, i;
	int	hflag, iflag, qflag, nflag, sflag, vflag;

	hflag = 0;
	iflag = 0;
	qflag = 0;
	nflag = 0;
	sflag = 0;
	vflag = 0;
	while ((ch = getopt_long(argc, argv, "+dhinqsv", long_options,
	    NULL)) != -1)
		switch(ch) {
		case 'd':
			type = ACL_TYPE_DEFAULT;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		default:
			usage();
			return(-1);
		}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		error = print_acl_from_stdin(type, hflag, iflag, nflag,
		    qflag, vflag, sflag);
		return(error ? 1 : 0);
	}

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-")) {
			error = print_acl_from_stdin(type, hflag, iflag, nflag,
			    qflag, vflag, sflag);
			if (error == -1)
				carried_error = -1;
		} else {
			error = print_acl(argv[i], type, hflag, iflag, nflag,
			    qflag, vflag, sflag);
			if (error == -1)
				carried_error = -1;
		}
	}

	return(carried_error ? 1 : 0);
}
