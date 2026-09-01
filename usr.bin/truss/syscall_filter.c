/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 */

/*
 * Selection of the system calls to report, driven by -t.
 */

#include <sys/param.h>
#include <sys/queue.h>

#include <err.h>
#include <fnmatch.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysdecode.h>

#include "truss.h"
#include "extern.h"

/*
 * Groups of related system calls, named as "@group" in a -t expression.
 *
 * A group's member list is an expression in exactly the form -t accepts,
 * so that anything a user can write on the command line can also be
 * written as a group: each member is an fnmatch(3) pattern matched
 * against the name of a system call, a decimal system call number, or
 * "@group" naming another group, and any of them may be prefixed with
 * '!' to exclude rather than include what it matches.  Keeping the two
 * languages identical is deliberate: a group defined elsewhere, from a
 * -t expression a user supplied, needs no translation to become a
 * member list here.
 *
 * Patterns are preferred over literal names wherever a family of system
 * calls shares a naming convention (e.g. "extattr_*_file"), so that
 * system calls added later are picked up without further change here.
 * Names from the ABIs truss supports beyond the native one are included
 * where they differ, since the Linux ABI in particular renames a number
 * of otherwise familiar system calls.
 *
 * Adding a group is a matter of adding a member list here and a single
 * entry to syscall_groups[] below.
 */

static const char *const group_all[] = {
	"*",
	NULL
};

static const char *const group_none[] = {
	"!*",
	NULL
};

static const char *const group_read[] = {
	"read", "readv", "pread*", "readahead", "readdir",
	"recv", "recvfrom", "recvmsg", "recvmmsg*",
	"aio_read*", "sctp_generic_recvmsg",
	"kmq_timedreceive", "mq_timedreceive*", "msgrcv",
	"getdents*", "getdirentries", "copy_file_range", "process_vm_readv",
	NULL
};

static const char *const group_write[] = {
	"write", "writev", "pwrite*",
	"send", "sendto", "sendmsg", "sendmmsg*", "sendfile*",
	"aio_write*", "sctp_generic_send*",
	"kmq_timedsend", "mq_timedsend*", "msgsnd",
	"copy_file_range", "process_vm_writev",
	NULL
};

static const char *const group_desc[] = {
	"@read", "@write",
	"close", "close_range", "closefrom", "dup", "dup2", "dup3",
	"fcntl", "fcntl64", "flock", "fsync", "fdatasync", "syncfs",
	"sync", "sync_file_range", "ftruncate*", "lseek", "llseek", "ioctl",
	"poll", "ppoll*", "select", "old_select", "pselect*",
	"epoll_*", "kqueue*", "kevent*", "eventfd*", "timerfd_*",
	"inotify_*", "signalfd*", "fanotify_*", "pipe", "pipe2",
	"fstat", "fstat64", "newfstat", "nfstat", "fstatfs*",
	"fchdir", "fchmod", "fchown", "fchflags", "futimes", "futimens",
	"fpathconf", "getdtablesize", "fexecve", "fspacectl",
	"posix_fadvise", "fadvise64*", "posix_fallocate", "fallocate",
	"splice", "tee", "vmsplice", "memfd_create", "__specialfd",
	"posix_openpt", "pddupfd", "aio_*", "lio_listio",
	"pidfd_*", "io_*", "f*xattr",
	"__acl_*_fd", "extattr_*_fd", "__mac_*_fd",
	"cap_fcntls_*", "cap_ioctls_*", "cap_rights_limit",
	"__cap_rights_get",
	NULL
};

