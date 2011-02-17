/*-
 * Copyright (c) 2006-2010 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#ifndef makedev
#include <sys/mkdev.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef HAS_TRUNCATE64
#define	truncate64	truncate
#endif
#ifndef HAS_STAT64
#define	stat64	stat
#define	lstat64	lstat
#endif
#ifdef HAS_FREEBSD_ACL
#include <sys/acl.h>
#endif

#ifndef ALLPERMS
#define	ALLPERMS	(S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)
#endif

enum action {
	ACTION_OPEN,
	ACTION_CREATE,
	ACTION_UNLINK,
	ACTION_MKDIR,
	ACTION_RMDIR,
	ACTION_LINK,
	ACTION_SYMLINK,
	ACTION_RENAME,
	ACTION_MKFIFO,
	ACTION_MKNOD,
	ACTION_BIND,
	ACTION_CONNECT,
	ACTION_CHMOD,
#ifdef HAS_LCHMOD
	ACTION_LCHMOD,
#endif
	ACTION_CHOWN,
	ACTION_LCHOWN,
#ifdef HAS_CHFLAGS
	ACTION_CHFLAGS,
#endif
#ifdef HAS_LCHFLAGS
	ACTION_LCHFLAGS,
#endif
	ACTION_TRUNCATE,
	ACTION_STAT,
	ACTION_LSTAT,
	ACTION_PATHCONF,
#ifdef HAS_FREEBSD_ACL
	ACTION_PREPENDACL,
	ACTION_READACL,
#endif
	ACTION_WRITE,
};

#define	TYPE_NONE	0x0000
#define	TYPE_STRING	0x0001
#define	TYPE_NUMBER	0x0002

#define	TYPE_OPTIONAL	0x0100

#define	MAX_ARGS	8

struct syscall_desc {
	char		*sd_name;
	enum action	 sd_action;
	int		 sd_args[MAX_ARGS];
};

static struct syscall_desc syscalls[] = {
	{ "open", ACTION_OPEN, { TYPE_STRING, TYPE_STRING, TYPE_NUMBER | TYPE_OPTIONAL, TYPE_NONE } },
	{ "create", ACTION_CREATE, { TYPE_STRING, TYPE_NUMBER, TYPE_NONE } },
	{ "unlink", ACTION_UNLINK, { TYPE_STRING, TYPE_NONE } },
	{ "mkdir", ACTION_MKDIR, { TYPE_STRING, TYPE_NUMBER, TYPE_NONE } },
	{ "rmdir", ACTION_RMDIR, { TYPE_STRING, TYPE_NONE } },
	{ "link", ACTION_LINK, { TYPE_STRING, TYPE_STRING, TYPE_NONE } },
	{ "symlink", ACTION_SYMLINK, { TYPE_STRING, TYPE_STRING, TYPE_NONE } },
	{ "rename", ACTION_RENAME, { TYPE_STRING, TYPE_STRING, TYPE_NONE } },
	{ "mkfifo", ACTION_MKFIFO, { TYPE_STRING, TYPE_NUMBER, TYPE_NONE } },
	{ "mknod", ACTION_MKNOD, { TYPE_STRING, TYPE_STRING, TYPE_NUMBER, TYPE_NUMBER, TYPE_NUMBER, TYPE_NONE} },
	{ "bind", ACTION_BIND, { TYPE_STRING, TYPE_NONE } },
	{ "connect", ACTION_CONNECT, { TYPE_STRING, TYPE_NONE } },
	{ "chmod", ACTION_CHMOD, { TYPE_STRING, TYPE_NUMBER, TYPE_NONE } },
#ifdef HAS_LCHMOD
	{ "lchmod", ACTION_LCHMOD, { TYPE_STRING, TYPE_NUMBER, TYPE_NONE } },
#endif
	{ "chown", ACTION_CHOWN, { TYPE_STRING, TYPE_NUMBER, TYPE_NUMBER, TYPE_NONE } },
	{ "lchown", ACTION_LCHOWN, { TYPE_STRING, TYPE_NUMBER, TYPE_NUMBER, TYPE_NONE } },
#ifdef HAS_CHFLAGS
	{ "chflags", ACTION_CHFLAGS, { TYPE_STRING, TYPE_STRING, TYPE_NONE } },
#endif
#ifdef HAS_LCHFLAGS
	{ "lchflags", ACTION_LCHFLAGS, { TYPE_STRING, TYPE_STRING, TYPE_NONE } },
#endif
	{ "truncate", ACTION_TRUNCATE, { TYPE_STRING, TYPE_NUMBER, TYPE_NONE } },
	{ "stat", ACTION_STAT, { TYPE_STRING, TYPE_STRING, TYPE_NONE } },
	{ "lstat", ACTION_LSTAT, { TYPE_STRING, TYPE_STRING, TYPE_NONE } },
	{ "pathconf", ACTION_PATHCONF, { TYPE_STRING, TYPE_STRING, TYPE_NONE } },
#ifdef HAS_FREEBSD_ACL
	{ "prependacl", ACTION_PREPENDACL, { TYPE_STRING, TYPE_STRING, TYPE_NONE } },
	{ "readacl", ACTION_READACL, { TYPE_STRING, TYPE_NONE } },
#endif
	{ "write", ACTION_WRITE, { TYPE_STRING, TYPE_NONE } },
	{ NULL, -1, { TYPE_NONE } }
};

struct flag {
	long long	 f_flag;
	char		*f_str;
};

static struct flag open_flags[] = {
#ifdef O_RDONLY
	{ O_RDONLY, "O_RDONLY" },
#endif
#ifdef O_WRONLY
	{ O_WRONLY, "O_WRONLY" },
#endif
#ifdef O_RDWR
	{ O_RDWR, "O_RDWR" },
#endif
#ifdef O_NONBLOCK
	{ O_NONBLOCK, "O_NONBLOCK" },
#endif
#ifdef O_APPEND
	{ O_APPEND, "O_APPEND" },
#endif
#ifdef O_CREAT
	{ O_CREAT, "O_CREAT" },
#endif
#ifdef O_TRUNC
	{ O_TRUNC, "O_TRUNC" },
#endif
#ifdef O_EXCL
	{ O_EXCL, "O_EXCL" },
#endif
#ifdef O_SHLOCK
	{ O_SHLOCK, "O_SHLOCK" },
#endif
#ifdef O_EXLOCK
	{ O_EXLOCK, "O_EXLOCK" },
#endif
#ifdef O_DIRECT
	{ O_DIRECT, "O_DIRECT" },
#endif
#ifdef O_FSYNC
	{ O_FSYNC, "O_FSYNC" },
#endif
#ifdef O_SYNC
	{ O_SYNC, "O_SYNC" },
#endif
#ifdef O_NOFOLLOW
	{ O_NOFOLLOW, "O_NOFOLLOW" },
#endif
#ifdef O_NOCTTY
	{ O_NOCTTY, "O_NOCTTY" },
#endif
	{ 0, NULL }
};

#ifdef HAS_CHFLAGS
static struct flag chflags_flags[] = {
#ifdef UF_NODUMP
	{ UF_NODUMP, "UF_NODUMP" },
#endif
#ifdef UF_IMMUTABLE
	{ UF_IMMUTABLE, "UF_IMMUTABLE" },
#endif
#ifdef UF_APPEND
	{ UF_APPEND, "UF_APPEND" },
#endif
#ifdef UF_NOUNLINK
	{ UF_NOUNLINK, "UF_NOUNLINK" },
#endif
#ifdef UF_OPAQUE
	{ UF_OPAQUE, "UF_OPAQUE" },
#endif
#ifdef SF_ARCHIVED
	{ SF_ARCHIVED, "SF_ARCHIVED" },
#endif
#ifdef SF_IMMUTABLE
	{ SF_IMMUTABLE, "SF_IMMUTABLE" },
#endif
#ifdef SF_APPEND
	{ SF_APPEND, "SF_APPEND" },
#endif
#ifdef SF_NOUNLINK
	{ SF_NOUNLINK, "SF_NOUNLINK" },
#endif
#ifdef SF_SNAPSHOT
	{ SF_SNAPSHOT, "SF_SNAPSHOT" },
#endif
	{ 0, NULL }
};
#endif

struct name {
	int	 n_name;
	char	*n_str;
};

static struct name pathconf_names[] = {
#ifdef _PC_LINK_MAX
	{ _PC_LINK_MAX, "_PC_LINK_MAX" },
#endif
#ifdef _PC_NAME_MAX
	{ _PC_NAME_MAX, "_PC_NAME_MAX" },
#endif
#ifdef _PC_PATH_MAX
	{ _PC_PATH_MAX, "_PC_PATH_MAX" },
#endif
#ifdef _PC_SYMLINK_MAX
	{ _PC_SYMLINK_MAX, "_PC_SYMLINK_MAX" },
#endif
	{ 0, NULL }
};

static const char *err2str(int error);

static void
usage(void)
{

	fprintf(stderr, "usage: pjdfstest [-U umask] [-u uid] [-g gid1[,gid2[...]]] syscall args ...\n");
	exit(1);
}

static long long
str2flags(struct flag *tflags, char *sflags)
{
	long long flags = 0;
	unsigned int i;
	char *f;

	for (f = strtok(sflags, ","); f != NULL; f = strtok(NULL, ",")) {
		/* Support magic 'none' flag which just reset all flags. */
		if (strcmp(f, "none") == 0)
			return (0);
		for (i = 0; tflags[i].f_str != NULL; i++) {
			if (strcmp(tflags[i].f_str, f) == 0)
				break;
		}
		if (tflags[i].f_str == NULL) {
			fprintf(stderr, "unknown flag '%s'\n", f);
			exit(1);
		}
		flags |= tflags[i].f_flag;
	}
	return (flags);
}

