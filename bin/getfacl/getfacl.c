/*-
 * Copyright (c) 1999-2001 Robert N M Watson
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
 *
 *	$FreeBSD$
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
#include <stdio.h>
#include <unistd.h>

int	more_than_one = 0;

static void
usage(void)
{

	fprintf(stderr, "getfacl [-d] [files ...]\n");
}

static acl_t
acl_from_stat(struct stat sb)
{
	acl_t acl;

	acl = acl_init(3);
	if (!acl)
		return(NULL);

	acl->acl_entry[0].ae_tag = ACL_USER_OBJ;
	acl->acl_entry[0].ae_id  = sb.st_uid;
	acl->acl_entry[0].ae_perm = 0;
	if (sb.st_mode & S_IRUSR)
		acl->acl_entry[0].ae_perm |= ACL_READ;
	if (sb.st_mode & S_IWUSR)
		acl->acl_entry[0].ae_perm |= ACL_WRITE;
	if (sb.st_mode & S_IXUSR)
		acl->acl_entry[0].ae_perm |= ACL_EXECUTE;

	acl->acl_entry[1].ae_tag = ACL_GROUP_OBJ;
	acl->acl_entry[1].ae_id  = sb.st_gid;
	acl->acl_entry[1].ae_perm = 0;
	if (sb.st_mode & S_IRGRP)
		acl->acl_entry[1].ae_perm |= ACL_READ;
	if (sb.st_mode & S_IWGRP)
		acl->acl_entry[1].ae_perm |= ACL_WRITE;
	if (sb.st_mode & S_IXGRP)
		acl->acl_entry[1].ae_perm |= ACL_EXECUTE;

	acl->acl_entry[2].ae_tag = ACL_OTHER_OBJ;
	acl->acl_entry[2].ae_id  = 0;
	acl->acl_entry[2].ae_perm = 0;
	if (sb.st_mode & S_IROTH)
		acl->acl_entry[2].ae_perm |= ACL_READ;
	if (sb.st_mode & S_IWOTH)
		acl->acl_entry[2].ae_perm |= ACL_WRITE;
	if (sb.st_mode & S_IXOTH)
		acl->acl_entry[2].ae_perm |= ACL_EXECUTE;

	acl->acl_cnt = 3;

	return(acl);
}

static int
print_acl(char *path, acl_type_t type)
{
	struct stat	sb;
	acl_t	acl;
	char	*acl_text;
	int	error;

	error = stat(path, &sb);
	if (error == -1) {
		perror(path);
		return(-1);
	}

	if (more_than_one)
		printf("\n");
	else
		more_than_one++;

	printf("#file:%s\n#owner:%d\n#group:%d\n", path, sb.st_uid, sb.st_gid);

	acl = acl_get_file(path, type);
	if (!acl) {
		if (errno != EOPNOTSUPP) {
			warn("%s", path);
			return(-1);
		}
		errno = 0;
		if (type != ACL_TYPE_ACCESS)
			return(0);
		acl = acl_from_stat(sb);
		if (!acl) {
			perror("acl_from_stat()");
			return(-1);
		}
	}

	acl_text = acl_to_text(acl, 0);
	if (!acl_text) {
		perror(path);
		return(-1);
	}

	printf("%s", acl_text);

	acl_free(acl);
	acl_free(acl_text);

	return(0);
}

static int
print_acl_from_stdin(acl_type_t type)
{
	char	pathname[PATH_MAX];
	int	carried_error = 0;

	pathname[sizeof(pathname) - 1] = '\0';
	while (fgets(pathname, sizeof(pathname), stdin)) {
		/* remove the \n */
		pathname[strlen(pathname) - 1] = '\0';
		if (print_acl(pathname, type) == -1) {
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

	while ((ch = getopt(argc, argv, "d")) != -1)
		switch(ch) {
		case 'd':
			type = ACL_TYPE_DEFAULT;
			break;
		default:
			usage();
			return(-1);
		}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		error = print_acl_from_stdin(type);
		return(error);
	}

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-")) {
			error = print_acl_from_stdin(type);
			if (error == -1)
				carried_error = -1;
		} else {
			error = print_acl(argv[i], type);
			if (error == -1)
				carried_error = -1;
		}
	}

	return(carried_error);
}