static const char *const group_file[] = {
	"open", "openat", "openat2", "open_by_handle_at", "open_tree",
	"creat", "stat", "stat64", "newstat", "nstat", "statx",
	"lstat", "lstat64", "newlstat", "nlstat",
	"fstatat", "fstatat64", "newfstatat", "statfs", "statfs64",
	"access", "eaccess", "faccessat*",
	"chdir", "chroot", "fchroot", "pivot_root",
	"chmod", "lchmod", "fchmodat*",
	"chown", "chown16", "lchown", "lchown16", "fchownat",
	"chflags", "lchflags", "chflagsat",
	"link", "linkat", "symlink", "symlinkat",
	"unlink", "unlinkat", "funlinkat",
	"rename", "renameat", "renameat2",
	"mkdir", "mkdirat", "rmdir", "mknod", "mknodat",
	"mkfifo", "mkfifoat", "readlink", "readlinkat",
	"truncate", "truncate64",
	"utime", "utimes", "lutimes", "utimensat*", "futimesat",
	"pathconf", "lpathconf", "__getcwd", "getcwd", "__realpathat",
	"revoke", "undelete", "acct", "quotactl*", "umask",
	"mount", "nmount", "unmount", "umount", "oldumount", "move_mount",
	"fh*", "getfh", "getfhat", "lgetfh", "getfsstat",
	"name_to_handle_at", "inotify_add_watch*",
	"execve", "execveat", "__mac_execve",
	"swapon", "swapoff", "kldload",
	"getxattr", "lgetxattr", "setxattr", "lsetxattr",
	"removexattr", "lremovexattr", "listxattr", "llistxattr",
	"__acl_*_file", "__acl_*_link", "extattr_*_file", "extattr_*_link",
	"extattrctl", "__mac_*_file", "__mac_*_link",
	NULL
};

static const char *const group_net[] = {
	"socket", "socketcall", "socketpair", "bind", "bindat",
	"connect", "connectat", "listen", "accept", "accept4",
	"getpeername", "getsockname", "getsockopt", "setsockopt", "shutdown",
	"send", "sendto", "sendmsg", "sendmmsg*", "sendfile*",
	"recv", "recvfrom", "recvmsg", "recvmmsg*", "sctp_*",
	"setfib", "gethostname", "sethostname",
	"getdomainname", "setdomainname",
	"nfssvc", "nlm_syscall", "rpctls_syscall",
	NULL
};

static const char *const group_proc[] = {
	"fork", "vfork", "rfork", "pdfork", "pdrfork", "clone", "clone3",
	"execve", "execveat", "fexecve", "__mac_execve",
	"_exit", "exit", "exit_group", "abort2",
	"wait", "wait4", "wait6", "waitid", "waitpid", "pdwait",
	"getpid", "getppid", "gettid", "getpgrp", "getpgid", "setpgid",
	"getsid", "setsid", "getpriority", "setpriority", "nice",
	"rtprio", "rtprio_thread", "sched_*", "cpuset*",
	"procctl", "prctl", "arch_prctl", "ptrace",
	"thr_*", "_umtx_*", "futex*", "sys_futex*", "membarrier",
	"pdgetpid", "pdkill", "pdopenpid", "pidfd_open", "pidfd_getfd",
	"jail*", "kcmp", "getcontext", "setcontext", "swapcontext", "yield",
	"getrusage", "getrlimit", "setrlimit", "getrlimitusage",
	"old_getrlimit", "prlimit64", "personality", "times", "vhangup",
	"set_tid_address", "setns", "unshare", "restart_syscall", "rseq",
	"get_robust_list", "set_robust_list",
	NULL
};

static const char *const group_signal[] = {
	"sig*", "rt_sig*", "rt_tgsigqueueinfo",
	"kill", "killpg", "thr_kill*", "pdkill",
	"tkill", "tgkill", "pidfd_send_signal",
	"sgetmask", "ssetmask", "pause",
	NULL
};

static const char *const group_memory[] = {
	"mmap", "mmap2", "munmap", "mprotect", "pkey_mprotect", "mremap",
	"madvise", "process_madvise", "mincore", "minherit",
	"mlock", "mlock2", "munlock", "mlockall", "munlockall", "aio_mlock",
	"msync", "break", "brk", "sbrk", "vadvise", "getpagesize",
	"shm_open*", "shm_unlink", "shm_rename",
	"memfd_create", "memfd_secret", "map_shadow_stack", "userfaultfd",
	"mbind", "get_mempolicy", "set_mempolicy*",
	"migrate_pages", "move_pages", "remap_file_pages",
	"pkey_alloc", "pkey_free", "swapon", "swapoff",
	NULL
};

static const char *const group_ipc[] = {
	"msgctl", "msgget", "msgrcv", "msgsnd", "msgsys",
	"semctl", "__semctl", "semget", "semop", "semsys", "semtimedop*",
	"shmat", "shmctl", "shmdt", "shmget", "shmsys",
	"ksem_*", "kmq_*", "mq_*", "ipc",
	NULL
};