#ifdef HAS_CHFLAGS
static char *
flags2str(struct flag *tflags, long long flags)
{
	static char sflags[1024];
	unsigned int i;

	sflags[0] = '\0';
	for (i = 0; tflags[i].f_str != NULL; i++) {
		if (flags & tflags[i].f_flag) {
			if (sflags[0] != '\0')
				strlcat(sflags, ",", sizeof(sflags));
			strlcat(sflags, tflags[i].f_str, sizeof(sflags));
		}
	}
	if (sflags[0] == '\0')
		strlcpy(sflags, "none", sizeof(sflags));
	return (sflags);
}
#endif

static int
str2name(struct name *names, char *name)
{
	unsigned int i;

	for (i = 0; names[i].n_str != NULL; i++) {
		if (strcmp(names[i].n_str, name) == 0)
			return (names[i].n_name);
	}
	return (-1);
}

static struct syscall_desc *
find_syscall(const char *name)
{
	int i;

	for (i = 0; syscalls[i].sd_name != NULL; i++) {
		if (strcmp(syscalls[i].sd_name, name) == 0)
			return (&syscalls[i]);
	}
	return (NULL);
}

static void
show_stat(struct stat64 *sp, const char *what)
{

	if (strcmp(what, "mode") == 0)
		printf("0%o", (unsigned int)(sp->st_mode & ALLPERMS));
	else if (strcmp(what, "inode") == 0)
		printf("%lld", (long long)sp->st_ino);
	else if (strcmp(what, "nlink") == 0)
		printf("%lld", (long long)sp->st_nlink);
	else if (strcmp(what, "uid") == 0)
		printf("%d", (int)sp->st_uid);
	else if (strcmp(what, "gid") == 0)
		printf("%d", (int)sp->st_gid);
	else if (strcmp(what, "size") == 0)
		printf("%lld", (long long)sp->st_size);
	else if (strcmp(what, "blocks") == 0)
		printf("%lld", (long long)sp->st_blocks);
	else if (strcmp(what, "atime") == 0)
		printf("%lld", (long long)sp->st_atime);
	else if (strcmp(what, "mtime") == 0)
		printf("%lld", (long long)sp->st_mtime);
	else if (strcmp(what, "ctime") == 0)
		printf("%lld", (long long)sp->st_ctime);
#ifdef HAS_CHFLAGS
	else if (strcmp(what, "flags") == 0)
		printf("%s", flags2str(chflags_flags, (long long)sp->st_flags));
#endif
	else if (strcmp(what, "major") == 0)
		printf("%u", (unsigned int)major(sp->st_rdev));
	else if (strcmp(what, "minor") == 0)
		printf("%u", (unsigned int)minor(sp->st_rdev));
	else if (strcmp(what, "type") == 0) {
		switch (sp->st_mode & S_IFMT) {
		case S_IFIFO:
			printf("fifo");
			break;
		case S_IFCHR:
			printf("char");
			break;
		case S_IFDIR:
			printf("dir");
			break;
		case S_IFBLK:
			printf("block");
			break;
		case S_IFREG:
			printf("regular");
			break;
		case S_IFLNK:
			printf("symlink");
			break;
		case S_IFSOCK:
			printf("socket");
			break;
		default:
			printf("unknown");
			break;
		}
	} else {
		printf("unknown");
	}
}