static const char *const group_creds[] = {
	"getuid*", "geteuid*", "getgid*", "getegid*",
	"getgroups*", "setgroups*",
	"setuid*", "seteuid", "setgid*", "setegid",
	"setreuid*", "setregid*", "setresuid*", "setresgid*",
	"getresuid*", "getresgid*", "setfsuid*", "setfsgid*",
	"issetugid", "__setugid", "setcred",
	"getlogin", "setlogin", "getloginclass", "setloginclass",
	"getauid", "setauid", "getaudit*", "setaudit*",
	"audit", "auditon", "auditctl",
	"capget", "capset", "cap_enter", "cap_getmode",
	"seccomp", "landlock_*",
	NULL
};

static const char *const group_time[] = {
	"clock_*", "nanosleep", "gettimeofday", "settimeofday",
	"adjtime", "adjtimex", "ntp_*",
	"getitimer", "setitimer", "ktimer_*", "timer_*", "timerfd_*",
	"ffclock_*", "time", "stime", "alarm",
	NULL
};

struct syscall_group {
	const char *name;
	const char *desc;
	const char *const *members;
};

/* Kept in alphabetical order; "truss -t" prints it as-is. */
static const struct syscall_group syscall_groups[] = {
	{ "all",    "every system call",			group_all },
	{ "creds",  "get or set process credentials",		group_creds },
	{ "desc",   "operate on a file descriptor",		group_desc },
	{ "file",   "operate on a pathname",			group_file },
	{ "ipc",    "System V and POSIX IPC",			group_ipc },
	{ "memory", "memory mapping and locking",		group_memory },
	{ "net",    "network and socket operations",		group_net },
	{ "none",   "no system call",				group_none },
	{ "proc",   "process and thread lifecycle",		group_proc },
	{ "read",   "read data from a descriptor",		group_read },
	{ "signal", "signal delivery and handling",		group_signal },
	{ "time",   "clocks, timers and sleeping",		group_time },
	{ "write",  "write data to a descriptor",		group_write },
};

/*
 * One comma-separated term of a -t expression.  Terms are held in the
 * order they were given: the last one to match a system call decides
 * whether it is reported.
 */
struct filter_term {
	STAILQ_ENTRY(filter_term) entries;
	const struct syscall_group *group; /* @group term, else NULL */
	char *pattern;			   /* name pattern, else NULL */
	u_int number;			   /* number, if by_number */
	bool by_number;
	bool negate;
};

static bool term_matches_any_syscall(const struct filter_term *);

static STAILQ_HEAD(, filter_term) filter_terms =
    STAILQ_HEAD_INITIALIZER(filter_terms);

/*
 * Whether a system call matched by no term at all is reported.  An
 * expression made up only of negated terms subtracts from the full set
 * of system calls; any other expression selects from an empty one.
 */
static bool filter_default = true;

/*
 * A name reported by sysdecode may carry a prefix naming a compatibility
 * layer ("compat11.stat"), a non-native ABI ("linux_open",
 * "freebsd32_ioctl"), or both ("compat4.freebsd32_getfsstat").  Terms are
 * matched against the name as displayed and against each shortened form,
 * so that "-t @file" selects stat, compat11.stat and freebsd32_stat alike.
 */
static const char *const abi_prefixes[] = {
	"freebsd32_",
	"linux_",
	"linux32_",
};

/*
 * A group referring to itself, directly or through others, would recurse
 * forever.  The table above has no such cycle; this only keeps a future
 * mistake in it from hanging truss.
 */
#define	GROUP_MAX_DEPTH	8

static const struct syscall_group *
find_group(const char *name)
{
	size_t i;

	for (i = 0; i < nitems(syscall_groups); i++) {
		if (strcmp(name, syscall_groups[i].name) == 0)
			return (&syscall_groups[i]);
	}
	return (NULL);
}

static bool group_selects(const struct syscall_group *, const char *, u_int,
    u_int);

static const char *
strip_abi_prefix(const char *name)
{
	size_t i, len;

	for (i = 0; i < nitems(abi_prefixes); i++) {
		len = strlen(abi_prefixes[i]);
		if (strncmp(name, abi_prefixes[i], len) == 0)
			return (name + len);
	}
	return (name);
}

/*
 * Expand a system call name into the forms a term may match it under:
 * the name itself, the name with any "compatN." prefix removed, and that
 * with any ABI prefix removed as well.  Returns the number of forms.
 */
static u_int
name_forms(const char *name, const char *forms[3])
{
	const char *shorter, *stripped;
	u_int nforms;

	nforms = 0;
	forms[nforms++] = name;
	shorter = strrchr(name, '.');
	if (shorter != NULL)
		forms[nforms++] = ++shorter;
	else
		shorter = name;
	stripped = strip_abi_prefix(shorter);
	if (stripped != shorter)
		forms[nforms++] = stripped;
	return (nforms);
}

/*
 * Whether one member of a group selects the given system call.  The
 * caller has already consumed any leading '!', leaving the same three
 * forms a -t term may take: a "@group" reference, a decimal system call
 * number, or an fnmatch(3) pattern matched against the name.
 */
static bool
member_matches(const char *member, const char *name, u_int number, u_int depth)
{
	const struct syscall_group *ref;
	const char *errstr;
	const char *forms[3];
	u_int i, nforms, num;

	/*
	 * A member of "!" alone leaves nothing behind once the caller has
	 * consumed the '!'.  The -t parser rejects that outright; say so
	 * explicitly here rather than falling into the numeric branch,
	 * where an empty string would otherwise be offered to strtonum().
	 */
	if (*member == '\0')
		return (false);

	if (*member == '@') {
		ref = find_group(member + 1);
		return (ref != NULL &&
		    group_selects(ref, name, number, depth + 1));
	}
	if (member[strspn(member, "0123456789")] == '\0') {
		num = (u_int)strtonum(member, 0, UINT_MAX, &errstr);
		return (errstr == NULL && num == number);
	}
	nforms = name_forms(name, forms);
	for (i = 0; i < nforms; i++) {
		if (fnmatch(member, forms[i], 0) == 0)
			return (true);
	}
	return (false);
}

/*
 * Whether a group selects the given system call.
 *
 * A group's member list is an expression in exactly the form -t accepts,
 * so that a group can say anything a user can say on the command line:
 * members apply in order, the last one to match decides, and a list of
 * only negated members starts from every system call rather than from
 * none.  "@none" is therefore written as the one member "!*".
 */
static bool
group_selects(const struct syscall_group *group, const char *name, u_int number,
    u_int depth)
{
	const char *const *member;
	const char *pattern;
	bool negate, selects;

	/*
	 * A group with no member list at all selects nothing.  The table
	 * below has no such entry, but a group built from anywhere less
	 * hand-audited should not be able to fault truss.
	 */
	if (depth >= GROUP_MAX_DEPTH || group->members == NULL)
		return (false);

	selects = true;
	for (member = group->members; *member != NULL; member++) {
		if (**member != '!') {
			selects = false;
			break;
		}
	}

	for (member = group->members; *member != NULL; member++) {
		pattern = *member;
		negate = *pattern == '!';
		if (negate)
			pattern++;
		if (member_matches(pattern, name, number, depth))
			selects = !negate;
	}
	return (selects);
}

/* Print the group table ("-t" with no expression). */
void
list_syscall_groups(void)
{
	size_t i;

	printf("System call groups usable as @group in a -t expression:\n\n");
	for (i = 0; i < nitems(syscall_groups); i++)
		printf("    @%-9s %s\n", syscall_groups[i].name,
		    syscall_groups[i].desc);
	printf("\n"
	    "Any other term is an fnmatch(3) pattern matched against the\n"
	    "system call name, so \"read\" selects read(2) alone and \"read*\"\n"
	    "also selects readv(2) and readlink(2).  A term prefixed with '!'\n"
	    "excludes what that one term matches rather than including it.\n");
}

/*
 * Add the terms of one -t expression.  Repeating -t appends to the
 * expression rather than replacing it.
 */