static void
show_stats(struct stat64 *sp, char *what)
{
	const char *s = "";
	char *w;

	for (w = strtok(what, ","); w != NULL; w = strtok(NULL, ",")) {
		printf("%s", s);
		show_stat(sp, w);
		s = ",";
	}
	printf("\n");
}

static unsigned int
call_syscall(struct syscall_desc *scall, char *argv[])
{
	struct stat64 sb;
	long long flags;
	unsigned int i;
	char *endp;
	int name, rval;
	union {
		char *str;
		long long num;
	} args[MAX_ARGS];
#ifdef HAS_FREEBSD_ACL
	int entry_id = ACL_FIRST_ENTRY;
	acl_t acl, newacl;
	acl_entry_t entry, newentry;
#endif

	/*
	 * Verify correctness of the arguments.
	 */
	for (i = 0; i < sizeof(args)/sizeof(args[0]); i++) {
		if (scall->sd_args[i] == TYPE_NONE) {
			if (argv[i] == NULL || strcmp(argv[i], ":") == 0)
				break;
			fprintf(stderr, "too many arguments [%s]\n", argv[i]);
			exit(1);
		} else {
			if (argv[i] == NULL || strcmp(argv[i], ":") == 0) {
				if (scall->sd_args[i] & TYPE_OPTIONAL)
					break;
				fprintf(stderr, "too few arguments\n");
				exit(1);
			}
			if (scall->sd_args[i] & TYPE_STRING) {
				if (strcmp(argv[i], "NULL") == 0)
					args[i].str = NULL;
				else if (strcmp(argv[i], "DEADCODE") == 0)
					args[i].str = (void *)0xdeadc0de;
				else
					args[i].str = argv[i];
			} else if (scall->sd_args[i] & TYPE_NUMBER) {
				args[i].num = strtoll(argv[i], &endp, 0);
				if (*endp != '\0' && !isspace((unsigned char)*endp)) {
					fprintf(stderr, "invalid argument %u, number expected [%s]\n", i, endp);
					exit(1);
				}
			}
		}
	}
	/*
	 * Call the given syscall.
	 */
#define	NUM(n)	(args[(n)].num)
#define	STR(n)	(args[(n)].str)
	switch (scall->sd_action) {
	case ACTION_OPEN:
		flags = str2flags(open_flags, STR(1));
		if (flags & O_CREAT) {
			if (i == 2) {
				fprintf(stderr, "too few arguments\n");
				exit(1);
			}
			rval = open(STR(0), (int)flags, (mode_t)NUM(2));
		} else {
			if (i == 3) {
				fprintf(stderr, "too many arguments\n");
				exit(1);
			}
			rval = open(STR(0), (int)flags);
		}
		break;
	case ACTION_CREATE:
		rval = open(STR(0), O_CREAT | O_EXCL, (mode_t)NUM(1));
		if (rval >= 0)
			close(rval);
		break;
	case ACTION_UNLINK:
		rval = unlink(STR(0));
		break;
	case ACTION_MKDIR:
		rval = mkdir(STR(0), (mode_t)NUM(1));
		break;
	case ACTION_RMDIR:
		rval = rmdir(STR(0));
		break;
	case ACTION_LINK:
		rval = link(STR(0), STR(1));
		break;
	case ACTION_SYMLINK:
		rval = symlink(STR(0), STR(1));
		break;
	case ACTION_RENAME:
		rval = rename(STR(0), STR(1));
		break;
	case ACTION_MKFIFO:
		rval = mkfifo(STR(0), (mode_t)NUM(1));
		break;
	case ACTION_MKNOD:
	    {
		mode_t ntype;
		dev_t dev;

		dev = makedev(NUM(3), NUM(4));
		if (strcmp(STR(1), "c") == 0)		/* character device */
			ntype = S_IFCHR;
		else if (strcmp(STR(1), "b") == 0)	/* block device */
			ntype = S_IFBLK;
		else if (strcmp(STR(1), "f") == 0)	/* fifo special */
			ntype = S_IFIFO;
		else if (strcmp(STR(1), "d") == 0)	/* directory */
			ntype = S_IFDIR;
		else if (strcmp(STR(1), "o") == 0)	/* regular file */
			ntype = S_IFREG;
		else {
			fprintf(stderr, "wrong argument 1\n");
			exit(1);
		}
		rval = mknod(STR(0), ntype | NUM(2), dev);
		break;
	    }
	case ACTION_BIND:
	    {
		struct sockaddr_un sunx;

		sunx.sun_family = AF_UNIX;
		strncpy(sunx.sun_path, STR(0), sizeof(sunx.sun_path) - 1);
		sunx.sun_path[sizeof(sunx.sun_path) - 1] = '\0';
		rval = socket(AF_UNIX, SOCK_STREAM, 0);
		if (rval < 0)
			break;
		rval = bind(rval, (struct sockaddr *)&sunx, sizeof(sunx));
		break;
	    }
	case ACTION_CONNECT:
	    {
		struct sockaddr_un sunx;

		sunx.sun_family = AF_UNIX;
		strncpy(sunx.sun_path, STR(0), sizeof(sunx.sun_path) - 1);
		sunx.sun_path[sizeof(sunx.sun_path) - 1] = '\0';
		rval = socket(AF_UNIX, SOCK_STREAM, 0);
		if (rval < 0)
			break;
		rval = connect(rval, (struct sockaddr *)&sunx, sizeof(sunx));
		break;
	    }
	case ACTION_CHMOD:
		rval = chmod(STR(0), (mode_t)NUM(1));
		break;
#ifdef HAS_LCHMOD
	case ACTION_LCHMOD:
		rval = lchmod(STR(0), (mode_t)NUM(1));
		break;
#endif
	case ACTION_CHOWN:
		rval = chown(STR(0), (uid_t)NUM(1), (gid_t)NUM(2));
		break;
	case ACTION_LCHOWN:
		rval = lchown(STR(0), (uid_t)NUM(1), (gid_t)NUM(2));
		break;
#ifdef HAS_CHFLAGS
	case ACTION_CHFLAGS:
		rval = chflags(STR(0), (unsigned long)str2flags(chflags_flags, STR(1)));
		break;
#endif
#ifdef HAS_LCHFLAGS
	case ACTION_LCHFLAGS:
		rval = lchflags(STR(0), (int)str2flags(chflags_flags, STR(1)));
		break;
#endif
	case ACTION_TRUNCATE:
		rval = truncate64(STR(0), NUM(1));
		break;
	case ACTION_STAT:
		rval = stat64(STR(0), &sb);
		if (rval == 0) {
			show_stats(&sb, STR(1));
			return (i);
		}
		break;
	case ACTION_LSTAT:
		rval = lstat64(STR(0), &sb);
		if (rval == 0) {
			show_stats(&sb, STR(1));
			return (i);
		}
		break;
	case ACTION_PATHCONF:
	    {
		long lrval;

		name = str2name(pathconf_names, STR(1));
		if (name == -1) {
			fprintf(stderr, "unknown name %s", STR(1));
			exit(1);
		}
		errno = 0;
		lrval = pathconf(STR(0), name);
		if (lrval == -1 && errno == 0) {
			printf("unlimited\n");
			return (i);
		} else if (lrval >= 0) {
			printf("%ld\n", lrval);
			return (i);
		}
		rval = -1;
		break;
	    }
#ifdef HAS_FREEBSD_ACL
	case ACTION_PREPENDACL:
		rval = -1;

		acl = acl_get_file(STR(0), ACL_TYPE_NFS4);
		if (acl == NULL)
			break;

		newacl = acl_from_text(STR(1));
		if (acl == NULL)
			break;

		while (acl_get_entry(newacl, entry_id, &newentry) == 1) {
			entry_id = ACL_NEXT_ENTRY;

			if (acl_create_entry_np(&acl, &entry, 0))
				break;

			if (acl_copy_entry(entry, newentry))
				break;
		}

		rval = acl_set_file(STR(0), ACL_TYPE_NFS4, acl);
		break;

	case ACTION_READACL:
		acl = acl_get_file(STR(0), ACL_TYPE_NFS4);
		if (acl == NULL)
			rval = -1;
		else
			rval = 0;
		break;
#endif

	case ACTION_WRITE:
		rval = open(STR(0), O_WRONLY);
		if (rval < 0)
			break;

		rval = write(rval, "x", 1);
		break;

	default:
		fprintf(stderr, "unsupported syscall\n");
		exit(1);
	}
#undef STR
#undef NUM
	if (rval < 0) {
		const char *serrno;

		serrno = err2str(errno);
		fprintf(stderr, "%s returned %d\n", scall->sd_name, rval);
		printf("%s\n", serrno);
		exit(1);
	}
	printf("0\n");
	return (i);
}