void
add_syscall_filter(const char *expr)
{
	struct filter_term *term;
	const char *errstr;
	char *copy, *next, *word;

	if ((copy = strdup(expr)) == NULL)
		err(1, "strdup");
	next = copy;
	while ((word = strsep(&next, ",")) != NULL) {
		bool negate = false;

		if (*word == '!') {
			negate = true;
			if (*++word == '\0')
				errx(1, "missing pattern after '!' in -t %s",
				    expr);
		}

		/*
		 * Ignore an empty term so that an empty expression, or one
		 * with a stray or trailing comma, adds no terms rather than
		 * being an error.  "truss -t ''" thus filters nothing.
		 */
		if (*word == '\0')
			continue;

		if ((term = calloc(1, sizeof(*term))) == NULL)
			err(1, "calloc");
		term->negate = negate;
		if (*word == '@') {
			term->group = find_group(word + 1);
			if (term->group == NULL)
				errx(1, "unknown system call group @%s; "
				    "\"truss -t\" lists them", word + 1);
		} else if (word[strspn(word, "0123456789")] == '\0') {
			/*
			 * A term of nothing but digits names a system call
			 * by number rather than by name.
			 */
			term->number = (u_int)strtonum(word, 0, UINT_MAX,
			    &errstr);
			if (errstr != NULL)
				errx(1, "system call number is %s: %s", errstr,
				    word);
			term->by_number = true;
		} else if ((term->pattern = strdup(word)) == NULL)
			err(1, "strdup");

		/*
		 * A name that can never match is almost always a typo, so
		 * say so rather than quietly tracing nothing.  It is only a
		 * warning: a name is still permitted to be one truss has no
		 * knowledge of.
		 *
		 * Numbers are not checked this way.  A process may issue any
		 * number the kernel can hold, whether or not a system call
		 * is implemented behind it; one that is not simply returns
		 * ENOSYS, which truss reports like any other result.  The
		 * only number that cannot name a system call is one that
		 * does not fit, which the conversion above rejected.
		 */
		if (term->pattern != NULL && !term_matches_any_syscall(term))
			warnx("%s: matches no known system call",
			    term->pattern);

		if (!term->negate)
			filter_default = false;
		STAILQ_INSERT_TAIL(&filter_terms, term, entries);
	}
	free(copy);
}


/*
 * Whether a term selects the system call with the given name and number.
 * A numeric term matches on the number alone, which is what the user
 * asked for: numbers identify a system call within one ABI, and it is
 * the ABI of the traced process that decides which one.
 */
static bool
term_matches(const struct filter_term *term, const char *name, u_int number)
{
	const char *forms[3];
	u_int i, nforms;

	if (term->by_number)
		return (term->number == number);
	if (term->group != NULL)
		return (group_selects(term->group, name, number, 0));

	nforms = name_forms(name, forms);
	for (i = 0; i < nforms; i++) {
		if (fnmatch(term->pattern, forms[i], 0) == 0)
			return (true);
	}
	return (false);
}

/*
 * Whether a term matches any system call of any ABI this build of truss
 * understands.  sysdecode(3) names every system call of every such ABI
 * whether or not the ABI's module happens to be loaded, and names them
 * exactly as truss reports them, so it answers the question a user asks
 * of -t.  Codes beyond an ABI's table return NULL.
 */
static bool
term_matches_any_syscall(const struct filter_term *term)
{
	static const enum sysdecode_abi abis[] = {
		SYSDECODE_ABI_FREEBSD,
		SYSDECODE_ABI_FREEBSD32,
		SYSDECODE_ABI_LINUX,
		SYSDECODE_ABI_LINUX32,
	};
	const char *name;
	size_t i;
	u_int code;

	for (i = 0; i < nitems(abis); i++) {
		for (code = 0; code < SYSCALL_NORMAL_COUNT; code++) {
			name = sysdecode_syscallname(abis[i], code);
			if (name != NULL && term_matches(term, name, code))
				return (true);
		}
	}
	return (false);
}

/*
 * Report whether a system call with the given name is to be traced.
 * With no -t expression every system call is, as before.
 */
bool
syscall_filter_match(const char *name, u_int number)
{
	const struct filter_term *term;
	bool trace;

	if (STAILQ_EMPTY(&filter_terms))
		return (true);

	trace = filter_default;
	STAILQ_FOREACH(term, &filter_terms, entries) {
		if (term_matches(term, name, number))
			trace = !term->negate;
	}
	return (trace);
}