static void
set_gids(char *gids)
{
	gid_t *gidset;
	long ngroups;
	char *g, *endp;
	unsigned i;

	ngroups = sysconf(_SC_NGROUPS_MAX);
	assert(ngroups > 0);
	gidset = malloc(sizeof(*gidset) * ngroups);
	assert(gidset != NULL);
	for (i = 0, g = strtok(gids, ","); g != NULL; g = strtok(NULL, ","), i++) {
		if (i >= ngroups) {
			fprintf(stderr, "too many gids\n");
			exit(1);
		}
		gidset[i] = strtol(g, &endp, 0);
		if (*endp != '\0' && !isspace((unsigned char)*endp)) {
			fprintf(stderr, "invalid gid '%s' - number expected\n",
			    g);
			exit(1);
		}
	}
	if (setgroups(i, gidset) < 0) {
		fprintf(stderr, "cannot change groups: %s\n", strerror(errno));
		exit(1);
	}
	if (setegid(gidset[0]) < 0) {
		fprintf(stderr, "cannot change effective gid: %s\n", strerror(errno));
		exit(1);
	}
	free(gidset);
}

int
main(int argc, char *argv[])
{
	struct syscall_desc *scall;
	unsigned int n;
	char *gids, *endp;
	int uid, umsk, ch;

	uid = -1;
	gids = NULL;
	umsk = 0;

	while ((ch = getopt(argc, argv, "g:u:U:")) != -1) {
		switch(ch) {
		case 'g':
			gids = optarg;
			break;
		case 'u':
			uid = (int)strtol(optarg, &endp, 0);
			if (*endp != '\0' && !isspace((unsigned char)*endp)) {
				fprintf(stderr, "invalid uid '%s' - number "
				    "expected\n", optarg);
				exit(1);
			}
			break;
		case 'U':
			umsk = (int)strtol(optarg, &endp, 0);
			if (*endp != '\0' && !isspace((unsigned char)*endp)) {
				fprintf(stderr, "invalid umask '%s' - number "
				    "expected\n", optarg);
				exit(1);
			}
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		fprintf(stderr, "too few arguments\n");
		usage();
	}

	if (gids != NULL) {
		fprintf(stderr, "changing groups to %s\n", gids);
		set_gids(gids);
	}
	if (uid != -1) {
		fprintf(stderr, "changing uid to %d\n", uid);
		if (setuid(uid) < 0) {
			fprintf(stderr, "cannot change uid: %s\n",
			    strerror(errno));
			exit(1);
		}
	}

	/* Change umask to requested value or to 0, if not requested. */
	umask(umsk);

	for (;;) {
		scall = find_syscall(argv[0]);
		if (scall == NULL) {
			fprintf(stderr, "syscall '%s' not supported\n", argv[0]);
			exit(1);
		}
		argc++;
		argv++;
		n = call_syscall(scall, argv);
		argc += n;
		argv += n;
		if (argv[0] == NULL)
			break;
		argc++;
		argv++;
	}

	exit(0);
}

static const char *
err2str(int error)
{
	static char errnum[8];

	switch (error) {
#ifdef EPERM
	case EPERM:
		return ("EPERM");
#endif
#ifdef ENOENT
	case ENOENT:
		return ("ENOENT");
#endif
#ifdef ESRCH
	case ESRCH:
		return ("ESRCH");
#endif
#ifdef EINTR
	case EINTR:
		return ("EINTR");
#endif
#ifdef EIO
	case EIO:
		return ("EIO");
#endif
#ifdef ENXIO
	case ENXIO:
		return ("ENXIO");
#endif
#ifdef E2BIG
	case E2BIG:
		return ("E2BIG");
#endif
#ifdef ENOEXEC
	case ENOEXEC:
		return ("ENOEXEC");
#endif
#ifdef EBADF
	case EBADF:
		return ("EBADF");
#endif
#ifdef ECHILD
	case ECHILD:
		return ("ECHILD");
#endif
#ifdef EDEADLK
	case EDEADLK:
		return ("EDEADLK");
#endif
#ifdef ENOMEM
	case ENOMEM:
		return ("ENOMEM");
#endif
#ifdef EACCES
	case EACCES:
		return ("EACCES");
#endif
#ifdef EFAULT
	case EFAULT:
		return ("EFAULT");
#endif
#ifdef ENOTBLK
	case ENOTBLK:
		return ("ENOTBLK");
#endif
#ifdef EBUSY
	case EBUSY:
		return ("EBUSY");
#endif
#ifdef EEXIST
	case EEXIST:
		return ("EEXIST");
#endif
#ifdef EXDEV
	case EXDEV:
		return ("EXDEV");
#endif
#ifdef ENODEV
	case ENODEV:
		return ("ENODEV");
#endif
#ifdef ENOTDIR
	case ENOTDIR:
		return ("ENOTDIR");
#endif
#ifdef EISDIR
	case EISDIR:
		return ("EISDIR");
#endif
#ifdef EINVAL
	case EINVAL:
		return ("EINVAL");
#endif
#ifdef ENFILE
	case ENFILE:
		return ("ENFILE");
#endif
#ifdef EMFILE
	case EMFILE:
		return ("EMFILE");
#endif
#ifdef ENOTTY
	case ENOTTY:
		return ("ENOTTY");
#endif
#ifdef ETXTBSY
	case ETXTBSY:
		return ("ETXTBSY");
#endif
#ifdef EFBIG
	case EFBIG:
		return ("EFBIG");
#endif
#ifdef ENOSPC
	case ENOSPC:
		return ("ENOSPC");
#endif
#ifdef ESPIPE
	case ESPIPE:
		return ("ESPIPE");
#endif
#ifdef EROFS
	case EROFS:
		return ("EROFS");
#endif
#ifdef EMLINK
	case EMLINK:
		return ("EMLINK");
#endif
#ifdef EPIPE
	case EPIPE:
		return ("EPIPE");
#endif
#ifdef EDOM
	case EDOM:
		return ("EDOM");
#endif
#ifdef ERANGE
	case ERANGE:
		return ("ERANGE");
#endif
#ifdef EAGAIN
	case EAGAIN:
		return ("EAGAIN");
#endif
#ifdef EINPROGRESS
	case EINPROGRESS:
		return ("EINPROGRESS");
#endif
#ifdef EALREADY
	case EALREADY:
		return ("EALREADY");
#endif
#ifdef ENOTSOCK
	case ENOTSOCK:
		return ("ENOTSOCK");
#endif
#ifdef EDESTADDRREQ
	case EDESTADDRREQ:
		return ("EDESTADDRREQ");
#endif
#ifdef EMSGSIZE
	case EMSGSIZE:
		return ("EMSGSIZE");
#endif
#ifdef EPROTOTYPE
	case EPROTOTYPE:
		return ("EPROTOTYPE");
#endif
#ifdef ENOPROTOOPT
	case ENOPROTOOPT:
		return ("ENOPROTOOPT");
#endif
#ifdef EPROTONOSUPPORT
	case EPROTONOSUPPORT:
		return ("EPROTONOSUPPORT");
#endif
#ifdef ESOCKTNOSUPPORT
	case ESOCKTNOSUPPORT:
		return ("ESOCKTNOSUPPORT");
#endif
#ifdef EOPNOTSUPP
	case EOPNOTSUPP:
		return ("EOPNOTSUPP");
#endif
#ifdef EPFNOSUPPORT
	case EPFNOSUPPORT:
		return ("EPFNOSUPPORT");
#endif
#ifdef EAFNOSUPPORT
	case EAFNOSUPPORT:
		return ("EAFNOSUPPORT");
#endif
#ifdef EADDRINUSE
	case EADDRINUSE:
		return ("EADDRINUSE");
#endif
#ifdef EADDRNOTAVAIL
	case EADDRNOTAVAIL:
		return ("EADDRNOTAVAIL");
#endif
#ifdef ENETDOWN
	case ENETDOWN:
		return ("ENETDOWN");
#endif
#ifdef ENETUNREACH
	case ENETUNREACH:
		return ("ENETUNREACH");
#endif
#ifdef ENETRESET
	case ENETRESET:
		return ("ENETRESET");
#endif
#ifdef ECONNABORTED
	case ECONNABORTED:
		return ("ECONNABORTED");
#endif
#ifdef ECONNRESET
	case ECONNRESET:
		return ("ECONNRESET");
#endif
#ifdef ENOBUFS
	case ENOBUFS:
		return ("ENOBUFS");
#endif
#ifdef EISCONN
	case EISCONN:
		return ("EISCONN");
#endif
#ifdef ENOTCONN
	case ENOTCONN:
		return ("ENOTCONN");
#endif
#ifdef ESHUTDOWN
	case ESHUTDOWN:
		return ("ESHUTDOWN");
#endif
#ifdef ETOOMANYREFS
	case ETOOMANYREFS:
		return ("ETOOMANYREFS");
#endif
#ifdef ETIMEDOUT
	case ETIMEDOUT:
		return ("ETIMEDOUT");
#endif
#ifdef ECONNREFUSED
	case ECONNREFUSED:
		return ("ECONNREFUSED");
#endif
#ifdef ELOOP
	case ELOOP:
		return ("ELOOP");
#endif
#ifdef ENAMETOOLONG
	case ENAMETOOLONG:
		return ("ENAMETOOLONG");
#endif
#ifdef EHOSTDOWN
	case EHOSTDOWN:
		return ("EHOSTDOWN");
#endif
#ifdef EHOSTUNREACH
	case EHOSTUNREACH:
		return ("EHOSTUNREACH");
#endif
#ifdef ENOTEMPTY
	case ENOTEMPTY:
		return ("ENOTEMPTY");
#endif
#ifdef EPROCLIM
	case EPROCLIM:
		return ("EPROCLIM");
#endif
#ifdef EUSERS
	case EUSERS:
		return ("EUSERS");
#endif
#ifdef EDQUOT
	case EDQUOT:
		return ("EDQUOT");
#endif
#ifdef ESTALE
	case ESTALE:
		return ("ESTALE");
#endif
#ifdef EREMOTE
	case EREMOTE:
		return ("EREMOTE");
#endif
#ifdef EBADRPC
	case EBADRPC:
		return ("EBADRPC");
#endif
#ifdef ERPCMISMATCH
	case ERPCMISMATCH:
		return ("ERPCMISMATCH");
#endif
#ifdef EPROGUNAVAIL
	case EPROGUNAVAIL:
		return ("EPROGUNAVAIL");
#endif
#ifdef EPROGMISMATCH
	case EPROGMISMATCH:
		return ("EPROGMISMATCH");
#endif
#ifdef EPROCUNAVAIL
	case EPROCUNAVAIL:
		return ("EPROCUNAVAIL");
#endif
#ifdef ENOLCK
	case ENOLCK:
		return ("ENOLCK");
#endif
#ifdef ENOSYS
	case ENOSYS:
		return ("ENOSYS");
#endif
#ifdef EFTYPE
	case EFTYPE:
		return ("EFTYPE");
#endif
#ifdef EAUTH
	case EAUTH:
		return ("EAUTH");
#endif
#ifdef ENEEDAUTH
	case ENEEDAUTH:
		return ("ENEEDAUTH");
#endif
#ifdef EIDRM
	case EIDRM:
		return ("EIDRM");
#endif
#ifdef ENOMSG
	case ENOMSG:
		return ("ENOMSG");
#endif
#ifdef EOVERFLOW
	case EOVERFLOW:
		return ("EOVERFLOW");
#endif
#ifdef ECANCELED
	case ECANCELED:
		return ("ECANCELED");
#endif
#ifdef EILSEQ
	case EILSEQ:
		return ("EILSEQ");
#endif
#ifdef ENOATTR
	case ENOATTR:
		return ("ENOATTR");
#endif
#ifdef EDOOFUS
	case EDOOFUS:
		return ("EDOOFUS");
#endif
#ifdef EBADMSG
	case EBADMSG:
		return ("EBADMSG");
#endif
#ifdef EMULTIHOP
	case EMULTIHOP:
		return ("EMULTIHOP");
#endif
#ifdef ENOLINK
	case ENOLINK:
		return ("ENOLINK");
#endif
#ifdef EPROTO
	case EPROTO:
		return ("EPROTO");
#endif
	default:
		snprintf(errnum, sizeof(errnum), "%d", error);
		return (errnum);
	}
}
