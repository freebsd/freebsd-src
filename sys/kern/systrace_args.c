/*
 * System call argument to DTrace register array conversion.
 *
 * This file is part of the DTrace syscall provider.
 *
 * DO NOT EDIT-- this file is automatically @generated.
 */

static void
systrace_args(int sysnum, void *params, uint64_t *uarg, int *n_args)
{
	int64_t *iarg = (int64_t *)uarg;
	int a = 0;
	switch (sysnum) {
	/* syscall */
	case 0: {
		*n_args = 0;
		break;
	}
	/* exit */
	case 1: {
		struct exit_args *p = params;
		iarg[a++] = p->rval; /* int */
		*n_args = 1;
		break;
	}
	/* fork */
	case 2: {
		*n_args = 0;
		break;
	}
	/* read */
	case 3: {
		struct read_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* void * */
		uarg[a++] = p->nbyte; /* size_t */
		*n_args = 3;
		break;
	}
	/* write */
	case 4: {
		struct write_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* const void * */
		uarg[a++] = p->nbyte; /* size_t */
		*n_args = 3;
		break;
	}
	/* open */
	case 5: {
		struct open_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->flags; /* int */
		iarg[a++] = p->mode; /* mode_t */
		*n_args = 3;
		break;
	}
	/* close */
	case 6: {
		struct close_args *p = params;
		iarg[a++] = p->fd; /* int */
		*n_args = 1;
		break;
	}
	/* wait4 */
	case 7: {
		struct wait4_args *p = params;
		iarg[a++] = p->pid; /* int */
		uarg[a++] = (intptr_t)p->status; /* int * */
		iarg[a++] = p->options; /* int */
		uarg[a++] = (intptr_t)p->rusage; /* struct rusage * */
		*n_args = 4;
		break;
	}
	/* link */
	case 9: {
		struct link_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->link; /* const char * */
		*n_args = 2;
		break;
	}
	/* unlink */
	case 10: {
		struct unlink_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		*n_args = 1;
		break;
	}
	/* chdir */
	case 12: {
		struct chdir_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		*n_args = 1;
		break;
	}
	/* fchdir */
	case 13: {
		struct fchdir_args *p = params;
		iarg[a++] = p->fd; /* int */
		*n_args = 1;
		break;
	}
	/* chmod */
	case 15: {
		struct chmod_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->mode; /* mode_t */
		*n_args = 2;
		break;
	}
	/* chown */
	case 16: {
		struct chown_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->uid; /* int */
		iarg[a++] = p->gid; /* int */
		*n_args = 3;
		break;
	}
	/* break */
	case 17: {
		struct break_args *p = params;
		uarg[a++] = (intptr_t)p->nsize; /* char * */
		*n_args = 1;
		break;
	}
	/* getpid */
	case 20: {
		*n_args = 0;
		break;
	}
	/* mount */
	case 21: {
		struct mount_args *p = params;
		uarg[a++] = (intptr_t)p->type; /* const char * */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->flags; /* int */
		uarg[a++] = (intptr_t)p->data; /* void * */
		*n_args = 4;
		break;
	}
	/* unmount */
	case 22: {
		struct unmount_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->flags; /* int */
		*n_args = 2;
		break;
	}
	/* setuid */
	case 23: {
		struct setuid_args *p = params;
		uarg[a++] = p->uid; /* uid_t */
		*n_args = 1;
		break;
	}
	/* getuid */
	case 24: {
		*n_args = 0;
		break;
	}
	/* geteuid */
	case 25: {
		*n_args = 0;
		break;
	}
	/* ptrace */
	case 26: {
		struct ptrace_args *p = params;
		iarg[a++] = p->req; /* int */
		iarg[a++] = p->pid; /* pid_t */
		uarg[a++] = (intptr_t)p->addr; /* caddr_t */
		iarg[a++] = p->data; /* int */
		*n_args = 4;
		break;
	}
	/* recvmsg */
	case 27: {
		struct recvmsg_args *p = params;
		iarg[a++] = p->s; /* int */
		uarg[a++] = (intptr_t)p->msg; /* struct msghdr * */
		iarg[a++] = p->flags; /* int */
		*n_args = 3;
		break;
	}
	/* sendmsg */
	case 28: {
		struct sendmsg_args *p = params;
		iarg[a++] = p->s; /* int */
		uarg[a++] = (intptr_t)p->msg; /* const struct msghdr * */
		iarg[a++] = p->flags; /* int */
		*n_args = 3;
		break;
	}
	/* recvfrom */
	case 29: {
		struct recvfrom_args *p = params;
		iarg[a++] = p->s; /* int */
		uarg[a++] = (intptr_t)p->buf; /* void * */
		uarg[a++] = p->len; /* size_t */
		iarg[a++] = p->flags; /* int */
		uarg[a++] = (intptr_t)p->from; /* struct sockaddr * */
		uarg[a++] = (intptr_t)p->fromlenaddr; /* __socklen_t * */
		*n_args = 6;
		break;
	}
	/* accept */
	case 30: {
		struct accept_args *p = params;
		iarg[a++] = p->s; /* int */
		uarg[a++] = (intptr_t)p->name; /* struct sockaddr * */
		uarg[a++] = (intptr_t)p->anamelen; /* __socklen_t * */
		*n_args = 3;
		break;
	}
	/* getpeername */
	case 31: {
		struct getpeername_args *p = params;
		iarg[a++] = p->fdes; /* int */
		uarg[a++] = (intptr_t)p->asa; /* struct sockaddr * */
		uarg[a++] = (intptr_t)p->alen; /* __socklen_t * */
		*n_args = 3;
		break;
	}
	/* getsockname */
	case 32: {
		struct getsockname_args *p = params;
		iarg[a++] = p->fdes; /* int */
		uarg[a++] = (intptr_t)p->asa; /* struct sockaddr * */
		uarg[a++] = (intptr_t)p->alen; /* __socklen_t * */
		*n_args = 3;
		break;
	}
	/* access */
	case 33: {
		struct access_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->amode; /* int */
		*n_args = 2;
		break;
	}
	/* chflags */
	case 34: {
		struct chflags_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = p->flags; /* u_long */
		*n_args = 2;
		break;
	}
	/* fchflags */
	case 35: {
		struct fchflags_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = p->flags; /* u_long */
		*n_args = 2;
		break;
	}
	/* sync */
	case 36: {
		*n_args = 0;
		break;
	}
	/* kill */
	case 37: {
		struct kill_args *p = params;
		iarg[a++] = p->pid; /* int */
		iarg[a++] = p->signum; /* int */
		*n_args = 2;
		break;
	}
	/* getppid */
	case 39: {
		*n_args = 0;
		break;
	}
	/* dup */
	case 41: {
		struct dup_args *p = params;
		uarg[a++] = p->fd; /* u_int */
		*n_args = 1;
		break;
	}
	/* getegid */
	case 43: {
		*n_args = 0;
		break;
	}
	/* profil */
	case 44: {
		struct profil_args *p = params;
		uarg[a++] = (intptr_t)p->samples; /* char * */
		uarg[a++] = p->size; /* size_t */
		uarg[a++] = p->offset; /* size_t */
		uarg[a++] = p->scale; /* u_int */
		*n_args = 4;
		break;
	}
	/* ktrace */
	case 45: {
		struct ktrace_args *p = params;
		uarg[a++] = (intptr_t)p->fname; /* const char * */
		iarg[a++] = p->ops; /* int */
		iarg[a++] = p->facs; /* int */
		iarg[a++] = p->pid; /* int */
		*n_args = 4;
		break;
	}
	/* getgid */
	case 47: {
		*n_args = 0;
		break;
	}
	/* getlogin */
	case 49: {
		struct getlogin_args *p = params;
		uarg[a++] = (intptr_t)p->namebuf; /* char * */
		uarg[a++] = p->namelen; /* u_int */
		*n_args = 2;
		break;
	}
	/* setlogin */
	case 50: {
		struct setlogin_args *p = params;
		uarg[a++] = (intptr_t)p->namebuf; /* const char * */
		*n_args = 1;
		break;
	}
	/* acct */
	case 51: {
		struct acct_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		*n_args = 1;
		break;
	}
	/* sigaltstack */
	case 53: {
		struct sigaltstack_args *p = params;
		uarg[a++] = (intptr_t)p->ss; /* const struct sigaltstack * */
		uarg[a++] = (intptr_t)p->oss; /* struct sigaltstack * */
		*n_args = 2;
		break;
	}
	/* ioctl */
	case 54: {
		struct ioctl_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = p->com; /* u_long */
		uarg[a++] = (intptr_t)p->data; /* char * */
		*n_args = 3;
		break;
	}
	/* reboot */
	case 55: {
		struct reboot_args *p = params;
		iarg[a++] = p->opt; /* int */
		*n_args = 1;
		break;
	}
	/* revoke */
	case 56: {
		struct revoke_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		*n_args = 1;
		break;
	}
	/* symlink */
	case 57: {
		struct symlink_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->link; /* const char * */
		*n_args = 2;
		break;
	}
	/* readlink */
	case 58: {
		struct readlink_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		uarg[a++] = p->count; /* size_t */
		*n_args = 3;
		break;
	}
	/* execve */
	case 59: {
		struct execve_args *p = params;
		uarg[a++] = (intptr_t)p->fname; /* const char * */
		uarg[a++] = (intptr_t)p->argv; /* char ** */
		uarg[a++] = (intptr_t)p->envv; /* char ** */
		*n_args = 3;
		break;
	}
	/* umask */
	case 60: {
		struct umask_args *p = params;
		iarg[a++] = p->newmask; /* mode_t */
		*n_args = 1;
		break;
	}
	/* chroot */
	case 61: {
		struct chroot_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		*n_args = 1;
		break;
	}
	/* msync */
	case 65: {
		struct msync_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* void * */
		uarg[a++] = p->len; /* size_t */
		iarg[a++] = p->flags; /* int */
		*n_args = 3;
		break;
	}
	/* vfork */
	case 66: {
		*n_args = 0;
		break;
	}
	/* munmap */
	case 73: {
		struct munmap_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* void * */
		uarg[a++] = p->len; /* size_t */
		*n_args = 2;
		break;
	}
	/* mprotect */
	case 74: {
		struct mprotect_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* void * */
		uarg[a++] = p->len; /* size_t */
		iarg[a++] = p->prot; /* int */
		*n_args = 3;
		break;
	}
	/* madvise */
	case 75: {
		struct madvise_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* void * */
		uarg[a++] = p->len; /* size_t */
		iarg[a++] = p->behav; /* int */
		*n_args = 3;
		break;
	}
	/* mincore */
	case 78: {
		struct mincore_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* const void * */
		uarg[a++] = p->len; /* size_t */
		uarg[a++] = (intptr_t)p->vec; /* char * */
		*n_args = 3;
		break;
	}
	/* getgroups */
	case 79: {
		struct getgroups_args *p = params;
		iarg[a++] = p->gidsetsize; /* int */
		uarg[a++] = (intptr_t)p->gidset; /* gid_t * */
		*n_args = 2;
		break;
	}
	/* setgroups */
	case 80: {
		struct setgroups_args *p = params;
		iarg[a++] = p->gidsetsize; /* int */
		uarg[a++] = (intptr_t)p->gidset; /* const gid_t * */
		*n_args = 2;
		break;
	}
	/* getpgrp */
	case 81: {
		*n_args = 0;
		break;
	}
	/* setpgid */
	case 82: {
		struct setpgid_args *p = params;
		iarg[a++] = p->pid; /* int */
		iarg[a++] = p->pgid; /* int */
		*n_args = 2;
		break;
	}
	/* setitimer */
	case 83: {
		struct setitimer_args *p = params;
		iarg[a++] = p->which; /* int */
		uarg[a++] = (intptr_t)p->itv; /* const struct itimerval * */
		uarg[a++] = (intptr_t)p->oitv; /* struct itimerval * */
		*n_args = 3;
		break;
	}
	/* swapon */
	case 85: {
		struct swapon_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 1;
		break;
	}
	/* getitimer */
	case 86: {
		struct getitimer_args *p = params;
		iarg[a++] = p->which; /* int */
		uarg[a++] = (intptr_t)p->itv; /* struct itimerval * */
		*n_args = 2;
		break;
	}
	/* getdtablesize */
	case 89: {
		*n_args = 0;
		break;
	}
	/* dup2 */
	case 90: {
		struct dup2_args *p = params;
		uarg[a++] = p->from; /* u_int */
		uarg[a++] = p->to; /* u_int */
		*n_args = 2;
		break;
	}
	/* fcntl */
	case 92: {
		struct fcntl_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->cmd; /* int */
		uarg[a++] = (intptr_t)p->arg; /* intptr_t */
		*n_args = 3;
		break;
	}
	/* select */
	case 93: {
		struct select_args *p = params;
		iarg[a++] = p->nd; /* int */
		uarg[a++] = (intptr_t)p->in; /* fd_set * */
		uarg[a++] = (intptr_t)p->ou; /* fd_set * */
		uarg[a++] = (intptr_t)p->ex; /* fd_set * */
		uarg[a++] = (intptr_t)p->tv; /* struct timeval * */
		*n_args = 5;
		break;
	}
	/* fsync */
	case 95: {
		struct fsync_args *p = params;
		iarg[a++] = p->fd; /* int */
		*n_args = 1;
		break;
	}
	/* setpriority */
	case 96: {
		struct setpriority_args *p = params;
		iarg[a++] = p->which; /* int */
		iarg[a++] = p->who; /* int */
		iarg[a++] = p->prio; /* int */
		*n_args = 3;
		break;
	}
	/* socket */
	case 97: {
		struct socket_args *p = params;
		iarg[a++] = p->domain; /* int */
		iarg[a++] = p->type; /* int */
		iarg[a++] = p->protocol; /* int */
		*n_args = 3;
		break;
	}
	/* connect */
	case 98: {
		struct connect_args *p = params;
		iarg[a++] = p->s; /* int */
		uarg[a++] = (intptr_t)p->name; /* const struct sockaddr * */
		iarg[a++] = p->namelen; /* __socklen_t */
		*n_args = 3;
		break;
	}
	/* getpriority */
	case 100: {
		struct getpriority_args *p = params;
		iarg[a++] = p->which; /* int */
		iarg[a++] = p->who; /* int */
		*n_args = 2;
		break;
	}
	/* bind */
	case 104: {
		struct bind_args *p = params;
		iarg[a++] = p->s; /* int */
		uarg[a++] = (intptr_t)p->name; /* const struct sockaddr * */
		iarg[a++] = p->namelen; /* __socklen_t */
		*n_args = 3;
		break;
	}
	/* setsockopt */
	case 105: {
		struct setsockopt_args *p = params;
		iarg[a++] = p->s; /* int */
		iarg[a++] = p->level; /* int */
		iarg[a++] = p->name; /* int */
		uarg[a++] = (intptr_t)p->val; /* const void * */
		iarg[a++] = p->valsize; /* __socklen_t */
		*n_args = 5;
		break;
	}
	/* listen */
	case 106: {
		struct listen_args *p = params;
		iarg[a++] = p->s; /* int */
		iarg[a++] = p->backlog; /* int */
		*n_args = 2;
		break;
	}
	/* gettimeofday */
	case 116: {
		struct gettimeofday_args *p = params;
		uarg[a++] = (intptr_t)p->tp; /* struct timeval * */
		uarg[a++] = (intptr_t)p->tzp; /* struct timezone * */
		*n_args = 2;
		break;
	}
	/* getrusage */
	case 117: {
		struct getrusage_args *p = params;
		iarg[a++] = p->who; /* int */
		uarg[a++] = (intptr_t)p->rusage; /* struct rusage * */
		*n_args = 2;
		break;
	}
	/* getsockopt */
	case 118: {
		struct getsockopt_args *p = params;
		iarg[a++] = p->s; /* int */
		iarg[a++] = p->level; /* int */
		iarg[a++] = p->name; /* int */
		uarg[a++] = (intptr_t)p->val; /* void * */
		uarg[a++] = (intptr_t)p->avalsize; /* __socklen_t * */
		*n_args = 5;
		break;
	}
	/* readv */
	case 120: {
		struct readv_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->iovp; /* const struct iovec * */
		uarg[a++] = p->iovcnt; /* u_int */
		*n_args = 3;
		break;
	}
	/* writev */
	case 121: {
		struct writev_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->iovp; /* const struct iovec * */
		uarg[a++] = p->iovcnt; /* u_int */
		*n_args = 3;
		break;
	}
	/* settimeofday */
	case 122: {
		struct settimeofday_args *p = params;
		uarg[a++] = (intptr_t)p->tv; /* const struct timeval * */
		uarg[a++] = (intptr_t)p->tzp; /* const struct timezone * */
		*n_args = 2;
		break;
	}
	/* fchown */
	case 123: {
		struct fchown_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->uid; /* int */
		iarg[a++] = p->gid; /* int */
		*n_args = 3;
		break;
	}
	/* fchmod */
	case 124: {
		struct fchmod_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->mode; /* mode_t */
		*n_args = 2;
		break;
	}
	/* setreuid */
	case 126: {
		struct setreuid_args *p = params;
		iarg[a++] = p->ruid; /* int */
		iarg[a++] = p->euid; /* int */
		*n_args = 2;
		break;
	}
	/* setregid */
	case 127: {
		struct setregid_args *p = params;
		iarg[a++] = p->rgid; /* int */
		iarg[a++] = p->egid; /* int */
		*n_args = 2;
		break;
	}
	/* rename */
	case 128: {
		struct rename_args *p = params;
		uarg[a++] = (intptr_t)p->from; /* const char * */
		uarg[a++] = (intptr_t)p->to; /* const char * */
		*n_args = 2;
		break;
	}
	/* flock */
	case 131: {
		struct flock_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->how; /* int */
		*n_args = 2;
		break;
	}
	/* mkfifo */
	case 132: {
		struct mkfifo_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->mode; /* mode_t */
		*n_args = 2;
		break;
	}
	/* sendto */
	case 133: {
		struct sendto_args *p = params;
		iarg[a++] = p->s; /* int */
		uarg[a++] = (intptr_t)p->buf; /* const void * */
		uarg[a++] = p->len; /* size_t */
		iarg[a++] = p->flags; /* int */
		uarg[a++] = (intptr_t)p->to; /* const struct sockaddr * */
		iarg[a++] = p->tolen; /* __socklen_t */
		*n_args = 6;
		break;
	}
	/* shutdown */
	case 134: {
		struct shutdown_args *p = params;
		iarg[a++] = p->s; /* int */
		iarg[a++] = p->how; /* int */
		*n_args = 2;
		break;
	}
	/* socketpair */
	case 135: {
		struct socketpair_args *p = params;
		iarg[a++] = p->domain; /* int */
		iarg[a++] = p->type; /* int */
		iarg[a++] = p->protocol; /* int */
		uarg[a++] = (intptr_t)p->rsv; /* int * */
		*n_args = 4;
		break;
	}
	/* mkdir */
	case 136: {
		struct mkdir_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->mode; /* mode_t */
		*n_args = 2;
		break;
	}
	/* rmdir */
	case 137: {
		struct rmdir_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		*n_args = 1;
		break;
	}
	/* utimes */
	case 138: {
		struct utimes_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->tptr; /* const struct timeval * */
		*n_args = 2;
		break;
	}
	/* adjtime */
	case 140: {
		struct adjtime_args *p = params;
		uarg[a++] = (intptr_t)p->delta; /* const struct timeval * */
		uarg[a++] = (intptr_t)p->olddelta; /* struct timeval * */
		*n_args = 2;
		break;
	}
	/* setsid */
	case 147: {
		*n_args = 0;
		break;
	}
	/* quotactl */
	case 148: {
		struct quotactl_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->cmd; /* int */
		iarg[a++] = p->uid; /* int */
		uarg[a++] = (intptr_t)p->arg; /* void * */
		*n_args = 4;
		break;
	}
	/* nlm_syscall */
	case 154: {
		struct nlm_syscall_args *p = params;
		iarg[a++] = p->debug_level; /* int */
		iarg[a++] = p->grace_period; /* int */
		iarg[a++] = p->addr_count; /* int */
		uarg[a++] = (intptr_t)p->addrs; /* char ** */
		*n_args = 4;
		break;
	}
	/* nfssvc */
	case 155: {
		struct nfssvc_args *p = params;
		iarg[a++] = p->flag; /* int */
		uarg[a++] = (intptr_t)p->argp; /* void * */
		*n_args = 2;
		break;
	}
	/* lgetfh */
	case 160: {
		struct lgetfh_args *p = params;
		uarg[a++] = (intptr_t)p->fname; /* const char * */
		uarg[a++] = (intptr_t)p->fhp; /* struct fhandle * */
		*n_args = 2;
		break;
	}
	/* getfh */
	case 161: {
		struct getfh_args *p = params;
		uarg[a++] = (intptr_t)p->fname; /* const char * */
		uarg[a++] = (intptr_t)p->fhp; /* struct fhandle * */
		*n_args = 2;
		break;
	}
	/* sysarch */
	case 165: {
		struct sysarch_args *p = params;
		iarg[a++] = p->op; /* int */
		uarg[a++] = (intptr_t)p->parms; /* char * */
		*n_args = 2;
		break;
	}
	/* rtprio */
	case 166: {
		struct rtprio_args *p = params;
		iarg[a++] = p->function; /* int */
		iarg[a++] = p->pid; /* pid_t */
		uarg[a++] = (intptr_t)p->rtp; /* struct rtprio * */
		*n_args = 3;
		break;
	}
	/* semsys */
	case 169: {
		struct semsys_args *p = params;
		iarg[a++] = p->which; /* int */
		iarg[a++] = p->a2; /* int */
		iarg[a++] = p->a3; /* int */
		iarg[a++] = p->a4; /* int */
		iarg[a++] = p->a5; /* int */
		*n_args = 5;
		break;
	}
	/* msgsys */
	case 170: {
		struct msgsys_args *p = params;
		iarg[a++] = p->which; /* int */
		iarg[a++] = p->a2; /* int */
		iarg[a++] = p->a3; /* int */
		iarg[a++] = p->a4; /* int */
		iarg[a++] = p->a5; /* int */
		iarg[a++] = p->a6; /* int */
		*n_args = 6;
		break;
	}
	/* shmsys */
	case 171: {
		struct shmsys_args *p = params;
		iarg[a++] = p->which; /* int */
		iarg[a++] = p->a2; /* int */
		iarg[a++] = p->a3; /* int */
		iarg[a++] = p->a4; /* int */
		*n_args = 4;
		break;
	}
	/* setfib */
	case 175: {
		struct setfib_args *p = params;
		iarg[a++] = p->fibnum; /* int */
		*n_args = 1;
		break;
	}
	/* ntp_adjtime */
	case 176: {
		struct ntp_adjtime_args *p = params;
		uarg[a++] = (intptr_t)p->tp; /* struct timex * */
		*n_args = 1;
		break;
	}
	/* setgid */
	case 181: {
		struct setgid_args *p = params;
		iarg[a++] = p->gid; /* gid_t */
		*n_args = 1;
		break;
	}
	/* setegid */
	case 182: {
		struct setegid_args *p = params;
		iarg[a++] = p->egid; /* gid_t */
		*n_args = 1;
		break;
	}
	/* seteuid */
	case 183: {
		struct seteuid_args *p = params;
		uarg[a++] = p->euid; /* uid_t */
		*n_args = 1;
		break;
	}
	/* pathconf */
	case 191: {
		struct pathconf_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->name; /* int */
		*n_args = 2;
		break;
	}
	/* fpathconf */
	case 192: {
		struct fpathconf_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->name; /* int */
		*n_args = 2;
		break;
	}
	/* getrlimit */
	case 194: {
		struct getrlimit_args *p = params;
		uarg[a++] = p->which; /* u_int */
		uarg[a++] = (intptr_t)p->rlp; /* struct rlimit * */
		*n_args = 2;
		break;
	}
	/* setrlimit */
	case 195: {
		struct setrlimit_args *p = params;
		uarg[a++] = p->which; /* u_int */
		uarg[a++] = (intptr_t)p->rlp; /* struct rlimit * */
		*n_args = 2;
		break;
	}
	/* __syscall */
	case 198: {
		*n_args = 0;
		break;
	}
	/* __sysctl */
	case 202: {
		struct __sysctl_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* int * */
		uarg[a++] = p->namelen; /* u_int */
		uarg[a++] = (intptr_t)p->old; /* void * */
		uarg[a++] = (intptr_t)p->oldlenp; /* size_t * */
		uarg[a++] = (intptr_t)p->new; /* const void * */
		uarg[a++] = p->newlen; /* size_t */
		*n_args = 6;
		break;
	}
	/* mlock */
	case 203: {
		struct mlock_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* const void * */
		uarg[a++] = p->len; /* size_t */
		*n_args = 2;
		break;
	}
	/* munlock */
	case 204: {
		struct munlock_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* const void * */
		uarg[a++] = p->len; /* size_t */
		*n_args = 2;
		break;
	}
	/* undelete */
	case 205: {
		struct undelete_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		*n_args = 1;
		break;
	}
	/* futimes */
	case 206: {
		struct futimes_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->tptr; /* const struct timeval * */
		*n_args = 2;
		break;
	}
	/* getpgid */
	case 207: {
		struct getpgid_args *p = params;
		iarg[a++] = p->pid; /* pid_t */
		*n_args = 1;
		break;
	}
	/* poll */
	case 209: {
		struct poll_args *p = params;
		uarg[a++] = (intptr_t)p->fds; /* struct pollfd * */
		uarg[a++] = p->nfds; /* u_int */
		iarg[a++] = p->timeout; /* int */
		*n_args = 3;
		break;
	}
	/* lkmnosys */
	case 210: {
		*n_args = 0;
		break;
	}
	/* lkmnosys */
	case 211: {
		*n_args = 0;
		break;
	}
	/* lkmnosys */
	case 212: {
		*n_args = 0;
		break;
	}
	/* lkmnosys */
	case 213: {
		*n_args = 0;
		break;
	}
	/* lkmnosys */
	case 214: {
		*n_args = 0;
		break;
	}
	/* lkmnosys */
	case 215: {
		*n_args = 0;
		break;
	}
	/* lkmnosys */
	case 216: {
		*n_args = 0;
		break;
	}
	/* lkmnosys */
	case 217: {
		*n_args = 0;
		break;
	}
	/* lkmnosys */
	case 218: {
		*n_args = 0;
		break;
	}
	/* lkmnosys */
	case 219: {
		*n_args = 0;
		break;
	}
	/* semget */
	case 221: {
		struct semget_args *p = params;
		iarg[a++] = p->key; /* key_t */
		iarg[a++] = p->nsems; /* int */
		iarg[a++] = p->semflg; /* int */
		*n_args = 3;
		break;
	}
	/* semop */
	case 222: {
		struct semop_args *p = params;
		iarg[a++] = p->semid; /* int */
		uarg[a++] = (intptr_t)p->sops; /* struct sembuf * */
		uarg[a++] = p->nsops; /* size_t */
		*n_args = 3;
		break;
	}
	/* msgget */
	case 225: {
		struct msgget_args *p = params;
		iarg[a++] = p->key; /* key_t */
		iarg[a++] = p->msgflg; /* int */
		*n_args = 2;
		break;
	}
	/* msgsnd */
	case 226: {
		struct msgsnd_args *p = params;
		iarg[a++] = p->msqid; /* int */
		uarg[a++] = (intptr_t)p->msgp; /* const void * */
		uarg[a++] = p->msgsz; /* size_t */
		iarg[a++] = p->msgflg; /* int */
		*n_args = 4;
		break;
	}
	/* msgrcv */
	case 227: {
		struct msgrcv_args *p = params;
		iarg[a++] = p->msqid; /* int */
		uarg[a++] = (intptr_t)p->msgp; /* void * */
		uarg[a++] = p->msgsz; /* size_t */
		iarg[a++] = p->msgtyp; /* long */
		iarg[a++] = p->msgflg; /* int */
		*n_args = 5;
		break;
	}
	/* shmat */
	case 228: {
		struct shmat_args *p = params;
		iarg[a++] = p->shmid; /* int */
		uarg[a++] = (intptr_t)p->shmaddr; /* const void * */
		iarg[a++] = p->shmflg; /* int */
		*n_args = 3;
		break;
	}
	/* shmdt */
	case 230: {
		struct shmdt_args *p = params;
		uarg[a++] = (intptr_t)p->shmaddr; /* const void * */
		*n_args = 1;
		break;
	}
	/* shmget */
	case 231: {
		struct shmget_args *p = params;
		iarg[a++] = p->key; /* key_t */
		uarg[a++] = p->size; /* size_t */
		iarg[a++] = p->shmflg; /* int */
		*n_args = 3;
		break;
	}
	/* clock_gettime */
	case 232: {
		struct clock_gettime_args *p = params;
		iarg[a++] = p->clock_id; /* clockid_t */
		uarg[a++] = (intptr_t)p->tp; /* struct timespec * */
		*n_args = 2;
		break;
	}
	/* clock_settime */
	case 233: {
		struct clock_settime_args *p = params;
		iarg[a++] = p->clock_id; /* clockid_t */
		uarg[a++] = (intptr_t)p->tp; /* const struct timespec * */
		*n_args = 2;
		break;
	}
	/* clock_getres */
	case 234: {
		struct clock_getres_args *p = params;
		iarg[a++] = p->clock_id; /* clockid_t */
		uarg[a++] = (intptr_t)p->tp; /* struct timespec * */
		*n_args = 2;
		break;
	}
	/* ktimer_create */
	case 235: {
		struct ktimer_create_args *p = params;
		iarg[a++] = p->clock_id; /* clockid_t */
		uarg[a++] = (intptr_t)p->evp; /* struct sigevent * */
		uarg[a++] = (intptr_t)p->timerid; /* int * */
		*n_args = 3;
		break;
	}
	/* ktimer_delete */
	case 236: {
		struct ktimer_delete_args *p = params;
		iarg[a++] = p->timerid; /* int */
		*n_args = 1;
		break;
	}
	/* ktimer_settime */
	case 237: {
		struct ktimer_settime_args *p = params;
		iarg[a++] = p->timerid; /* int */
		iarg[a++] = p->flags; /* int */
		uarg[a++] = (intptr_t)p->value; /* const struct itimerspec * */
		uarg[a++] = (intptr_t)p->ovalue; /* struct itimerspec * */
		*n_args = 4;
		break;
	}
	/* ktimer_gettime */
	case 238: {
		struct ktimer_gettime_args *p = params;
		iarg[a++] = p->timerid; /* int */
		uarg[a++] = (intptr_t)p->value; /* struct itimerspec * */
		*n_args = 2;
		break;
	}
	/* ktimer_getoverrun */
	case 239: {
		struct ktimer_getoverrun_args *p = params;
		iarg[a++] = p->timerid; /* int */
		*n_args = 1;
		break;
	}
	/* nanosleep */
	case 240: {
		struct nanosleep_args *p = params;
		uarg[a++] = (intptr_t)p->rqtp; /* const struct timespec * */
		uarg[a++] = (intptr_t)p->rmtp; /* struct timespec * */
		*n_args = 2;
		break;
	}
	/* ffclock_getcounter */
	case 241: {
		struct ffclock_getcounter_args *p = params;
		uarg[a++] = (intptr_t)p->ffcount; /* ffcounter * */
		*n_args = 1;
		break;
	}
	/* ffclock_setestimate */
	case 242: {
		struct ffclock_setestimate_args *p = params;
		uarg[a++] = (intptr_t)p->cest; /* struct ffclock_estimate * */
		*n_args = 1;
		break;
	}
	/* ffclock_getestimate */
	case 243: {
		struct ffclock_getestimate_args *p = params;
		uarg[a++] = (intptr_t)p->cest; /* struct ffclock_estimate * */
		*n_args = 1;
		break;
	}
	/* clock_nanosleep */
	case 244: {
		struct clock_nanosleep_args *p = params;
		iarg[a++] = p->clock_id; /* clockid_t */
		iarg[a++] = p->flags; /* int */
		uarg[a++] = (intptr_t)p->rqtp; /* const struct timespec * */
		uarg[a++] = (intptr_t)p->rmtp; /* struct timespec * */
		*n_args = 4;
		break;
	}
	/* clock_getcpuclockid2 */
	case 247: {
		struct clock_getcpuclockid2_args *p = params;
		iarg[a++] = p->id; /* id_t */
		iarg[a++] = p->which; /* int */
		uarg[a++] = (intptr_t)p->clock_id; /* clockid_t * */
		*n_args = 3;
		break;
	}
	/* ntp_gettime */
	case 248: {
		struct ntp_gettime_args *p = params;
		uarg[a++] = (intptr_t)p->ntvp; /* struct ntptimeval * */
		*n_args = 1;
		break;
	}
	/* minherit */
	case 250: {
		struct minherit_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* void * */
		uarg[a++] = p->len; /* size_t */
		iarg[a++] = p->inherit; /* int */
		*n_args = 3;
		break;
	}
	/* rfork */
	case 251: {
		struct rfork_args *p = params;
		iarg[a++] = p->flags; /* int */
		*n_args = 1;
		break;
	}
	/* issetugid */
	case 253: {
		*n_args = 0;
		break;
	}
	/* lchown */
	case 254: {
		struct lchown_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->uid; /* int */
		iarg[a++] = p->gid; /* int */
		*n_args = 3;
		break;
	}
	/* aio_read */
	case 255: {
		struct aio_read_args *p = params;
		uarg[a++] = (intptr_t)p->aiocbp; /* struct aiocb * */
		*n_args = 1;
		break;
	}
	/* aio_write */
	case 256: {
		struct aio_write_args *p = params;
		uarg[a++] = (intptr_t)p->aiocbp; /* struct aiocb * */
		*n_args = 1;
		break;
	}
	/* lio_listio */
	case 257: {
		struct lio_listio_args *p = params;
		iarg[a++] = p->mode; /* int */
		uarg[a++] = (intptr_t)p->acb_list; /* struct aiocb * const * */
		iarg[a++] = p->nent; /* int */
		uarg[a++] = (intptr_t)p->sig; /* struct sigevent * */
		*n_args = 4;
		break;
	}
	/* lchmod */
	case 274: {
		struct lchmod_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->mode; /* mode_t */
		*n_args = 2;
		break;
	}
	/* lutimes */
	case 276: {
		struct lutimes_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->tptr; /* const struct timeval * */
		*n_args = 2;
		break;
	}
	/* preadv */
	case 289: {
		struct preadv_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->iovp; /* struct iovec * */
		uarg[a++] = p->iovcnt; /* u_int */
		iarg[a++] = p->offset; /* off_t */
		*n_args = 4;
		break;
	}
	/* pwritev */
	case 290: {
		struct pwritev_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->iovp; /* struct iovec * */
		uarg[a++] = p->iovcnt; /* u_int */
		iarg[a++] = p->offset; /* off_t */
		*n_args = 4;
		break;
	}
	/* fhopen */
	case 298: {
		struct fhopen_args *p = params;
		uarg[a++] = (intptr_t)p->u_fhp; /* const struct fhandle * */
		iarg[a++] = p->flags; /* int */
		*n_args = 2;
		break;
	}
	/* modnext */
	case 300: {
		struct modnext_args *p = params;
		iarg[a++] = p->modid; /* int */
		*n_args = 1;
		break;
	}
	/* modstat */
	case 301: {
		struct modstat_args *p = params;
		iarg[a++] = p->modid; /* int */
		uarg[a++] = (intptr_t)p->stat; /* struct module_stat * */
		*n_args = 2;
		break;
	}
	/* modfnext */
	case 302: {
		struct modfnext_args *p = params;
		iarg[a++] = p->modid; /* int */
		*n_args = 1;
		break;
	}
	/* modfind */
	case 303: {
		struct modfind_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 1;
		break;
	}
	/* kldload */
	case 304: {
		struct kldload_args *p = params;
		uarg[a++] = (intptr_t)p->file; /* const char * */
		*n_args = 1;
		break;
	}
	/* kldunload */
	case 305: {
		struct kldunload_args *p = params;
		iarg[a++] = p->fileid; /* int */
		*n_args = 1;
		break;
	}
	/* kldfind */
	case 306: {
		struct kldfind_args *p = params;
		uarg[a++] = (intptr_t)p->file; /* const char * */
		*n_args = 1;
		break;
	}
	/* kldnext */
	case 307: {
		struct kldnext_args *p = params;
		iarg[a++] = p->fileid; /* int */
		*n_args = 1;
		break;
	}
	/* kldstat */
	case 308: {
		struct kldstat_args *p = params;
		iarg[a++] = p->fileid; /* int */
		uarg[a++] = (intptr_t)p->stat; /* struct kld_file_stat * */
		*n_args = 2;
		break;
	}
	/* kldfirstmod */
	case 309: {
		struct kldfirstmod_args *p = params;
		iarg[a++] = p->fileid; /* int */
		*n_args = 1;
		break;
	}
	/* getsid */
	case 310: {
		struct getsid_args *p = params;
		iarg[a++] = p->pid; /* pid_t */
		*n_args = 1;
		break;
	}
	/* setresuid */
	case 311: {
		struct setresuid_args *p = params;
		uarg[a++] = p->ruid; /* uid_t */
		uarg[a++] = p->euid; /* uid_t */
		uarg[a++] = p->suid; /* uid_t */
		*n_args = 3;
		break;
	}
	/* setresgid */
	case 312: {
		struct setresgid_args *p = params;
		iarg[a++] = p->rgid; /* gid_t */
		iarg[a++] = p->egid; /* gid_t */
		iarg[a++] = p->sgid; /* gid_t */
		*n_args = 3;
		break;
	}
	/* aio_return */
	case 314: {
		struct aio_return_args *p = params;
		uarg[a++] = (intptr_t)p->aiocbp; /* struct aiocb * */
		*n_args = 1;
		break;
	}
	/* aio_suspend */
	case 315: {
		struct aio_suspend_args *p = params;
		uarg[a++] = (intptr_t)p->aiocbp; /* const struct aiocb * const * */
		iarg[a++] = p->nent; /* int */
		uarg[a++] = (intptr_t)p->timeout; /* const struct timespec * */
		*n_args = 3;
		break;
	}
	/* aio_cancel */
	case 316: {
		struct aio_cancel_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->aiocbp; /* struct aiocb * */
		*n_args = 2;
		break;
	}
	/* aio_error */
	case 317: {
		struct aio_error_args *p = params;
		uarg[a++] = (intptr_t)p->aiocbp; /* struct aiocb * */
		*n_args = 1;
		break;
	}
	/* yield */
	case 321: {
		*n_args = 0;
		break;
	}
	/* mlockall */
	case 324: {
		struct mlockall_args *p = params;
		iarg[a++] = p->how; /* int */
		*n_args = 1;
		break;
	}
	/* munlockall */
	case 325: {
		*n_args = 0;
		break;
	}
	/* __getcwd */
	case 326: {
		struct __getcwd_args *p = params;
		uarg[a++] = (intptr_t)p->buf; /* char * */
		uarg[a++] = p->buflen; /* size_t */
		*n_args = 2;
		break;
	}
	/* sched_setparam */
	case 327: {
		struct sched_setparam_args *p = params;
		iarg[a++] = p->pid; /* pid_t */
		uarg[a++] = (intptr_t)p->param; /* const struct sched_param * */
		*n_args = 2;
		break;
	}
	/* sched_getparam */
	case 328: {
		struct sched_getparam_args *p = params;
		iarg[a++] = p->pid; /* pid_t */
		uarg[a++] = (intptr_t)p->param; /* struct sched_param * */
		*n_args = 2;
		break;
	}
	/* sched_setscheduler */
	case 329: {
		struct sched_setscheduler_args *p = params;
		iarg[a++] = p->pid; /* pid_t */
		iarg[a++] = p->policy; /* int */
		uarg[a++] = (intptr_t)p->param; /* const struct sched_param * */
		*n_args = 3;
		break;
	}
	/* sched_getscheduler */
	case 330: {
		struct sched_getscheduler_args *p = params;
		iarg[a++] = p->pid; /* pid_t */
		*n_args = 1;
		break;
	}
	/* sched_yield */
	case 331: {
		*n_args = 0;
		break;
	}
	/* sched_get_priority_max */
	case 332: {
		struct sched_get_priority_max_args *p = params;
		iarg[a++] = p->policy; /* int */
		*n_args = 1;
		break;
	}
	/* sched_get_priority_min */
	case 333: {
		struct sched_get_priority_min_args *p = params;
		iarg[a++] = p->policy; /* int */
		*n_args = 1;
		break;
	}
	/* sched_rr_get_interval */
	case 334: {
		struct sched_rr_get_interval_args *p = params;
		iarg[a++] = p->pid; /* pid_t */
		uarg[a++] = (intptr_t)p->interval; /* struct timespec * */
		*n_args = 2;
		break;
	}
	/* utrace */
	case 335: {
		struct utrace_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* const void * */
		uarg[a++] = p->len; /* size_t */
		*n_args = 2;
		break;
	}
	/* kldsym */
	case 337: {
		struct kldsym_args *p = params;
		iarg[a++] = p->fileid; /* int */
		iarg[a++] = p->cmd; /* int */
		uarg[a++] = (intptr_t)p->data; /* void * */
		*n_args = 3;
		break;
	}
	/* jail */
	case 338: {
		struct jail_args *p = params;
		uarg[a++] = (intptr_t)p->jail; /* struct jail * */
		*n_args = 1;
		break;
	}
	/* nnpfs_syscall */
	case 339: {
		struct nnpfs_syscall_args *p = params;
		iarg[a++] = p->operation; /* int */
		uarg[a++] = (intptr_t)p->a_pathP; /* char * */
		iarg[a++] = p->a_opcode; /* int */
		uarg[a++] = (intptr_t)p->a_paramsP; /* void * */
		iarg[a++] = p->a_followSymlinks; /* int */
		*n_args = 5;
		break;
	}
	/* sigprocmask */
	case 340: {
		struct sigprocmask_args *p = params;
		iarg[a++] = p->how; /* int */
		uarg[a++] = (intptr_t)p->set; /* const sigset_t * */
		uarg[a++] = (intptr_t)p->oset; /* sigset_t * */
		*n_args = 3;
		break;
	}
	/* sigsuspend */
	case 341: {
		struct sigsuspend_args *p = params;
		uarg[a++] = (intptr_t)p->sigmask; /* const sigset_t * */
		*n_args = 1;
		break;
	}
	/* sigpending */
	case 343: {
		struct sigpending_args *p = params;
		uarg[a++] = (intptr_t)p->set; /* sigset_t * */
		*n_args = 1;
		break;
	}
	/* sigtimedwait */
	case 345: {
		struct sigtimedwait_args *p = params;
		uarg[a++] = (intptr_t)p->set; /* const sigset_t * */
		uarg[a++] = (intptr_t)p->info; /* struct __siginfo * */
		uarg[a++] = (intptr_t)p->timeout; /* const struct timespec * */
		*n_args = 3;
		break;
	}
	/* sigwaitinfo */
	case 346: {
		struct sigwaitinfo_args *p = params;
		uarg[a++] = (intptr_t)p->set; /* const sigset_t * */
		uarg[a++] = (intptr_t)p->info; /* struct __siginfo * */
		*n_args = 2;
		break;
	}
	/* __acl_get_file */
	case 347: {
		struct __acl_get_file_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->type; /* __acl_type_t */
		uarg[a++] = (intptr_t)p->aclp; /* struct acl * */
		*n_args = 3;
		break;
	}
	/* __acl_set_file */
	case 348: {
		struct __acl_set_file_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->type; /* __acl_type_t */
		uarg[a++] = (intptr_t)p->aclp; /* struct acl * */
		*n_args = 3;
		break;
	}
	/* __acl_get_fd */
	case 349: {
		struct __acl_get_fd_args *p = params;
		iarg[a++] = p->filedes; /* int */
		iarg[a++] = p->type; /* __acl_type_t */
		uarg[a++] = (intptr_t)p->aclp; /* struct acl * */
		*n_args = 3;
		break;
	}
	/* __acl_set_fd */
	case 350: {
		struct __acl_set_fd_args *p = params;
		iarg[a++] = p->filedes; /* int */
		iarg[a++] = p->type; /* __acl_type_t */
		uarg[a++] = (intptr_t)p->aclp; /* struct acl * */
		*n_args = 3;
		break;
	}
	/* __acl_delete_file */
	case 351: {
		struct __acl_delete_file_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->type; /* __acl_type_t */
		*n_args = 2;
		break;
	}
	/* __acl_delete_fd */
	case 352: {
		struct __acl_delete_fd_args *p = params;
		iarg[a++] = p->filedes; /* int */
		iarg[a++] = p->type; /* __acl_type_t */
		*n_args = 2;
		break;
	}
	/* __acl_aclcheck_file */
	case 353: {
		struct __acl_aclcheck_file_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->type; /* __acl_type_t */
		uarg[a++] = (intptr_t)p->aclp; /* struct acl * */
		*n_args = 3;
		break;
	}
	/* __acl_aclcheck_fd */
	case 354: {
		struct __acl_aclcheck_fd_args *p = params;
		iarg[a++] = p->filedes; /* int */
		iarg[a++] = p->type; /* __acl_type_t */
		uarg[a++] = (intptr_t)p->aclp; /* struct acl * */
		*n_args = 3;
		break;
	}
	/* extattrctl */
	case 355: {
		struct extattrctl_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->cmd; /* int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->attrname; /* const char * */
		*n_args = 5;
		break;
	}
	/* extattr_set_file */
	case 356: {
		struct extattr_set_file_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->attrname; /* const char * */
		uarg[a++] = (intptr_t)p->data; /* void * */
		uarg[a++] = p->nbytes; /* size_t */
		*n_args = 5;
		break;
	}
	/* extattr_get_file */
	case 357: {
		struct extattr_get_file_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->attrname; /* const char * */
		uarg[a++] = (intptr_t)p->data; /* void * */
		uarg[a++] = p->nbytes; /* size_t */
		*n_args = 5;
		break;
	}
	/* extattr_delete_file */
	case 358: {
		struct extattr_delete_file_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->attrname; /* const char * */
		*n_args = 3;
		break;
	}
	/* aio_waitcomplete */
	case 359: {
		struct aio_waitcomplete_args *p = params;
		uarg[a++] = (intptr_t)p->aiocbp; /* struct aiocb ** */
		uarg[a++] = (intptr_t)p->timeout; /* struct timespec * */
		*n_args = 2;
		break;
	}
	/* getresuid */
	case 360: {
		struct getresuid_args *p = params;
		uarg[a++] = (intptr_t)p->ruid; /* uid_t * */
		uarg[a++] = (intptr_t)p->euid; /* uid_t * */
		uarg[a++] = (intptr_t)p->suid; /* uid_t * */
		*n_args = 3;
		break;
	}
	/* getresgid */
	case 361: {
		struct getresgid_args *p = params;
		uarg[a++] = (intptr_t)p->rgid; /* gid_t * */
		uarg[a++] = (intptr_t)p->egid; /* gid_t * */
		uarg[a++] = (intptr_t)p->sgid; /* gid_t * */
		*n_args = 3;
		break;
	}
	/* kqueue */
	case 362: {
		*n_args = 0;
		break;
	}
	/* extattr_set_fd */
	case 371: {
		struct extattr_set_fd_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->attrname; /* const char * */
		uarg[a++] = (intptr_t)p->data; /* void * */
		uarg[a++] = p->nbytes; /* size_t */
		*n_args = 5;
		break;
	}
	/* extattr_get_fd */
	case 372: {
		struct extattr_get_fd_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->attrname; /* const char * */
		uarg[a++] = (intptr_t)p->data; /* void * */
		uarg[a++] = p->nbytes; /* size_t */
		*n_args = 5;
		break;
	}
	/* extattr_delete_fd */
	case 373: {
		struct extattr_delete_fd_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->attrname; /* const char * */
		*n_args = 3;
		break;
	}
	/* __setugid */
	case 374: {
		struct __setugid_args *p = params;
		iarg[a++] = p->flag; /* int */
		*n_args = 1;
		break;
	}
	/* eaccess */
	case 376: {
		struct eaccess_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->amode; /* int */
		*n_args = 2;
		break;
	}
	/* afs3_syscall */
	case 377: {
		struct afs3_syscall_args *p = params;
		iarg[a++] = p->syscall; /* long */
		iarg[a++] = p->parm1; /* long */
		iarg[a++] = p->parm2; /* long */
		iarg[a++] = p->parm3; /* long */
		iarg[a++] = p->parm4; /* long */
		iarg[a++] = p->parm5; /* long */
		iarg[a++] = p->parm6; /* long */
		*n_args = 7;
		break;
	}
	/* nmount */
	case 378: {
		struct nmount_args *p = params;
		uarg[a++] = (intptr_t)p->iovp; /* struct iovec * */
		uarg[a++] = p->iovcnt; /* unsigned int */
		iarg[a++] = p->flags; /* int */
		*n_args = 3;
		break;
	}
	/* __mac_get_proc */
	case 384: {
		struct __mac_get_proc_args *p = params;
		uarg[a++] = (intptr_t)p->mac_p; /* struct mac * */
		*n_args = 1;
		break;
	}
	/* __mac_set_proc */
	case 385: {
		struct __mac_set_proc_args *p = params;
		uarg[a++] = (intptr_t)p->mac_p; /* struct mac * */
		*n_args = 1;
		break;
	}
	/* __mac_get_fd */
	case 386: {
		struct __mac_get_fd_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->mac_p; /* struct mac * */
		*n_args = 2;
		break;
	}
	/* __mac_get_file */
	case 387: {
		struct __mac_get_file_args *p = params;
		uarg[a++] = (intptr_t)p->path_p; /* const char * */
		uarg[a++] = (intptr_t)p->mac_p; /* struct mac * */
		*n_args = 2;
		break;
	}
	/* __mac_set_fd */
	case 388: {
		struct __mac_set_fd_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->mac_p; /* struct mac * */
		*n_args = 2;
		break;
	}
	/* __mac_set_file */
	case 389: {
		struct __mac_set_file_args *p = params;
		uarg[a++] = (intptr_t)p->path_p; /* const char * */
		uarg[a++] = (intptr_t)p->mac_p; /* struct mac * */
		*n_args = 2;
		break;
	}
	/* kenv */
	case 390: {
		struct kenv_args *p = params;
		iarg[a++] = p->what; /* int */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = (intptr_t)p->value; /* char * */
		iarg[a++] = p->len; /* int */
		*n_args = 4;
		break;
	}
	/* lchflags */
	case 391: {
		struct lchflags_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = p->flags; /* u_long */
		*n_args = 2;
		break;
	}
	/* uuidgen */
	case 392: {
		struct uuidgen_args *p = params;
		uarg[a++] = (intptr_t)p->store; /* struct uuid * */
		iarg[a++] = p->count; /* int */
		*n_args = 2;
		break;
	}
	/* sendfile */
	case 393: {
		struct sendfile_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->s; /* int */
		iarg[a++] = p->offset; /* off_t */
		uarg[a++] = p->nbytes; /* size_t */
		uarg[a++] = (intptr_t)p->hdtr; /* struct sf_hdtr * */
		uarg[a++] = (intptr_t)p->sbytes; /* off_t * */
		iarg[a++] = p->flags; /* int */
		*n_args = 7;
		break;
	}
	/* mac_syscall */
	case 394: {
		struct mac_syscall_args *p = params;
		uarg[a++] = (intptr_t)p->policy; /* const char * */
		iarg[a++] = p->call; /* int */
		uarg[a++] = (intptr_t)p->arg; /* void * */
		*n_args = 3;
		break;
	}
	/* ksem_close */
	case 400: {
		struct ksem_close_args *p = params;
		iarg[a++] = p->id; /* semid_t */
		*n_args = 1;
		break;
	}
	/* ksem_post */
	case 401: {
		struct ksem_post_args *p = params;
		iarg[a++] = p->id; /* semid_t */
		*n_args = 1;
		break;
	}
	/* ksem_wait */
	case 402: {
		struct ksem_wait_args *p = params;
		iarg[a++] = p->id; /* semid_t */
		*n_args = 1;
		break;
	}
	/* ksem_trywait */
	case 403: {
		struct ksem_trywait_args *p = params;
		iarg[a++] = p->id; /* semid_t */
		*n_args = 1;
		break;
	}
	/* ksem_init */
	case 404: {
		struct ksem_init_args *p = params;
		uarg[a++] = (intptr_t)p->idp; /* semid_t * */
		uarg[a++] = p->value; /* unsigned int */
		*n_args = 2;
		break;
	}
	/* ksem_open */
	case 405: {
		struct ksem_open_args *p = params;
		uarg[a++] = (intptr_t)p->idp; /* semid_t * */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		iarg[a++] = p->oflag; /* int */
		iarg[a++] = p->mode; /* mode_t */
		uarg[a++] = p->value; /* unsigned int */
		*n_args = 5;
		break;
	}
	/* ksem_unlink */
	case 406: {
		struct ksem_unlink_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 1;
		break;
	}
	/* ksem_getvalue */
	case 407: {
		struct ksem_getvalue_args *p = params;
		iarg[a++] = p->id; /* semid_t */
		uarg[a++] = (intptr_t)p->val; /* int * */
		*n_args = 2;
		break;
	}
	/* ksem_destroy */
	case 408: {
		struct ksem_destroy_args *p = params;
		iarg[a++] = p->id; /* semid_t */
		*n_args = 1;
		break;
	}
	/* __mac_get_pid */
	case 409: {
		struct __mac_get_pid_args *p = params;
		iarg[a++] = p->pid; /* pid_t */
		uarg[a++] = (intptr_t)p->mac_p; /* struct mac * */
		*n_args = 2;
		break;
	}
	/* __mac_get_link */
	case 410: {
		struct __mac_get_link_args *p = params;
		uarg[a++] = (intptr_t)p->path_p; /* const char * */
		uarg[a++] = (intptr_t)p->mac_p; /* struct mac * */
		*n_args = 2;
		break;
	}
	/* __mac_set_link */
	case 411: {
		struct __mac_set_link_args *p = params;
		uarg[a++] = (intptr_t)p->path_p; /* const char * */
		uarg[a++] = (intptr_t)p->mac_p; /* struct mac * */
		*n_args = 2;
		break;
	}
	/* extattr_set_link */
	case 412: {
		struct extattr_set_link_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->attrname; /* const char * */
		uarg[a++] = (intptr_t)p->data; /* void * */
		uarg[a++] = p->nbytes; /* size_t */
		*n_args = 5;
		break;
	}
	/* extattr_get_link */
	case 413: {
		struct extattr_get_link_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->attrname; /* const char * */
		uarg[a++] = (intptr_t)p->data; /* void * */
		uarg[a++] = p->nbytes; /* size_t */
		*n_args = 5;
		break;
	}
	/* extattr_delete_link */
	case 414: {
		struct extattr_delete_link_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->attrname; /* const char * */
		*n_args = 3;
		break;
	}
	/* __mac_execve */
	case 415: {
		struct __mac_execve_args *p = params;
		uarg[a++] = (intptr_t)p->fname; /* const char * */
		uarg[a++] = (intptr_t)p->argv; /* char ** */
		uarg[a++] = (intptr_t)p->envv; /* char ** */
		uarg[a++] = (intptr_t)p->mac_p; /* struct mac * */
		*n_args = 4;
		break;
	}
	/* sigaction */
	case 416: {
		struct sigaction_args *p = params;
		iarg[a++] = p->sig; /* int */
		uarg[a++] = (intptr_t)p->act; /* const struct sigaction * */
		uarg[a++] = (intptr_t)p->oact; /* struct sigaction * */
		*n_args = 3;
		break;
	}
	/* sigreturn */
	case 417: {
		struct sigreturn_args *p = params;
		uarg[a++] = (intptr_t)p->sigcntxp; /* const struct __ucontext * */
		*n_args = 1;
		break;
	}
	/* getcontext */
	case 421: {
		struct getcontext_args *p = params;
		uarg[a++] = (intptr_t)p->ucp; /* struct __ucontext * */
		*n_args = 1;
		break;
	}
	/* setcontext */
	case 422: {
		struct setcontext_args *p = params;
		uarg[a++] = (intptr_t)p->ucp; /* const struct __ucontext * */
		*n_args = 1;
		break;
	}
	/* swapcontext */
	case 423: {
		struct swapcontext_args *p = params;
		uarg[a++] = (intptr_t)p->oucp; /* struct __ucontext * */
		uarg[a++] = (intptr_t)p->ucp; /* const struct __ucontext * */
		*n_args = 2;
		break;
	}
	/* __acl_get_link */
	case 425: {
		struct __acl_get_link_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->type; /* __acl_type_t */
		uarg[a++] = (intptr_t)p->aclp; /* struct acl * */
		*n_args = 3;
		break;
	}
	/* __acl_set_link */
	case 426: {
		struct __acl_set_link_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->type; /* __acl_type_t */
		uarg[a++] = (intptr_t)p->aclp; /* struct acl * */
		*n_args = 3;
		break;
	}
	/* __acl_delete_link */
	case 427: {
		struct __acl_delete_link_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->type; /* __acl_type_t */
		*n_args = 2;
		break;
	}
	/* __acl_aclcheck_link */
	case 428: {
		struct __acl_aclcheck_link_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->type; /* __acl_type_t */
		uarg[a++] = (intptr_t)p->aclp; /* struct acl * */
		*n_args = 3;
		break;
	}
	/* sigwait */
	case 429: {
		struct sigwait_args *p = params;
		uarg[a++] = (intptr_t)p->set; /* const sigset_t * */
		uarg[a++] = (intptr_t)p->sig; /* int * */
		*n_args = 2;
		break;
	}
	/* thr_create */
	case 430: {
		struct thr_create_args *p = params;
		uarg[a++] = (intptr_t)p->ctx; /* ucontext_t * */
		uarg[a++] = (intptr_t)p->id; /* long * */
		iarg[a++] = p->flags; /* int */
		*n_args = 3;
		break;
	}
	/* thr_exit */
	case 431: {
		struct thr_exit_args *p = params;
		uarg[a++] = (intptr_t)p->state; /* long * */
		*n_args = 1;
		break;
	}
	/* thr_self */
	case 432: {
		struct thr_self_args *p = params;
		uarg[a++] = (intptr_t)p->id; /* long * */
		*n_args = 1;
		break;
	}
	/* thr_kill */
	case 433: {
		struct thr_kill_args *p = params;
		iarg[a++] = p->id; /* long */
		iarg[a++] = p->sig; /* int */
		*n_args = 2;
		break;
	}
	/* jail_attach */
	case 436: {
		struct jail_attach_args *p = params;
		iarg[a++] = p->jid; /* int */
		*n_args = 1;
		break;
	}
	/* extattr_list_fd */
	case 437: {
		struct extattr_list_fd_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->data; /* void * */
		uarg[a++] = p->nbytes; /* size_t */
		*n_args = 4;
		break;
	}
	/* extattr_list_file */
	case 438: {
		struct extattr_list_file_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->data; /* void * */
		uarg[a++] = p->nbytes; /* size_t */
		*n_args = 4;
		break;
	}
	/* extattr_list_link */
	case 439: {
		struct extattr_list_link_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->attrnamespace; /* int */
		uarg[a++] = (intptr_t)p->data; /* void * */
		uarg[a++] = p->nbytes; /* size_t */
		*n_args = 4;
		break;
	}
	/* ksem_timedwait */
	case 441: {
		struct ksem_timedwait_args *p = params;
		iarg[a++] = p->id; /* semid_t */
		uarg[a++] = (intptr_t)p->abstime; /* const struct timespec * */
		*n_args = 2;
		break;
	}
	/* thr_suspend */
	case 442: {
		struct thr_suspend_args *p = params;
		uarg[a++] = (intptr_t)p->timeout; /* const struct timespec * */
		*n_args = 1;
		break;
	}
	/* thr_wake */
	case 443: {
		struct thr_wake_args *p = params;
		iarg[a++] = p->id; /* long */
		*n_args = 1;
		break;
	}
	/* kldunloadf */
	case 444: {
		struct kldunloadf_args *p = params;
		iarg[a++] = p->fileid; /* int */
		iarg[a++] = p->flags; /* int */
		*n_args = 2;
		break;
	}
	/* audit */
	case 445: {
		struct audit_args *p = params;
		uarg[a++] = (intptr_t)p->record; /* const void * */
		uarg[a++] = p->length; /* u_int */
		*n_args = 2;
		break;
	}
	/* auditon */
	case 446: {
		struct auditon_args *p = params;
		iarg[a++] = p->cmd; /* int */
		uarg[a++] = (intptr_t)p->data; /* void * */
		uarg[a++] = p->length; /* u_int */
		*n_args = 3;
		break;
	}
	/* getauid */
	case 447: {
		struct getauid_args *p = params;
		uarg[a++] = (intptr_t)p->auid; /* uid_t * */
		*n_args = 1;
		break;
	}
	/* setauid */
	case 448: {
		struct setauid_args *p = params;
		uarg[a++] = (intptr_t)p->auid; /* uid_t * */
		*n_args = 1;
		break;
	}
	/* getaudit */
	case 449: {
		struct getaudit_args *p = params;
		uarg[a++] = (intptr_t)p->auditinfo; /* struct auditinfo * */
		*n_args = 1;
		break;
	}
	/* setaudit */
	case 450: {
		struct setaudit_args *p = params;
		uarg[a++] = (intptr_t)p->auditinfo; /* struct auditinfo * */
		*n_args = 1;
		break;
	}
	/* getaudit_addr */
	case 451: {
		struct getaudit_addr_args *p = params;
		uarg[a++] = (intptr_t)p->auditinfo_addr; /* struct auditinfo_addr * */
		uarg[a++] = p->length; /* u_int */
		*n_args = 2;
		break;
	}
	/* setaudit_addr */
	case 452: {
		struct setaudit_addr_args *p = params;
		uarg[a++] = (intptr_t)p->auditinfo_addr; /* struct auditinfo_addr * */
		uarg[a++] = p->length; /* u_int */
		*n_args = 2;
		break;
	}
	/* auditctl */
	case 453: {
		struct auditctl_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		*n_args = 1;
		break;
	}
	/* _umtx_op */
	case 454: {
		struct _umtx_op_args *p = params;
		uarg[a++] = (intptr_t)p->obj; /* void * */
		iarg[a++] = p->op; /* int */
		uarg[a++] = p->val; /* u_long */
		uarg[a++] = (intptr_t)p->uaddr1; /* void * */
		uarg[a++] = (intptr_t)p->uaddr2; /* void * */
		*n_args = 5;
		break;
	}
	/* thr_new */
	case 455: {
		struct thr_new_args *p = params;
		uarg[a++] = (intptr_t)p->param; /* struct thr_param * */
		iarg[a++] = p->param_size; /* int */
		*n_args = 2;
		break;
	}
	/* sigqueue */
	case 456: {
		struct sigqueue_args *p = params;
		iarg[a++] = p->pid; /* pid_t */
		iarg[a++] = p->signum; /* int */
		uarg[a++] = (intptr_t)p->value; /* void * */
		*n_args = 3;
		break;
	}
	/* kmq_open */
	case 457: {
		struct kmq_open_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->flags; /* int */
		iarg[a++] = p->mode; /* mode_t */
		uarg[a++] = (intptr_t)p->attr; /* const struct mq_attr * */
		*n_args = 4;
		break;
	}
	/* kmq_setattr */
	case 458: {
		struct kmq_setattr_args *p = params;
		iarg[a++] = p->mqd; /* int */
		uarg[a++] = (intptr_t)p->attr; /* const struct mq_attr * */
		uarg[a++] = (intptr_t)p->oattr; /* struct mq_attr * */
		*n_args = 3;
		break;
	}
	/* kmq_timedreceive */
	case 459: {
		struct kmq_timedreceive_args *p = params;
		iarg[a++] = p->mqd; /* int */
		uarg[a++] = (intptr_t)p->msg_ptr; /* char * */
		uarg[a++] = p->msg_len; /* size_t */
		uarg[a++] = (intptr_t)p->msg_prio; /* unsigned * */
		uarg[a++] = (intptr_t)p->abs_timeout; /* const struct timespec * */
		*n_args = 5;
		break;
	}
	/* kmq_timedsend */
	case 460: {
		struct kmq_timedsend_args *p = params;
		iarg[a++] = p->mqd; /* int */
		uarg[a++] = (intptr_t)p->msg_ptr; /* const char * */
		uarg[a++] = p->msg_len; /* size_t */
		uarg[a++] = p->msg_prio; /* unsigned */
		uarg[a++] = (intptr_t)p->abs_timeout; /* const struct timespec * */
		*n_args = 5;
		break;
	}
	/* kmq_notify */
	case 461: {
		struct kmq_notify_args *p = params;
		iarg[a++] = p->mqd; /* int */
		uarg[a++] = (intptr_t)p->sigev; /* const struct sigevent * */
		*n_args = 2;
		break;
	}
	/* kmq_unlink */
	case 462: {
		struct kmq_unlink_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		*n_args = 1;
		break;
	}
	/* abort2 */
	case 463: {
		struct abort2_args *p = params;
		uarg[a++] = (intptr_t)p->why; /* const char * */
		iarg[a++] = p->nargs; /* int */
		uarg[a++] = (intptr_t)p->args; /* void ** */
		*n_args = 3;
		break;
	}
	/* thr_set_name */
	case 464: {
		struct thr_set_name_args *p = params;
		iarg[a++] = p->id; /* long */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 2;
		break;
	}
	/* aio_fsync */
	case 465: {
		struct aio_fsync_args *p = params;
		iarg[a++] = p->op; /* int */
		uarg[a++] = (intptr_t)p->aiocbp; /* struct aiocb * */
		*n_args = 2;
		break;
	}
	/* rtprio_thread */
	case 466: {
		struct rtprio_thread_args *p = params;
		iarg[a++] = p->function; /* int */
		iarg[a++] = p->lwpid; /* lwpid_t */
		uarg[a++] = (intptr_t)p->rtp; /* struct rtprio * */
		*n_args = 3;
		break;
	}
	/* sctp_peeloff */
	case 471: {
		struct sctp_peeloff_args *p = params;
		iarg[a++] = p->sd; /* int */
		uarg[a++] = p->name; /* uint32_t */
		*n_args = 2;
		break;
	}
	/* sctp_generic_sendmsg */
	case 472: {
		struct sctp_generic_sendmsg_args *p = params;
		iarg[a++] = p->sd; /* int */
		uarg[a++] = (intptr_t)p->msg; /* void * */
		iarg[a++] = p->mlen; /* int */
		uarg[a++] = (intptr_t)p->to; /* const struct sockaddr * */
		iarg[a++] = p->tolen; /* __socklen_t */
		uarg[a++] = (intptr_t)p->sinfo; /* struct sctp_sndrcvinfo * */
		iarg[a++] = p->flags; /* int */
		*n_args = 7;
		break;
	}
	/* sctp_generic_sendmsg_iov */
	case 473: {
		struct sctp_generic_sendmsg_iov_args *p = params;
		iarg[a++] = p->sd; /* int */
		uarg[a++] = (intptr_t)p->iov; /* struct iovec * */
		iarg[a++] = p->iovlen; /* int */
		uarg[a++] = (intptr_t)p->to; /* const struct sockaddr * */
		iarg[a++] = p->tolen; /* __socklen_t */
		uarg[a++] = (intptr_t)p->sinfo; /* struct sctp_sndrcvinfo * */
		iarg[a++] = p->flags; /* int */
		*n_args = 7;
		break;
	}
	/* sctp_generic_recvmsg */
	case 474: {
		struct sctp_generic_recvmsg_args *p = params;
		iarg[a++] = p->sd; /* int */
		uarg[a++] = (intptr_t)p->iov; /* struct iovec * */
		iarg[a++] = p->iovlen; /* int */
		uarg[a++] = (intptr_t)p->from; /* struct sockaddr * */
		uarg[a++] = (intptr_t)p->fromlenaddr; /* __socklen_t * */
		uarg[a++] = (intptr_t)p->sinfo; /* struct sctp_sndrcvinfo * */
		uarg[a++] = (intptr_t)p->msg_flags; /* int * */
		*n_args = 7;
		break;
	}
	/* pread */
	case 475: {
		struct pread_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* void * */
		uarg[a++] = p->nbyte; /* size_t */
		iarg[a++] = p->offset; /* off_t */
		*n_args = 4;
		break;
	}
	/* pwrite */
	case 476: {
		struct pwrite_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* const void * */
		uarg[a++] = p->nbyte; /* size_t */
		iarg[a++] = p->offset; /* off_t */
		*n_args = 4;
		break;
	}
	/* mmap */
	case 477: {
		struct mmap_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* void * */
		uarg[a++] = p->len; /* size_t */
		iarg[a++] = p->prot; /* int */
		iarg[a++] = p->flags; /* int */
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->pos; /* off_t */
		*n_args = 6;
		break;
	}
	/* lseek */
	case 478: {
		struct lseek_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->offset; /* off_t */
		iarg[a++] = p->whence; /* int */
		*n_args = 3;
		break;
	}
	/* truncate */
	case 479: {
		struct truncate_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->length; /* off_t */
		*n_args = 2;
		break;
	}
	/* ftruncate */
	case 480: {
		struct ftruncate_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->length; /* off_t */
		*n_args = 2;
		break;
	}
	/* thr_kill2 */
	case 481: {
		struct thr_kill2_args *p = params;
		iarg[a++] = p->pid; /* pid_t */
		iarg[a++] = p->id; /* long */
		iarg[a++] = p->sig; /* int */
		*n_args = 3;
		break;
	}
	/* shm_unlink */
	case 483: {
		struct shm_unlink_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		*n_args = 1;
		break;
	}
	/* cpuset */
	case 484: {
		struct cpuset_args *p = params;
		uarg[a++] = (intptr_t)p->setid; /* cpusetid_t * */
		*n_args = 1;
		break;
	}
	/* cpuset_setid */
	case 485: {
		struct cpuset_setid_args *p = params;
		iarg[a++] = p->which; /* cpuwhich_t */
		iarg[a++] = p->id; /* id_t */
		iarg[a++] = p->setid; /* cpusetid_t */
		*n_args = 3;
		break;
	}
	/* cpuset_getid */
	case 486: {
		struct cpuset_getid_args *p = params;
		iarg[a++] = p->level; /* cpulevel_t */
		iarg[a++] = p->which; /* cpuwhich_t */
		iarg[a++] = p->id; /* id_t */
		uarg[a++] = (intptr_t)p->setid; /* cpusetid_t * */
		*n_args = 4;
		break;
	}
	/* cpuset_getaffinity */
	case 487: {
		struct cpuset_getaffinity_args *p = params;
		iarg[a++] = p->level; /* cpulevel_t */
		iarg[a++] = p->which; /* cpuwhich_t */
		iarg[a++] = p->id; /* id_t */
		uarg[a++] = p->cpusetsize; /* size_t */
		uarg[a++] = (intptr_t)p->mask; /* cpuset_t * */
		*n_args = 5;
		break;
	}
	/* cpuset_setaffinity */
	case 488: {
		struct cpuset_setaffinity_args *p = params;
		iarg[a++] = p->level; /* cpulevel_t */
		iarg[a++] = p->which; /* cpuwhich_t */
		iarg[a++] = p->id; /* id_t */
		uarg[a++] = p->cpusetsize; /* size_t */
		uarg[a++] = (intptr_t)p->mask; /* const cpuset_t * */
		*n_args = 5;
		break;
	}
	/* faccessat */
	case 489: {
		struct faccessat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->amode; /* int */
		iarg[a++] = p->flag; /* int */
		*n_args = 4;
		break;
	}
	/* fchmodat */
	case 490: {
		struct fchmodat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->mode; /* mode_t */
		iarg[a++] = p->flag; /* int */
		*n_args = 4;
		break;
	}
	/* fchownat */
	case 491: {
		struct fchownat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = p->uid; /* uid_t */
		iarg[a++] = p->gid; /* gid_t */
		iarg[a++] = p->flag; /* int */
		*n_args = 5;
		break;
	}
	/* fexecve */
	case 492: {
		struct fexecve_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->argv; /* char ** */
		uarg[a++] = (intptr_t)p->envv; /* char ** */
		*n_args = 3;
		break;
	}
	/* futimesat */
	case 494: {
		struct futimesat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->times; /* const struct timeval * */
		*n_args = 3;
		break;
	}
	/* linkat */
	case 495: {
		struct linkat_args *p = params;
		iarg[a++] = p->fd1; /* int */
		uarg[a++] = (intptr_t)p->path1; /* const char * */
		iarg[a++] = p->fd2; /* int */
		uarg[a++] = (intptr_t)p->path2; /* const char * */
		iarg[a++] = p->flag; /* int */
		*n_args = 5;
		break;
	}
	/* mkdirat */
	case 496: {
		struct mkdirat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->mode; /* mode_t */
		*n_args = 3;
		break;
	}
	/* mkfifoat */
	case 497: {
		struct mkfifoat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->mode; /* mode_t */
		*n_args = 3;
		break;
	}
	/* openat */
	case 499: {
		struct openat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->flag; /* int */
		iarg[a++] = p->mode; /* mode_t */
		*n_args = 4;
		break;
	}
	/* readlinkat */
	case 500: {
		struct readlinkat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		uarg[a++] = p->bufsize; /* size_t */
		*n_args = 4;
		break;
	}
	/* renameat */
	case 501: {
		struct renameat_args *p = params;
		iarg[a++] = p->oldfd; /* int */
		uarg[a++] = (intptr_t)p->old; /* const char * */
		iarg[a++] = p->newfd; /* int */
		uarg[a++] = (intptr_t)p->new; /* const char * */
		*n_args = 4;
		break;
	}
	/* symlinkat */
	case 502: {
		struct symlinkat_args *p = params;
		uarg[a++] = (intptr_t)p->path1; /* const char * */
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path2; /* const char * */
		*n_args = 3;
		break;
	}
	/* unlinkat */
	case 503: {
		struct unlinkat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->flag; /* int */
		*n_args = 3;
		break;
	}
	/* posix_openpt */
	case 504: {
		struct posix_openpt_args *p = params;
		iarg[a++] = p->flags; /* int */
		*n_args = 1;
		break;
	}
	/* jail_get */
	case 506: {
		struct jail_get_args *p = params;
		uarg[a++] = (intptr_t)p->iovp; /* struct iovec * */
		uarg[a++] = p->iovcnt; /* unsigned int */
		iarg[a++] = p->flags; /* int */
		*n_args = 3;
		break;
	}
	/* jail_set */
	case 507: {
		struct jail_set_args *p = params;
		uarg[a++] = (intptr_t)p->iovp; /* struct iovec * */
		uarg[a++] = p->iovcnt; /* unsigned int */
		iarg[a++] = p->flags; /* int */
		*n_args = 3;
		break;
	}
	/* jail_remove */
	case 508: {
		struct jail_remove_args *p = params;
		iarg[a++] = p->jid; /* int */
		*n_args = 1;
		break;
	}
	/* __semctl */
	case 510: {
		struct __semctl_args *p = params;
		iarg[a++] = p->semid; /* int */
		iarg[a++] = p->semnum; /* int */
		iarg[a++] = p->cmd; /* int */
		uarg[a++] = (intptr_t)p->arg; /* union semun * */
		*n_args = 4;
		break;
	}
	/* msgctl */
	case 511: {
		struct msgctl_args *p = params;
		iarg[a++] = p->msqid; /* int */
		iarg[a++] = p->cmd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* struct msqid_ds * */
		*n_args = 3;
		break;
	}
	/* shmctl */
	case 512: {
		struct shmctl_args *p = params;
		iarg[a++] = p->shmid; /* int */
		iarg[a++] = p->cmd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* struct shmid_ds * */
		*n_args = 3;
		break;
	}
	/* lpathconf */
	case 513: {
		struct lpathconf_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->name; /* int */
		*n_args = 2;
		break;
	}
	/* __cap_rights_get */
	case 515: {
		struct __cap_rights_get_args *p = params;
		iarg[a++] = p->version; /* int */
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->rightsp; /* cap_rights_t * */
		*n_args = 3;
		break;
	}
	/* cap_enter */
	case 516: {
		*n_args = 0;
		break;
	}
	/* cap_getmode */
	case 517: {
		struct cap_getmode_args *p = params;
		uarg[a++] = (intptr_t)p->modep; /* u_int * */
		*n_args = 1;
		break;
	}
	/* pdfork */
	case 518: {
		struct pdfork_args *p = params;
		uarg[a++] = (intptr_t)p->fdp; /* int * */
		iarg[a++] = p->flags; /* int */
		*n_args = 2;
		break;
	}
	/* pdkill */
	case 519: {
		struct pdkill_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->signum; /* int */
		*n_args = 2;
		break;
	}
	/* pdgetpid */
	case 520: {
		struct pdgetpid_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->pidp; /* pid_t * */
		*n_args = 2;
		break;
	}
	/* pselect */
	case 522: {
		struct pselect_args *p = params;
		iarg[a++] = p->nd; /* int */
		uarg[a++] = (intptr_t)p->in; /* fd_set * */
		uarg[a++] = (intptr_t)p->ou; /* fd_set * */
		uarg[a++] = (intptr_t)p->ex; /* fd_set * */
		uarg[a++] = (intptr_t)p->ts; /* const struct timespec * */
		uarg[a++] = (intptr_t)p->sm; /* const sigset_t * */
		*n_args = 6;
		break;
	}
	/* getloginclass */
	case 523: {
		struct getloginclass_args *p = params;
		uarg[a++] = (intptr_t)p->namebuf; /* char * */
		uarg[a++] = p->namelen; /* size_t */
		*n_args = 2;
		break;
	}
	/* setloginclass */
	case 524: {
		struct setloginclass_args *p = params;
		uarg[a++] = (intptr_t)p->namebuf; /* const char * */
		*n_args = 1;
		break;
	}
	/* rctl_get_racct */
	case 525: {
		struct rctl_get_racct_args *p = params;
		uarg[a++] = (intptr_t)p->inbufp; /* const void * */
		uarg[a++] = p->inbuflen; /* size_t */
		uarg[a++] = (intptr_t)p->outbufp; /* void * */
		uarg[a++] = p->outbuflen; /* size_t */
		*n_args = 4;
		break;
	}
	/* rctl_get_rules */
	case 526: {
		struct rctl_get_rules_args *p = params;
		uarg[a++] = (intptr_t)p->inbufp; /* const void * */
		uarg[a++] = p->inbuflen; /* size_t */
		uarg[a++] = (intptr_t)p->outbufp; /* void * */
		uarg[a++] = p->outbuflen; /* size_t */
		*n_args = 4;
		break;
	}
	/* rctl_get_limits */
	case 527: {
		struct rctl_get_limits_args *p = params;
		uarg[a++] = (intptr_t)p->inbufp; /* const void * */
		uarg[a++] = p->inbuflen; /* size_t */
		uarg[a++] = (intptr_t)p->outbufp; /* void * */
		uarg[a++] = p->outbuflen; /* size_t */
		*n_args = 4;
		break;
	}
	/* rctl_add_rule */
	case 528: {
		struct rctl_add_rule_args *p = params;
		uarg[a++] = (intptr_t)p->inbufp; /* const void * */
		uarg[a++] = p->inbuflen; /* size_t */
		uarg[a++] = (intptr_t)p->outbufp; /* void * */
		uarg[a++] = p->outbuflen; /* size_t */
		*n_args = 4;
		break;
	}
	/* rctl_remove_rule */
	case 529: {
		struct rctl_remove_rule_args *p = params;
		uarg[a++] = (intptr_t)p->inbufp; /* const void * */
		uarg[a++] = p->inbuflen; /* size_t */
		uarg[a++] = (intptr_t)p->outbufp; /* void * */
		uarg[a++] = p->outbuflen; /* size_t */
		*n_args = 4;
		break;
	}
	/* posix_fallocate */
	case 530: {
		struct posix_fallocate_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->offset; /* off_t */
		iarg[a++] = p->len; /* off_t */
		*n_args = 3;
		break;
	}
	/* posix_fadvise */
	case 531: {
		struct posix_fadvise_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->offset; /* off_t */
		iarg[a++] = p->len; /* off_t */
		iarg[a++] = p->advice; /* int */
		*n_args = 4;
		break;
	}
	/* wait6 */
	case 532: {
		struct wait6_args *p = params;
		iarg[a++] = p->idtype; /* idtype_t */
		iarg[a++] = p->id; /* id_t */
		uarg[a++] = (intptr_t)p->status; /* int * */
		iarg[a++] = p->options; /* int */
		uarg[a++] = (intptr_t)p->wrusage; /* struct __wrusage * */
		uarg[a++] = (intptr_t)p->info; /* struct __siginfo * */
		*n_args = 6;
		break;
	}
	/* cap_rights_limit */
	case 533: {
		struct cap_rights_limit_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->rightsp; /* cap_rights_t * */
		*n_args = 2;
		break;
	}
	/* cap_ioctls_limit */
	case 534: {
		struct cap_ioctls_limit_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->cmds; /* const u_long * */
		uarg[a++] = p->ncmds; /* size_t */
		*n_args = 3;
		break;
	}
	/* cap_ioctls_get */
	case 535: {
		struct cap_ioctls_get_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->cmds; /* u_long * */
		uarg[a++] = p->maxcmds; /* size_t */
		*n_args = 3;
		break;
	}
	/* cap_fcntls_limit */
	case 536: {
		struct cap_fcntls_limit_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = p->fcntlrights; /* uint32_t */
		*n_args = 2;
		break;
	}
	/* cap_fcntls_get */
	case 537: {
		struct cap_fcntls_get_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->fcntlrightsp; /* uint32_t * */
		*n_args = 2;
		break;
	}
	/* bindat */
	case 538: {
		struct bindat_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->s; /* int */
		uarg[a++] = (intptr_t)p->name; /* const struct sockaddr * */
		iarg[a++] = p->namelen; /* __socklen_t */
		*n_args = 4;
		break;
	}
	/* connectat */
	case 539: {
		struct connectat_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->s; /* int */
		uarg[a++] = (intptr_t)p->name; /* const struct sockaddr * */
		iarg[a++] = p->namelen; /* __socklen_t */
		*n_args = 4;
		break;
	}
	/* chflagsat */
	case 540: {
		struct chflagsat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = p->flags; /* u_long */
		iarg[a++] = p->atflag; /* int */
		*n_args = 4;
		break;
	}
	/* accept4 */
	case 541: {
		struct accept4_args *p = params;
		iarg[a++] = p->s; /* int */
		uarg[a++] = (intptr_t)p->name; /* struct sockaddr * */
		uarg[a++] = (intptr_t)p->anamelen; /* __socklen_t * */
		iarg[a++] = p->flags; /* int */
		*n_args = 4;
		break;
	}
	/* pipe2 */
	case 542: {
		struct pipe2_args *p = params;
		uarg[a++] = (intptr_t)p->fildes; /* int * */
		iarg[a++] = p->flags; /* int */
		*n_args = 2;
		break;
	}
	/* aio_mlock */
	case 543: {
		struct aio_mlock_args *p = params;
		uarg[a++] = (intptr_t)p->aiocbp; /* struct aiocb * */
		*n_args = 1;
		break;
	}
	/* procctl */
	case 544: {
		struct procctl_args *p = params;
		iarg[a++] = p->idtype; /* idtype_t */
		iarg[a++] = p->id; /* id_t */
		iarg[a++] = p->com; /* int */
		uarg[a++] = (intptr_t)p->data; /* void * */
		*n_args = 4;
		break;
	}
	/* ppoll */
	case 545: {
		struct ppoll_args *p = params;
		uarg[a++] = (intptr_t)p->fds; /* struct pollfd * */
		uarg[a++] = p->nfds; /* u_int */
		uarg[a++] = (intptr_t)p->ts; /* const struct timespec * */
		uarg[a++] = (intptr_t)p->set; /* const sigset_t * */
		*n_args = 4;
		break;
	}
	/* futimens */
	case 546: {
		struct futimens_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->times; /* const struct timespec * */
		*n_args = 2;
		break;
	}
	/* utimensat */
	case 547: {
		struct utimensat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->times; /* const struct timespec * */
		iarg[a++] = p->flag; /* int */
		*n_args = 4;
		break;
	}
	/* fdatasync */
	case 550: {
		struct fdatasync_args *p = params;
		iarg[a++] = p->fd; /* int */
		*n_args = 1;
		break;
	}
	/* fstat */
	case 551: {
		struct fstat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->sb; /* struct stat * */
		*n_args = 2;
		break;
	}
	/* fstatat */
	case 552: {
		struct fstatat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->buf; /* struct stat * */
		iarg[a++] = p->flag; /* int */
		*n_args = 4;
		break;
	}
	/* fhstat */
	case 553: {
		struct fhstat_args *p = params;
		uarg[a++] = (intptr_t)p->u_fhp; /* const struct fhandle * */
		uarg[a++] = (intptr_t)p->sb; /* struct stat * */
		*n_args = 2;
		break;
	}
	/* getdirentries */
	case 554: {
		struct getdirentries_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		uarg[a++] = p->count; /* size_t */
		uarg[a++] = (intptr_t)p->basep; /* off_t * */
		*n_args = 4;
		break;
	}
	/* statfs */
	case 555: {
		struct statfs_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->buf; /* struct statfs * */
		*n_args = 2;
		break;
	}
	/* fstatfs */
	case 556: {
		struct fstatfs_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* struct statfs * */
		*n_args = 2;
		break;
	}
	/* getfsstat */
	case 557: {
		struct getfsstat_args *p = params;
		uarg[a++] = (intptr_t)p->buf; /* struct statfs * */
		iarg[a++] = p->bufsize; /* long */
		iarg[a++] = p->mode; /* int */
		*n_args = 3;
		break;
	}
	/* fhstatfs */
	case 558: {
		struct fhstatfs_args *p = params;
		uarg[a++] = (intptr_t)p->u_fhp; /* const struct fhandle * */
		uarg[a++] = (intptr_t)p->buf; /* struct statfs * */
		*n_args = 2;
		break;
	}
	/* mknodat */
	case 559: {
		struct mknodat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->mode; /* mode_t */
		iarg[a++] = p->dev; /* dev_t */
		*n_args = 4;
		break;
	}
	/* kevent */
	case 560: {
		struct kevent_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->changelist; /* const struct kevent * */
		iarg[a++] = p->nchanges; /* int */
		uarg[a++] = (intptr_t)p->eventlist; /* struct kevent * */
		iarg[a++] = p->nevents; /* int */
		uarg[a++] = (intptr_t)p->timeout; /* const struct timespec * */
		*n_args = 6;
		break;
	}
	/* cpuset_getdomain */
	case 561: {
		struct cpuset_getdomain_args *p = params;
		iarg[a++] = p->level; /* cpulevel_t */
		iarg[a++] = p->which; /* cpuwhich_t */
		iarg[a++] = p->id; /* id_t */
		uarg[a++] = p->domainsetsize; /* size_t */
		uarg[a++] = (intptr_t)p->mask; /* domainset_t * */
		uarg[a++] = (intptr_t)p->policy; /* int * */
		*n_args = 6;
		break;
	}
	/* cpuset_setdomain */
	case 562: {
		struct cpuset_setdomain_args *p = params;
		iarg[a++] = p->level; /* cpulevel_t */
		iarg[a++] = p->which; /* cpuwhich_t */
		iarg[a++] = p->id; /* id_t */
		uarg[a++] = p->domainsetsize; /* size_t */
		uarg[a++] = (intptr_t)p->mask; /* domainset_t * */
		iarg[a++] = p->policy; /* int */
		*n_args = 6;
		break;
	}
	/* getrandom */
	case 563: {
		struct getrandom_args *p = params;
		uarg[a++] = (intptr_t)p->buf; /* void * */
		uarg[a++] = p->buflen; /* size_t */
		uarg[a++] = p->flags; /* unsigned int */
		*n_args = 3;
		break;
	}
	/* getfhat */
	case 564: {
		struct getfhat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* char * */
		uarg[a++] = (intptr_t)p->fhp; /* struct fhandle * */
		iarg[a++] = p->flags; /* int */
		*n_args = 4;
		break;
	}
	/* fhlink */
	case 565: {
		struct fhlink_args *p = params;
		uarg[a++] = (intptr_t)p->fhp; /* struct fhandle * */
		uarg[a++] = (intptr_t)p->to; /* const char * */
		*n_args = 2;
		break;
	}
	/* fhlinkat */
	case 566: {
		struct fhlinkat_args *p = params;
		uarg[a++] = (intptr_t)p->fhp; /* struct fhandle * */
		iarg[a++] = p->tofd; /* int */
		uarg[a++] = (intptr_t)p->to; /* const char * */
		*n_args = 3;
		break;
	}
	/* fhreadlink */
	case 567: {
		struct fhreadlink_args *p = params;
		uarg[a++] = (intptr_t)p->fhp; /* struct fhandle * */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		uarg[a++] = p->bufsize; /* size_t */
		*n_args = 3;
		break;
	}
	/* funlinkat */
	case 568: {
		struct funlinkat_args *p = params;
		iarg[a++] = p->dfd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->flag; /* int */
		*n_args = 4;
		break;
	}
	/* copy_file_range */
	case 569: {
		struct copy_file_range_args *p = params;
		iarg[a++] = p->infd; /* int */
		uarg[a++] = (intptr_t)p->inoffp; /* off_t * */
		iarg[a++] = p->outfd; /* int */
		uarg[a++] = (intptr_t)p->outoffp; /* off_t * */
		uarg[a++] = p->len; /* size_t */
		uarg[a++] = p->flags; /* unsigned int */
		*n_args = 6;
		break;
	}
	/* __sysctlbyname */
	case 570: {
		struct __sysctlbyname_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = p->namelen; /* size_t */
		uarg[a++] = (intptr_t)p->old; /* void * */
		uarg[a++] = (intptr_t)p->oldlenp; /* size_t * */
		uarg[a++] = (intptr_t)p->new; /* void * */
		uarg[a++] = p->newlen; /* size_t */
		*n_args = 6;
		break;
	}
	/* shm_open2 */
	case 571: {
		struct shm_open2_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		iarg[a++] = p->flags; /* int */
		iarg[a++] = p->mode; /* mode_t */
		iarg[a++] = p->shmflags; /* int */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 5;
		break;
	}
	/* shm_rename */
	case 572: {
		struct shm_rename_args *p = params;
		uarg[a++] = (intptr_t)p->path_from; /* const char * */
		uarg[a++] = (intptr_t)p->path_to; /* const char * */
		iarg[a++] = p->flags; /* int */
		*n_args = 3;
		break;
	}
	/* sigfastblock */
	case 573: {
		struct sigfastblock_args *p = params;
		iarg[a++] = p->cmd; /* int */
		uarg[a++] = (intptr_t)p->ptr; /* void * */
		*n_args = 2;
		break;
	}
	/* __realpathat */
	case 574: {
		struct __realpathat_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		uarg[a++] = p->size; /* size_t */
		iarg[a++] = p->flags; /* int */
		*n_args = 5;
		break;
	}
	/* close_range */
	case 575: {
		struct close_range_args *p = params;
		uarg[a++] = p->lowfd; /* u_int */
		uarg[a++] = p->highfd; /* u_int */
		iarg[a++] = p->flags; /* int */
		*n_args = 3;
		break;
	}
	/* rpctls_syscall */
	case 576: {
		struct rpctls_syscall_args *p = params;
		uarg[a++] = p->socookie; /* uint64_t */
		*n_args = 1;
		break;
	}
	/* __specialfd */
	case 577: {
		struct __specialfd_args *p = params;
		iarg[a++] = p->type; /* int */
		uarg[a++] = (intptr_t)p->req; /* const void * */
		uarg[a++] = p->len; /* size_t */
		*n_args = 3;
		break;
	}
	/* aio_writev */
	case 578: {
		struct aio_writev_args *p = params;
		uarg[a++] = (intptr_t)p->aiocbp; /* struct aiocb * */
		*n_args = 1;
		break;
	}
	/* aio_readv */
	case 579: {
		struct aio_readv_args *p = params;
		uarg[a++] = (intptr_t)p->aiocbp; /* struct aiocb * */
		*n_args = 1;
		break;
	}
	/* fspacectl */
	case 580: {
		struct fspacectl_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->cmd; /* int */
		uarg[a++] = (intptr_t)p->rqsr; /* const struct spacectl_range * */
		iarg[a++] = p->flags; /* int */
		uarg[a++] = (intptr_t)p->rmsr; /* struct spacectl_range * */
		*n_args = 5;
		break;
	}
	/* sched_getcpu */
	case 581: {
		*n_args = 0;
		break;
	}
	/* swapoff */
	case 582: {
		struct swapoff_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = p->flags; /* u_int */
		*n_args = 2;
		break;
	}
	/* kqueuex */
	case 583: {
		struct kqueuex_args *p = params;
		uarg[a++] = p->flags; /* u_int */
		*n_args = 1;
		break;
	}
	/* membarrier */
	case 584: {
		struct membarrier_args *p = params;
		iarg[a++] = p->cmd; /* int */
		uarg[a++] = p->flags; /* unsigned */
		iarg[a++] = p->cpu_id; /* int */
		*n_args = 3;
		break;
	}
	/* timerfd_create */
	case 585: {
		struct timerfd_create_args *p = params;
		iarg[a++] = p->clockid; /* int */
		iarg[a++] = p->flags; /* int */
		*n_args = 2;
		break;
	}
	/* timerfd_gettime */
	case 586: {
		struct timerfd_gettime_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->curr_value; /* struct itimerspec * */
		*n_args = 2;
		break;
	}
	/* timerfd_settime */
	case 587: {
		struct timerfd_settime_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->flags; /* int */
		uarg[a++] = (intptr_t)p->new_value; /* const struct itimerspec * */
		uarg[a++] = (intptr_t)p->old_value; /* struct itimerspec * */
		*n_args = 4;
		break;
	}
	/* kcmp */
	case 588: {
		struct kcmp_args *p = params;
		iarg[a++] = p->pid1; /* pid_t */
		iarg[a++] = p->pid2; /* pid_t */
		iarg[a++] = p->type; /* int */
		uarg[a++] = (intptr_t)p->idx1; /* uintptr_t */
		uarg[a++] = (intptr_t)p->idx2; /* uintptr_t */
		*n_args = 5;
		break;
	}
	/* getrlimitusage */
	case 589: {
		struct getrlimitusage_args *p = params;
		uarg[a++] = p->which; /* u_int */
		iarg[a++] = p->flags; /* int */
		uarg[a++] = (intptr_t)p->res; /* rlim_t * */
		*n_args = 3;
		break;
	}
	/* fchroot */
	case 590: {
		struct fchroot_args *p = params;
		iarg[a++] = p->fd; /* int */
		*n_args = 1;
		break;
	}
	/* setcred */
	case 591: {
		struct setcred_args *p = params;
		uarg[a++] = p->flags; /* u_int */
		uarg[a++] = (intptr_t)p->wcred; /* const struct setcred * */
		uarg[a++] = p->size; /* size_t */
		*n_args = 3;
		break;
	}
	/* exterrctl */
	case 592: {
		struct exterrctl_args *p = params;
		uarg[a++] = p->op; /* u_int */
		uarg[a++] = p->flags; /* u_int */
		uarg[a++] = (intptr_t)p->ptr; /* void * */
		*n_args = 3;
		break;
	}
	/* inotify_add_watch_at */
	case 593: {
		struct inotify_add_watch_at_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->dfd; /* int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = p->mask; /* uint32_t */
		*n_args = 4;
		break;
	}
	/* inotify_rm_watch */
	case 594: {
		struct inotify_rm_watch_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->wd; /* int */
		*n_args = 2;
		break;
	}
	default:
		*n_args = 0;
		break;
	};
}
static void
systrace_entry_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
	/* syscall */
	case 0:
		break;
	/* exit */
	case 1:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* fork */
	case 2:
		break;
	/* read */
	case 3:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* write */
	case 4:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const void *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* open */
	case 5:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "mode_t";
			break;
		default:
			break;
		};
		break;
	/* close */
	case 6:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* wait4 */
	case 7:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland int *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland struct rusage *";
			break;
		default:
			break;
		};
		break;
	/* link */
	case 9:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* unlink */
	case 10:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* chdir */
	case 12:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* fchdir */
	case 13:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* chmod */
	case 15:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "mode_t";
			break;
		default:
			break;
		};
		break;
	/* chown */
	case 16:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* break */
	case 17:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* getpid */
	case 20:
		break;
	/* mount */
	case 21:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* unmount */
	case 22:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* setuid */
	case 23:
		switch (ndx) {
		case 0:
			p = "uid_t";
			break;
		default:
			break;
		};
		break;
	/* getuid */
	case 24:
		break;
	/* geteuid */
	case 25:
		break;
	/* ptrace */
	case 26:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "pid_t";
			break;
		case 2:
			p = "caddr_t";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* recvmsg */
	case 27:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct msghdr *";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* sendmsg */
	case 28:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct msghdr *";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* recvfrom */
	case 29:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "int";
			break;
		case 4:
			p = "userland struct sockaddr *";
			break;
		case 5:
			p = "userland __socklen_t *";
			break;
		default:
			break;
		};
		break;
	/* accept */
	case 30:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct sockaddr *";
			break;
		case 2:
			p = "userland __socklen_t *";
			break;
		default:
			break;
		};
		break;
	/* getpeername */
	case 31:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct sockaddr *";
			break;
		case 2:
			p = "userland __socklen_t *";
			break;
		default:
			break;
		};
		break;
	/* getsockname */
	case 32:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct sockaddr *";
			break;
		case 2:
			p = "userland __socklen_t *";
			break;
		default:
			break;
		};
		break;
	/* access */
	case 33:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* chflags */
	case 34:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "u_long";
			break;
		default:
			break;
		};
		break;
	/* fchflags */
	case 35:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "u_long";
			break;
		default:
			break;
		};
		break;
	/* sync */
	case 36:
		break;
	/* kill */
	case 37:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* getppid */
	case 39:
		break;
	/* dup */
	case 41:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* getegid */
	case 43:
		break;
	/* profil */
	case 44:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* ktrace */
	case 45:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* getgid */
	case 47:
		break;
	/* getlogin */
	case 49:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* setlogin */
	case 50:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* acct */
	case 51:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* sigaltstack */
	case 53:
		switch (ndx) {
		case 0:
			p = "userland const struct sigaltstack *";
			break;
		case 1:
			p = "userland struct sigaltstack *";
			break;
		default:
			break;
		};
		break;
	/* ioctl */
	case 54:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "u_long";
			break;
		case 2:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* reboot */
	case 55:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* revoke */
	case 56:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* symlink */
	case 57:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* readlink */
	case 58:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* execve */
	case 59:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland char **";
			break;
		case 2:
			p = "userland char **";
			break;
		default:
			break;
		};
		break;
	/* umask */
	case 60:
		switch (ndx) {
		case 0:
			p = "mode_t";
			break;
		default:
			break;
		};
		break;
	/* chroot */
	case 61:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* msync */
	case 65:
		switch (ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* vfork */
	case 66:
		break;
	/* munmap */
	case 73:
		switch (ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* mprotect */
	case 74:
		switch (ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* madvise */
	case 75:
		switch (ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* mincore */
	case 78:
		switch (ndx) {
		case 0:
			p = "userland const void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* getgroups */
	case 79:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland gid_t *";
			break;
		default:
			break;
		};
		break;
	/* setgroups */
	case 80:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const gid_t *";
			break;
		default:
			break;
		};
		break;
	/* getpgrp */
	case 81:
		break;
	/* setpgid */
	case 82:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* setitimer */
	case 83:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct itimerval *";
			break;
		case 2:
			p = "userland struct itimerval *";
			break;
		default:
			break;
		};
		break;
	/* swapon */
	case 85:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* getitimer */
	case 86:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct itimerval *";
			break;
		default:
			break;
		};
		break;
	/* getdtablesize */
	case 89:
		break;
	/* dup2 */
	case 90:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		case 1:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* fcntl */
	case 92:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "intptr_t";
			break;
		default:
			break;
		};
		break;
	/* select */
	case 93:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland fd_set *";
			break;
		case 2:
			p = "userland fd_set *";
			break;
		case 3:
			p = "userland fd_set *";
			break;
		case 4:
			p = "userland struct timeval *";
			break;
		default:
			break;
		};
		break;
	/* fsync */
	case 95:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* setpriority */
	case 96:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* socket */
	case 97:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* connect */
	case 98:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct sockaddr *";
			break;
		case 2:
			p = "__socklen_t";
			break;
		default:
			break;
		};
		break;
	/* getpriority */
	case 100:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* bind */
	case 104:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct sockaddr *";
			break;
		case 2:
			p = "__socklen_t";
			break;
		default:
			break;
		};
		break;
	/* setsockopt */
	case 105:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland const void *";
			break;
		case 4:
			p = "__socklen_t";
			break;
		default:
			break;
		};
		break;
	/* listen */
	case 106:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* gettimeofday */
	case 116:
		switch (ndx) {
		case 0:
			p = "userland struct timeval *";
			break;
		case 1:
			p = "userland struct timezone *";
			break;
		default:
			break;
		};
		break;
	/* getrusage */
	case 117:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct rusage *";
			break;
		default:
			break;
		};
		break;
	/* getsockopt */
	case 118:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland void *";
			break;
		case 4:
			p = "userland __socklen_t *";
			break;
		default:
			break;
		};
		break;
	/* readv */
	case 120:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct iovec *";
			break;
		case 2:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* writev */
	case 121:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct iovec *";
			break;
		case 2:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* settimeofday */
	case 122:
		switch (ndx) {
		case 0:
			p = "userland const struct timeval *";
			break;
		case 1:
			p = "userland const struct timezone *";
			break;
		default:
			break;
		};
		break;
	/* fchown */
	case 123:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* fchmod */
	case 124:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "mode_t";
			break;
		default:
			break;
		};
		break;
	/* setreuid */
	case 126:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* setregid */
	case 127:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* rename */
	case 128:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* flock */
	case 131:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* mkfifo */
	case 132:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "mode_t";
			break;
		default:
			break;
		};
		break;
	/* sendto */
	case 133:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const void *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "int";
			break;
		case 4:
			p = "userland const struct sockaddr *";
			break;
		case 5:
			p = "__socklen_t";
			break;
		default:
			break;
		};
		break;
	/* shutdown */
	case 134:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* socketpair */
	case 135:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland int *";
			break;
		default:
			break;
		};
		break;
	/* mkdir */
	case 136:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "mode_t";
			break;
		default:
			break;
		};
		break;
	/* rmdir */
	case 137:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* utimes */
	case 138:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland const struct timeval *";
			break;
		default:
			break;
		};
		break;
	/* adjtime */
	case 140:
		switch (ndx) {
		case 0:
			p = "userland const struct timeval *";
			break;
		case 1:
			p = "userland struct timeval *";
			break;
		default:
			break;
		};
		break;
	/* setsid */
	case 147:
		break;
	/* quotactl */
	case 148:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* nlm_syscall */
	case 154:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland char **";
			break;
		default:
			break;
		};
		break;
	/* nfssvc */
	case 155:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* lgetfh */
	case 160:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland struct fhandle *";
			break;
		default:
			break;
		};
		break;
	/* getfh */
	case 161:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland struct fhandle *";
			break;
		default:
			break;
		};
		break;
	/* sysarch */
	case 165:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* rtprio */
	case 166:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "pid_t";
			break;
		case 2:
			p = "userland struct rtprio *";
			break;
		default:
			break;
		};
		break;
	/* semsys */
	case 169:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "int";
			break;
		case 4:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* msgsys */
	case 170:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "int";
			break;
		case 4:
			p = "int";
			break;
		case 5:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* shmsys */
	case 171:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* setfib */
	case 175:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* ntp_adjtime */
	case 176:
		switch (ndx) {
		case 0:
			p = "userland struct timex *";
			break;
		default:
			break;
		};
		break;
	/* setgid */
	case 181:
		switch (ndx) {
		case 0:
			p = "gid_t";
			break;
		default:
			break;
		};
		break;
	/* setegid */
	case 182:
		switch (ndx) {
		case 0:
			p = "gid_t";
			break;
		default:
			break;
		};
		break;
	/* seteuid */
	case 183:
		switch (ndx) {
		case 0:
			p = "uid_t";
			break;
		default:
			break;
		};
		break;
	/* pathconf */
	case 191:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* fpathconf */
	case 192:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* getrlimit */
	case 194:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		case 1:
			p = "userland struct rlimit *";
			break;
		default:
			break;
		};
		break;
	/* setrlimit */
	case 195:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		case 1:
			p = "userland struct rlimit *";
			break;
		default:
			break;
		};
		break;
	/* __syscall */
	case 198:
		break;
	/* __sysctl */
	case 202:
		switch (ndx) {
		case 0:
			p = "userland int *";
			break;
		case 1:
			p = "u_int";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "userland size_t *";
			break;
		case 4:
			p = "userland const void *";
			break;
		case 5:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* mlock */
	case 203:
		switch (ndx) {
		case 0:
			p = "userland const void *";
			break;
		case 1:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* munlock */
	case 204:
		switch (ndx) {
		case 0:
			p = "userland const void *";
			break;
		case 1:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* undelete */
	case 205:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* futimes */
	case 206:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct timeval *";
			break;
		default:
			break;
		};
		break;
	/* getpgid */
	case 207:
		switch (ndx) {
		case 0:
			p = "pid_t";
			break;
		default:
			break;
		};
		break;
	/* poll */
	case 209:
		switch (ndx) {
		case 0:
			p = "userland struct pollfd *";
			break;
		case 1:
			p = "u_int";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* lkmnosys */
	case 210:
		break;
	/* lkmnosys */
	case 211:
		break;
	/* lkmnosys */
	case 212:
		break;
	/* lkmnosys */
	case 213:
		break;
	/* lkmnosys */
	case 214:
		break;
	/* lkmnosys */
	case 215:
		break;
	/* lkmnosys */
	case 216:
		break;
	/* lkmnosys */
	case 217:
		break;
	/* lkmnosys */
	case 218:
		break;
	/* lkmnosys */
	case 219:
		break;
	/* semget */
	case 221:
		switch (ndx) {
		case 0:
			p = "key_t";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* semop */
	case 222:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct sembuf *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* msgget */
	case 225:
		switch (ndx) {
		case 0:
			p = "key_t";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* msgsnd */
	case 226:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const void *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* msgrcv */
	case 227:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "long";
			break;
		case 4:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* shmat */
	case 228:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const void *";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* shmdt */
	case 230:
		switch (ndx) {
		case 0:
			p = "userland const void *";
			break;
		default:
			break;
		};
		break;
	/* shmget */
	case 231:
		switch (ndx) {
		case 0:
			p = "key_t";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* clock_gettime */
	case 232:
		switch (ndx) {
		case 0:
			p = "clockid_t";
			break;
		case 1:
			p = "userland struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* clock_settime */
	case 233:
		switch (ndx) {
		case 0:
			p = "clockid_t";
			break;
		case 1:
			p = "userland const struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* clock_getres */
	case 234:
		switch (ndx) {
		case 0:
			p = "clockid_t";
			break;
		case 1:
			p = "userland struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* ktimer_create */
	case 235:
		switch (ndx) {
		case 0:
			p = "clockid_t";
			break;
		case 1:
			p = "userland struct sigevent *";
			break;
		case 2:
			p = "userland int *";
			break;
		default:
			break;
		};
		break;
	/* ktimer_delete */
	case 236:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* ktimer_settime */
	case 237:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const struct itimerspec *";
			break;
		case 3:
			p = "userland struct itimerspec *";
			break;
		default:
			break;
		};
		break;
	/* ktimer_gettime */
	case 238:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct itimerspec *";
			break;
		default:
			break;
		};
		break;
	/* ktimer_getoverrun */
	case 239:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* nanosleep */
	case 240:
		switch (ndx) {
		case 0:
			p = "userland const struct timespec *";
			break;
		case 1:
			p = "userland struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* ffclock_getcounter */
	case 241:
		switch (ndx) {
		case 0:
			p = "userland ffcounter *";
			break;
		default:
			break;
		};
		break;
	/* ffclock_setestimate */
	case 242:
		switch (ndx) {
		case 0:
			p = "userland struct ffclock_estimate *";
			break;
		default:
			break;
		};
		break;
	/* ffclock_getestimate */
	case 243:
		switch (ndx) {
		case 0:
			p = "userland struct ffclock_estimate *";
			break;
		default:
			break;
		};
		break;
	/* clock_nanosleep */
	case 244:
		switch (ndx) {
		case 0:
			p = "clockid_t";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const struct timespec *";
			break;
		case 3:
			p = "userland struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* clock_getcpuclockid2 */
	case 247:
		switch (ndx) {
		case 0:
			p = "id_t";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland clockid_t *";
			break;
		default:
			break;
		};
		break;
	/* ntp_gettime */
	case 248:
		switch (ndx) {
		case 0:
			p = "userland struct ntptimeval *";
			break;
		default:
			break;
		};
		break;
	/* minherit */
	case 250:
		switch (ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* rfork */
	case 251:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* issetugid */
	case 253:
		break;
	/* lchown */
	case 254:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* aio_read */
	case 255:
		switch (ndx) {
		case 0:
			p = "userland struct aiocb *";
			break;
		default:
			break;
		};
		break;
	/* aio_write */
	case 256:
		switch (ndx) {
		case 0:
			p = "userland struct aiocb *";
			break;
		default:
			break;
		};
		break;
	/* lio_listio */
	case 257:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct aiocb * const *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland struct sigevent *";
			break;
		default:
			break;
		};
		break;
	/* lchmod */
	case 274:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "mode_t";
			break;
		default:
			break;
		};
		break;
	/* lutimes */
	case 276:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland const struct timeval *";
			break;
		default:
			break;
		};
		break;
	/* preadv */
	case 289:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct iovec *";
			break;
		case 2:
			p = "u_int";
			break;
		case 3:
			p = "off_t";
			break;
		default:
			break;
		};
		break;
	/* pwritev */
	case 290:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct iovec *";
			break;
		case 2:
			p = "u_int";
			break;
		case 3:
			p = "off_t";
			break;
		default:
			break;
		};
		break;
	/* fhopen */
	case 298:
		switch (ndx) {
		case 0:
			p = "userland const struct fhandle *";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* modnext */
	case 300:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* modstat */
	case 301:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct module_stat *";
			break;
		default:
			break;
		};
		break;
	/* modfnext */
	case 302:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* modfind */
	case 303:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* kldload */
	case 304:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* kldunload */
	case 305:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* kldfind */
	case 306:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* kldnext */
	case 307:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* kldstat */
	case 308:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct kld_file_stat *";
			break;
		default:
			break;
		};
		break;
	/* kldfirstmod */
	case 309:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* getsid */
	case 310:
		switch (ndx) {
		case 0:
			p = "pid_t";
			break;
		default:
			break;
		};
		break;
	/* setresuid */
	case 311:
		switch (ndx) {
		case 0:
			p = "uid_t";
			break;
		case 1:
			p = "uid_t";
			break;
		case 2:
			p = "uid_t";
			break;
		default:
			break;
		};
		break;
	/* setresgid */
	case 312:
		switch (ndx) {
		case 0:
			p = "gid_t";
			break;
		case 1:
			p = "gid_t";
			break;
		case 2:
			p = "gid_t";
			break;
		default:
			break;
		};
		break;
	/* aio_return */
	case 314:
		switch (ndx) {
		case 0:
			p = "userland struct aiocb *";
			break;
		default:
			break;
		};
		break;
	/* aio_suspend */
	case 315:
		switch (ndx) {
		case 0:
			p = "userland const struct aiocb * const *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* aio_cancel */
	case 316:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct aiocb *";
			break;
		default:
			break;
		};
		break;
	/* aio_error */
	case 317:
		switch (ndx) {
		case 0:
			p = "userland struct aiocb *";
			break;
		default:
			break;
		};
		break;
	/* yield */
	case 321:
		break;
	/* mlockall */
	case 324:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* munlockall */
	case 325:
		break;
	/* __getcwd */
	case 326:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* sched_setparam */
	case 327:
		switch (ndx) {
		case 0:
			p = "pid_t";
			break;
		case 1:
			p = "userland const struct sched_param *";
			break;
		default:
			break;
		};
		break;
	/* sched_getparam */
	case 328:
		switch (ndx) {
		case 0:
			p = "pid_t";
			break;
		case 1:
			p = "userland struct sched_param *";
			break;
		default:
			break;
		};
		break;
	/* sched_setscheduler */
	case 329:
		switch (ndx) {
		case 0:
			p = "pid_t";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const struct sched_param *";
			break;
		default:
			break;
		};
		break;
	/* sched_getscheduler */
	case 330:
		switch (ndx) {
		case 0:
			p = "pid_t";
			break;
		default:
			break;
		};
		break;
	/* sched_yield */
	case 331:
		break;
	/* sched_get_priority_max */
	case 332:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* sched_get_priority_min */
	case 333:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* sched_rr_get_interval */
	case 334:
		switch (ndx) {
		case 0:
			p = "pid_t";
			break;
		case 1:
			p = "userland struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* utrace */
	case 335:
		switch (ndx) {
		case 0:
			p = "userland const void *";
			break;
		case 1:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* kldsym */
	case 337:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* jail */
	case 338:
		switch (ndx) {
		case 0:
			p = "userland struct jail *";
			break;
		default:
			break;
		};
		break;
	/* nnpfs_syscall */
	case 339:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland void *";
			break;
		case 4:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* sigprocmask */
	case 340:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const sigset_t *";
			break;
		case 2:
			p = "userland sigset_t *";
			break;
		default:
			break;
		};
		break;
	/* sigsuspend */
	case 341:
		switch (ndx) {
		case 0:
			p = "userland const sigset_t *";
			break;
		default:
			break;
		};
		break;
	/* sigpending */
	case 343:
		switch (ndx) {
		case 0:
			p = "userland sigset_t *";
			break;
		default:
			break;
		};
		break;
	/* sigtimedwait */
	case 345:
		switch (ndx) {
		case 0:
			p = "userland const sigset_t *";
			break;
		case 1:
			p = "userland struct __siginfo *";
			break;
		case 2:
			p = "userland const struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* sigwaitinfo */
	case 346:
		switch (ndx) {
		case 0:
			p = "userland const sigset_t *";
			break;
		case 1:
			p = "userland struct __siginfo *";
			break;
		default:
			break;
		};
		break;
	/* __acl_get_file */
	case 347:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		case 2:
			p = "userland struct acl *";
			break;
		default:
			break;
		};
		break;
	/* __acl_set_file */
	case 348:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		case 2:
			p = "userland struct acl *";
			break;
		default:
			break;
		};
		break;
	/* __acl_get_fd */
	case 349:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		case 2:
			p = "userland struct acl *";
			break;
		default:
			break;
		};
		break;
	/* __acl_set_fd */
	case 350:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		case 2:
			p = "userland struct acl *";
			break;
		default:
			break;
		};
		break;
	/* __acl_delete_file */
	case 351:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		default:
			break;
		};
		break;
	/* __acl_delete_fd */
	case 352:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		default:
			break;
		};
		break;
	/* __acl_aclcheck_file */
	case 353:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		case 2:
			p = "userland struct acl *";
			break;
		default:
			break;
		};
		break;
	/* __acl_aclcheck_fd */
	case 354:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		case 2:
			p = "userland struct acl *";
			break;
		default:
			break;
		};
		break;
	/* extattrctl */
	case 355:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		case 3:
			p = "int";
			break;
		case 4:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* extattr_set_file */
	case 356:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		case 3:
			p = "userland void *";
			break;
		case 4:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* extattr_get_file */
	case 357:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		case 3:
			p = "userland void *";
			break;
		case 4:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* extattr_delete_file */
	case 358:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* aio_waitcomplete */
	case 359:
		switch (ndx) {
		case 0:
			p = "userland struct aiocb **";
			break;
		case 1:
			p = "userland struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* getresuid */
	case 360:
		switch (ndx) {
		case 0:
			p = "userland uid_t *";
			break;
		case 1:
			p = "userland uid_t *";
			break;
		case 2:
			p = "userland uid_t *";
			break;
		default:
			break;
		};
		break;
	/* getresgid */
	case 361:
		switch (ndx) {
		case 0:
			p = "userland gid_t *";
			break;
		case 1:
			p = "userland gid_t *";
			break;
		case 2:
			p = "userland gid_t *";
			break;
		default:
			break;
		};
		break;
	/* kqueue */
	case 362:
		break;
	/* extattr_set_fd */
	case 371:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		case 3:
			p = "userland void *";
			break;
		case 4:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* extattr_get_fd */
	case 372:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		case 3:
			p = "userland void *";
			break;
		case 4:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* extattr_delete_fd */
	case 373:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* __setugid */
	case 374:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* eaccess */
	case 376:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* afs3_syscall */
	case 377:
		switch (ndx) {
		case 0:
			p = "long";
			break;
		case 1:
			p = "long";
			break;
		case 2:
			p = "long";
			break;
		case 3:
			p = "long";
			break;
		case 4:
			p = "long";
			break;
		case 5:
			p = "long";
			break;
		case 6:
			p = "long";
			break;
		default:
			break;
		};
		break;
	/* nmount */
	case 378:
		switch (ndx) {
		case 0:
			p = "userland struct iovec *";
			break;
		case 1:
			p = "unsigned int";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* __mac_get_proc */
	case 384:
		switch (ndx) {
		case 0:
			p = "userland struct mac *";
			break;
		default:
			break;
		};
		break;
	/* __mac_set_proc */
	case 385:
		switch (ndx) {
		case 0:
			p = "userland struct mac *";
			break;
		default:
			break;
		};
		break;
	/* __mac_get_fd */
	case 386:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct mac *";
			break;
		default:
			break;
		};
		break;
	/* __mac_get_file */
	case 387:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland struct mac *";
			break;
		default:
			break;
		};
		break;
	/* __mac_set_fd */
	case 388:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct mac *";
			break;
		default:
			break;
		};
		break;
	/* __mac_set_file */
	case 389:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland struct mac *";
			break;
		default:
			break;
		};
		break;
	/* kenv */
	case 390:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland char *";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* lchflags */
	case 391:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "u_long";
			break;
		default:
			break;
		};
		break;
	/* uuidgen */
	case 392:
		switch (ndx) {
		case 0:
			p = "userland struct uuid *";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* sendfile */
	case 393:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "off_t";
			break;
		case 3:
			p = "size_t";
			break;
		case 4:
			p = "userland struct sf_hdtr *";
			break;
		case 5:
			p = "userland off_t *";
			break;
		case 6:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* mac_syscall */
	case 394:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* ksem_close */
	case 400:
		switch (ndx) {
		case 0:
			p = "semid_t";
			break;
		default:
			break;
		};
		break;
	/* ksem_post */
	case 401:
		switch (ndx) {
		case 0:
			p = "semid_t";
			break;
		default:
			break;
		};
		break;
	/* ksem_wait */
	case 402:
		switch (ndx) {
		case 0:
			p = "semid_t";
			break;
		default:
			break;
		};
		break;
	/* ksem_trywait */
	case 403:
		switch (ndx) {
		case 0:
			p = "semid_t";
			break;
		default:
			break;
		};
		break;
	/* ksem_init */
	case 404:
		switch (ndx) {
		case 0:
			p = "userland semid_t *";
			break;
		case 1:
			p = "unsigned int";
			break;
		default:
			break;
		};
		break;
	/* ksem_open */
	case 405:
		switch (ndx) {
		case 0:
			p = "userland semid_t *";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "mode_t";
			break;
		case 4:
			p = "unsigned int";
			break;
		default:
			break;
		};
		break;
	/* ksem_unlink */
	case 406:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* ksem_getvalue */
	case 407:
		switch (ndx) {
		case 0:
			p = "semid_t";
			break;
		case 1:
			p = "userland int *";
			break;
		default:
			break;
		};
		break;
	/* ksem_destroy */
	case 408:
		switch (ndx) {
		case 0:
			p = "semid_t";
			break;
		default:
			break;
		};
		break;
	/* __mac_get_pid */
	case 409:
		switch (ndx) {
		case 0:
			p = "pid_t";
			break;
		case 1:
			p = "userland struct mac *";
			break;
		default:
			break;
		};
		break;
	/* __mac_get_link */
	case 410:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland struct mac *";
			break;
		default:
			break;
		};
		break;
	/* __mac_set_link */
	case 411:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland struct mac *";
			break;
		default:
			break;
		};
		break;
	/* extattr_set_link */
	case 412:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		case 3:
			p = "userland void *";
			break;
		case 4:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* extattr_get_link */
	case 413:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		case 3:
			p = "userland void *";
			break;
		case 4:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* extattr_delete_link */
	case 414:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* __mac_execve */
	case 415:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland char **";
			break;
		case 2:
			p = "userland char **";
			break;
		case 3:
			p = "userland struct mac *";
			break;
		default:
			break;
		};
		break;
	/* sigaction */
	case 416:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct sigaction *";
			break;
		case 2:
			p = "userland struct sigaction *";
			break;
		default:
			break;
		};
		break;
	/* sigreturn */
	case 417:
		switch (ndx) {
		case 0:
			p = "userland const struct __ucontext *";
			break;
		default:
			break;
		};
		break;
	/* getcontext */
	case 421:
		switch (ndx) {
		case 0:
			p = "userland struct __ucontext *";
			break;
		default:
			break;
		};
		break;
	/* setcontext */
	case 422:
		switch (ndx) {
		case 0:
			p = "userland const struct __ucontext *";
			break;
		default:
			break;
		};
		break;
	/* swapcontext */
	case 423:
		switch (ndx) {
		case 0:
			p = "userland struct __ucontext *";
			break;
		case 1:
			p = "userland const struct __ucontext *";
			break;
		default:
			break;
		};
		break;
	/* __acl_get_link */
	case 425:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		case 2:
			p = "userland struct acl *";
			break;
		default:
			break;
		};
		break;
	/* __acl_set_link */
	case 426:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		case 2:
			p = "userland struct acl *";
			break;
		default:
			break;
		};
		break;
	/* __acl_delete_link */
	case 427:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		default:
			break;
		};
		break;
	/* __acl_aclcheck_link */
	case 428:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "__acl_type_t";
			break;
		case 2:
			p = "userland struct acl *";
			break;
		default:
			break;
		};
		break;
	/* sigwait */
	case 429:
		switch (ndx) {
		case 0:
			p = "userland const sigset_t *";
			break;
		case 1:
			p = "userland int *";
			break;
		default:
			break;
		};
		break;
	/* thr_create */
	case 430:
		switch (ndx) {
		case 0:
			p = "userland ucontext_t *";
			break;
		case 1:
			p = "userland long *";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* thr_exit */
	case 431:
		switch (ndx) {
		case 0:
			p = "userland long *";
			break;
		default:
			break;
		};
		break;
	/* thr_self */
	case 432:
		switch (ndx) {
		case 0:
			p = "userland long *";
			break;
		default:
			break;
		};
		break;
	/* thr_kill */
	case 433:
		switch (ndx) {
		case 0:
			p = "long";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* jail_attach */
	case 436:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* extattr_list_fd */
	case 437:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* extattr_list_file */
	case 438:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* extattr_list_link */
	case 439:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* ksem_timedwait */
	case 441:
		switch (ndx) {
		case 0:
			p = "semid_t";
			break;
		case 1:
			p = "userland const struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* thr_suspend */
	case 442:
		switch (ndx) {
		case 0:
			p = "userland const struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* thr_wake */
	case 443:
		switch (ndx) {
		case 0:
			p = "long";
			break;
		default:
			break;
		};
		break;
	/* kldunloadf */
	case 444:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* audit */
	case 445:
		switch (ndx) {
		case 0:
			p = "userland const void *";
			break;
		case 1:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* auditon */
	case 446:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* getauid */
	case 447:
		switch (ndx) {
		case 0:
			p = "userland uid_t *";
			break;
		default:
			break;
		};
		break;
	/* setauid */
	case 448:
		switch (ndx) {
		case 0:
			p = "userland uid_t *";
			break;
		default:
			break;
		};
		break;
	/* getaudit */
	case 449:
		switch (ndx) {
		case 0:
			p = "userland struct auditinfo *";
			break;
		default:
			break;
		};
		break;
	/* setaudit */
	case 450:
		switch (ndx) {
		case 0:
			p = "userland struct auditinfo *";
			break;
		default:
			break;
		};
		break;
	/* getaudit_addr */
	case 451:
		switch (ndx) {
		case 0:
			p = "userland struct auditinfo_addr *";
			break;
		case 1:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* setaudit_addr */
	case 452:
		switch (ndx) {
		case 0:
			p = "userland struct auditinfo_addr *";
			break;
		case 1:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* auditctl */
	case 453:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* _umtx_op */
	case 454:
		switch (ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "u_long";
			break;
		case 3:
			p = "userland void *";
			break;
		case 4:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* thr_new */
	case 455:
		switch (ndx) {
		case 0:
			p = "userland struct thr_param *";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* sigqueue */
	case 456:
		switch (ndx) {
		case 0:
			p = "pid_t";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* kmq_open */
	case 457:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "mode_t";
			break;
		case 3:
			p = "userland const struct mq_attr *";
			break;
		default:
			break;
		};
		break;
	/* kmq_setattr */
	case 458:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct mq_attr *";
			break;
		case 2:
			p = "userland struct mq_attr *";
			break;
		default:
			break;
		};
		break;
	/* kmq_timedreceive */
	case 459:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "userland unsigned *";
			break;
		case 4:
			p = "userland const struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* kmq_timedsend */
	case 460:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "unsigned";
			break;
		case 4:
			p = "userland const struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* kmq_notify */
	case 461:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct sigevent *";
			break;
		default:
			break;
		};
		break;
	/* kmq_unlink */
	case 462:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* abort2 */
	case 463:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland void **";
			break;
		default:
			break;
		};
		break;
	/* thr_set_name */
	case 464:
		switch (ndx) {
		case 0:
			p = "long";
			break;
		case 1:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* aio_fsync */
	case 465:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct aiocb *";
			break;
		default:
			break;
		};
		break;
	/* rtprio_thread */
	case 466:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "lwpid_t";
			break;
		case 2:
			p = "userland struct rtprio *";
			break;
		default:
			break;
		};
		break;
	/* sctp_peeloff */
	case 471:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "uint32_t";
			break;
		default:
			break;
		};
		break;
	/* sctp_generic_sendmsg */
	case 472:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland const struct sockaddr *";
			break;
		case 4:
			p = "__socklen_t";
			break;
		case 5:
			p = "userland struct sctp_sndrcvinfo *";
			break;
		case 6:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* sctp_generic_sendmsg_iov */
	case 473:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct iovec *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland const struct sockaddr *";
			break;
		case 4:
			p = "__socklen_t";
			break;
		case 5:
			p = "userland struct sctp_sndrcvinfo *";
			break;
		case 6:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* sctp_generic_recvmsg */
	case 474:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct iovec *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland struct sockaddr *";
			break;
		case 4:
			p = "userland __socklen_t *";
			break;
		case 5:
			p = "userland struct sctp_sndrcvinfo *";
			break;
		case 6:
			p = "userland int *";
			break;
		default:
			break;
		};
		break;
	/* pread */
	case 475:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "off_t";
			break;
		default:
			break;
		};
		break;
	/* pwrite */
	case 476:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const void *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "off_t";
			break;
		default:
			break;
		};
		break;
	/* mmap */
	case 477:
		switch (ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "int";
			break;
		case 4:
			p = "int";
			break;
		case 5:
			p = "off_t";
			break;
		default:
			break;
		};
		break;
	/* lseek */
	case 478:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "off_t";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* truncate */
	case 479:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "off_t";
			break;
		default:
			break;
		};
		break;
	/* ftruncate */
	case 480:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "off_t";
			break;
		default:
			break;
		};
		break;
	/* thr_kill2 */
	case 481:
		switch (ndx) {
		case 0:
			p = "pid_t";
			break;
		case 1:
			p = "long";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* shm_unlink */
	case 483:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* cpuset */
	case 484:
		switch (ndx) {
		case 0:
			p = "userland cpusetid_t *";
			break;
		default:
			break;
		};
		break;
	/* cpuset_setid */
	case 485:
		switch (ndx) {
		case 0:
			p = "cpuwhich_t";
			break;
		case 1:
			p = "id_t";
			break;
		case 2:
			p = "cpusetid_t";
			break;
		default:
			break;
		};
		break;
	/* cpuset_getid */
	case 486:
		switch (ndx) {
		case 0:
			p = "cpulevel_t";
			break;
		case 1:
			p = "cpuwhich_t";
			break;
		case 2:
			p = "id_t";
			break;
		case 3:
			p = "userland cpusetid_t *";
			break;
		default:
			break;
		};
		break;
	/* cpuset_getaffinity */
	case 487:
		switch (ndx) {
		case 0:
			p = "cpulevel_t";
			break;
		case 1:
			p = "cpuwhich_t";
			break;
		case 2:
			p = "id_t";
			break;
		case 3:
			p = "size_t";
			break;
		case 4:
			p = "userland cpuset_t *";
			break;
		default:
			break;
		};
		break;
	/* cpuset_setaffinity */
	case 488:
		switch (ndx) {
		case 0:
			p = "cpulevel_t";
			break;
		case 1:
			p = "cpuwhich_t";
			break;
		case 2:
			p = "id_t";
			break;
		case 3:
			p = "size_t";
			break;
		case 4:
			p = "userland const cpuset_t *";
			break;
		default:
			break;
		};
		break;
	/* faccessat */
	case 489:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* fchmodat */
	case 490:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "mode_t";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* fchownat */
	case 491:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "uid_t";
			break;
		case 3:
			p = "gid_t";
			break;
		case 4:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* fexecve */
	case 492:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland char **";
			break;
		case 2:
			p = "userland char **";
			break;
		default:
			break;
		};
		break;
	/* futimesat */
	case 494:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland const struct timeval *";
			break;
		default:
			break;
		};
		break;
	/* linkat */
	case 495:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland const char *";
			break;
		case 4:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* mkdirat */
	case 496:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "mode_t";
			break;
		default:
			break;
		};
		break;
	/* mkfifoat */
	case 497:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "mode_t";
			break;
		default:
			break;
		};
		break;
	/* openat */
	case 499:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "mode_t";
			break;
		default:
			break;
		};
		break;
	/* readlinkat */
	case 500:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland char *";
			break;
		case 3:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* renameat */
	case 501:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* symlinkat */
	case 502:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* unlinkat */
	case 503:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* posix_openpt */
	case 504:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* jail_get */
	case 506:
		switch (ndx) {
		case 0:
			p = "userland struct iovec *";
			break;
		case 1:
			p = "unsigned int";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* jail_set */
	case 507:
		switch (ndx) {
		case 0:
			p = "userland struct iovec *";
			break;
		case 1:
			p = "unsigned int";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* jail_remove */
	case 508:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* __semctl */
	case 510:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland union semun *";
			break;
		default:
			break;
		};
		break;
	/* msgctl */
	case 511:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland struct msqid_ds *";
			break;
		default:
			break;
		};
		break;
	/* shmctl */
	case 512:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland struct shmid_ds *";
			break;
		default:
			break;
		};
		break;
	/* lpathconf */
	case 513:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* __cap_rights_get */
	case 515:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland cap_rights_t *";
			break;
		default:
			break;
		};
		break;
	/* cap_enter */
	case 516:
		break;
	/* cap_getmode */
	case 517:
		switch (ndx) {
		case 0:
			p = "userland u_int *";
			break;
		default:
			break;
		};
		break;
	/* pdfork */
	case 518:
		switch (ndx) {
		case 0:
			p = "userland int *";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* pdkill */
	case 519:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* pdgetpid */
	case 520:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland pid_t *";
			break;
		default:
			break;
		};
		break;
	/* pselect */
	case 522:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland fd_set *";
			break;
		case 2:
			p = "userland fd_set *";
			break;
		case 3:
			p = "userland fd_set *";
			break;
		case 4:
			p = "userland const struct timespec *";
			break;
		case 5:
			p = "userland const sigset_t *";
			break;
		default:
			break;
		};
		break;
	/* getloginclass */
	case 523:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* setloginclass */
	case 524:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* rctl_get_racct */
	case 525:
		switch (ndx) {
		case 0:
			p = "userland const void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* rctl_get_rules */
	case 526:
		switch (ndx) {
		case 0:
			p = "userland const void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* rctl_get_limits */
	case 527:
		switch (ndx) {
		case 0:
			p = "userland const void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* rctl_add_rule */
	case 528:
		switch (ndx) {
		case 0:
			p = "userland const void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* rctl_remove_rule */
	case 529:
		switch (ndx) {
		case 0:
			p = "userland const void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* posix_fallocate */
	case 530:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "off_t";
			break;
		case 2:
			p = "off_t";
			break;
		default:
			break;
		};
		break;
	/* posix_fadvise */
	case 531:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "off_t";
			break;
		case 2:
			p = "off_t";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* wait6 */
	case 532:
		switch (ndx) {
		case 0:
			p = "idtype_t";
			break;
		case 1:
			p = "id_t";
			break;
		case 2:
			p = "userland int *";
			break;
		case 3:
			p = "int";
			break;
		case 4:
			p = "userland struct __wrusage *";
			break;
		case 5:
			p = "userland struct __siginfo *";
			break;
		default:
			break;
		};
		break;
	/* cap_rights_limit */
	case 533:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland cap_rights_t *";
			break;
		default:
			break;
		};
		break;
	/* cap_ioctls_limit */
	case 534:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const u_long *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cap_ioctls_get */
	case 535:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland u_long *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cap_fcntls_limit */
	case 536:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "uint32_t";
			break;
		default:
			break;
		};
		break;
	/* cap_fcntls_get */
	case 537:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland uint32_t *";
			break;
		default:
			break;
		};
		break;
	/* bindat */
	case 538:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const struct sockaddr *";
			break;
		case 3:
			p = "__socklen_t";
			break;
		default:
			break;
		};
		break;
	/* connectat */
	case 539:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const struct sockaddr *";
			break;
		case 3:
			p = "__socklen_t";
			break;
		default:
			break;
		};
		break;
	/* chflagsat */
	case 540:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "u_long";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* accept4 */
	case 541:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct sockaddr *";
			break;
		case 2:
			p = "userland __socklen_t *";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* pipe2 */
	case 542:
		switch (ndx) {
		case 0:
			p = "userland int *";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* aio_mlock */
	case 543:
		switch (ndx) {
		case 0:
			p = "userland struct aiocb *";
			break;
		default:
			break;
		};
		break;
	/* procctl */
	case 544:
		switch (ndx) {
		case 0:
			p = "idtype_t";
			break;
		case 1:
			p = "id_t";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* ppoll */
	case 545:
		switch (ndx) {
		case 0:
			p = "userland struct pollfd *";
			break;
		case 1:
			p = "u_int";
			break;
		case 2:
			p = "userland const struct timespec *";
			break;
		case 3:
			p = "userland const sigset_t *";
			break;
		default:
			break;
		};
		break;
	/* futimens */
	case 546:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* utimensat */
	case 547:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland const struct timespec *";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* fdatasync */
	case 550:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* fstat */
	case 551:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct stat *";
			break;
		default:
			break;
		};
		break;
	/* fstatat */
	case 552:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland struct stat *";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* fhstat */
	case 553:
		switch (ndx) {
		case 0:
			p = "userland const struct fhandle *";
			break;
		case 1:
			p = "userland struct stat *";
			break;
		default:
			break;
		};
		break;
	/* getdirentries */
	case 554:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "userland off_t *";
			break;
		default:
			break;
		};
		break;
	/* statfs */
	case 555:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland struct statfs *";
			break;
		default:
			break;
		};
		break;
	/* fstatfs */
	case 556:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct statfs *";
			break;
		default:
			break;
		};
		break;
	/* getfsstat */
	case 557:
		switch (ndx) {
		case 0:
			p = "userland struct statfs *";
			break;
		case 1:
			p = "long";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* fhstatfs */
	case 558:
		switch (ndx) {
		case 0:
			p = "userland const struct fhandle *";
			break;
		case 1:
			p = "userland struct statfs *";
			break;
		default:
			break;
		};
		break;
	/* mknodat */
	case 559:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "mode_t";
			break;
		case 3:
			p = "dev_t";
			break;
		default:
			break;
		};
		break;
	/* kevent */
	case 560:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const struct kevent *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland struct kevent *";
			break;
		case 4:
			p = "int";
			break;
		case 5:
			p = "userland const struct timespec *";
			break;
		default:
			break;
		};
		break;
	/* cpuset_getdomain */
	case 561:
		switch (ndx) {
		case 0:
			p = "cpulevel_t";
			break;
		case 1:
			p = "cpuwhich_t";
			break;
		case 2:
			p = "id_t";
			break;
		case 3:
			p = "size_t";
			break;
		case 4:
			p = "userland domainset_t *";
			break;
		case 5:
			p = "userland int *";
			break;
		default:
			break;
		};
		break;
	/* cpuset_setdomain */
	case 562:
		switch (ndx) {
		case 0:
			p = "cpulevel_t";
			break;
		case 1:
			p = "cpuwhich_t";
			break;
		case 2:
			p = "id_t";
			break;
		case 3:
			p = "size_t";
			break;
		case 4:
			p = "userland domainset_t *";
			break;
		case 5:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* getrandom */
	case 563:
		switch (ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "unsigned int";
			break;
		default:
			break;
		};
		break;
	/* getfhat */
	case 564:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "userland struct fhandle *";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* fhlink */
	case 565:
		switch (ndx) {
		case 0:
			p = "userland struct fhandle *";
			break;
		case 1:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* fhlinkat */
	case 566:
		switch (ndx) {
		case 0:
			p = "userland struct fhandle *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* fhreadlink */
	case 567:
		switch (ndx) {
		case 0:
			p = "userland struct fhandle *";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* funlinkat */
	case 568:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* copy_file_range */
	case 569:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland off_t *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland off_t *";
			break;
		case 4:
			p = "size_t";
			break;
		case 5:
			p = "unsigned int";
			break;
		default:
			break;
		};
		break;
	/* __sysctlbyname */
	case 570:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "userland size_t *";
			break;
		case 4:
			p = "userland void *";
			break;
		case 5:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* shm_open2 */
	case 571:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "mode_t";
			break;
		case 3:
			p = "int";
			break;
		case 4:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* shm_rename */
	case 572:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* sigfastblock */
	case 573:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* __realpathat */
	case 574:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland char *";
			break;
		case 3:
			p = "size_t";
			break;
		case 4:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* close_range */
	case 575:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		case 1:
			p = "u_int";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* rpctls_syscall */
	case 576:
		switch (ndx) {
		case 0:
			p = "uint64_t";
			break;
		default:
			break;
		};
		break;
	/* __specialfd */
	case 577:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland const void *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* aio_writev */
	case 578:
		switch (ndx) {
		case 0:
			p = "userland struct aiocb *";
			break;
		default:
			break;
		};
		break;
	/* aio_readv */
	case 579:
		switch (ndx) {
		case 0:
			p = "userland struct aiocb *";
			break;
		default:
			break;
		};
		break;
	/* fspacectl */
	case 580:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const struct spacectl_range *";
			break;
		case 3:
			p = "int";
			break;
		case 4:
			p = "userland struct spacectl_range *";
			break;
		default:
			break;
		};
		break;
	/* sched_getcpu */
	case 581:
		break;
	/* swapoff */
	case 582:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* kqueuex */
	case 583:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* membarrier */
	case 584:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "unsigned";
			break;
		case 2:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* timerfd_create */
	case 585:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* timerfd_gettime */
	case 586:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland struct itimerspec *";
			break;
		default:
			break;
		};
		break;
	/* timerfd_settime */
	case 587:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const struct itimerspec *";
			break;
		case 3:
			p = "userland struct itimerspec *";
			break;
		default:
			break;
		};
		break;
	/* kcmp */
	case 588:
		switch (ndx) {
		case 0:
			p = "pid_t";
			break;
		case 1:
			p = "pid_t";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "uintptr_t";
			break;
		case 4:
			p = "uintptr_t";
			break;
		default:
			break;
		};
		break;
	/* getrlimitusage */
	case 589:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland rlim_t *";
			break;
		default:
			break;
		};
		break;
	/* fchroot */
	case 590:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* setcred */
	case 591:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		case 1:
			p = "userland const struct setcred *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* exterrctl */
	case 592:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		case 1:
			p = "u_int";
			break;
		case 2:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* inotify_add_watch_at */
	case 593:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		case 2:
			p = "userland const char *";
			break;
		case 3:
			p = "uint32_t";
			break;
		default:
			break;
		};
		break;
	/* inotify_rm_watch */
	case 594:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "int";
			break;
		default:
			break;
		};
		break;
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
static void
systrace_return_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
	/* syscall */
	case 0:
	/* exit */
	case 1:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* fork */
	case 2:
	/* read */
	case 3:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* write */
	case 4:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* open */
	case 5:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* close */
	case 6:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* wait4 */
	case 7:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* link */
	case 9:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* unlink */
	case 10:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* chdir */
	case 12:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchdir */
	case 13:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* chmod */
	case 15:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* chown */
	case 16:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* break */
	case 17:
		if (ndx == 0 || ndx == 1)
			p = "void *";
		break;
	/* getpid */
	case 20:
	/* mount */
	case 21:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* unmount */
	case 22:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setuid */
	case 23:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getuid */
	case 24:
	/* geteuid */
	case 25:
	/* ptrace */
	case 26:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* recvmsg */
	case 27:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* sendmsg */
	case 28:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* recvfrom */
	case 29:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* accept */
	case 30:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getpeername */
	case 31:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getsockname */
	case 32:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* access */
	case 33:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* chflags */
	case 34:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchflags */
	case 35:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sync */
	case 36:
	/* kill */
	case 37:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getppid */
	case 39:
	/* dup */
	case 41:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getegid */
	case 43:
	/* profil */
	case 44:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ktrace */
	case 45:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getgid */
	case 47:
	/* getlogin */
	case 49:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setlogin */
	case 50:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* acct */
	case 51:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sigaltstack */
	case 53:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ioctl */
	case 54:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* reboot */
	case 55:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* revoke */
	case 56:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* symlink */
	case 57:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* readlink */
	case 58:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* execve */
	case 59:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* umask */
	case 60:
		if (ndx == 0 || ndx == 1)
			p = "mode_t";
		break;
	/* chroot */
	case 61:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* msync */
	case 65:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* vfork */
	case 66:
	/* munmap */
	case 73:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mprotect */
	case 74:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* madvise */
	case 75:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mincore */
	case 78:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getgroups */
	case 79:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setgroups */
	case 80:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getpgrp */
	case 81:
	/* setpgid */
	case 82:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setitimer */
	case 83:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* swapon */
	case 85:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getitimer */
	case 86:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getdtablesize */
	case 89:
	/* dup2 */
	case 90:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fcntl */
	case 92:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* select */
	case 93:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fsync */
	case 95:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setpriority */
	case 96:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* socket */
	case 97:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* connect */
	case 98:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getpriority */
	case 100:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* bind */
	case 104:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setsockopt */
	case 105:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* listen */
	case 106:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* gettimeofday */
	case 116:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getrusage */
	case 117:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getsockopt */
	case 118:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* readv */
	case 120:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* writev */
	case 121:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* settimeofday */
	case 122:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchown */
	case 123:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchmod */
	case 124:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setreuid */
	case 126:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setregid */
	case 127:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* rename */
	case 128:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* flock */
	case 131:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mkfifo */
	case 132:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sendto */
	case 133:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* shutdown */
	case 134:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* socketpair */
	case 135:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mkdir */
	case 136:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* rmdir */
	case 137:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* utimes */
	case 138:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* adjtime */
	case 140:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setsid */
	case 147:
	/* quotactl */
	case 148:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* nlm_syscall */
	case 154:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* nfssvc */
	case 155:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* lgetfh */
	case 160:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getfh */
	case 161:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sysarch */
	case 165:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* rtprio */
	case 166:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* semsys */
	case 169:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* msgsys */
	case 170:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* shmsys */
	case 171:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setfib */
	case 175:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ntp_adjtime */
	case 176:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setgid */
	case 181:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setegid */
	case 182:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* seteuid */
	case 183:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* pathconf */
	case 191:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fpathconf */
	case 192:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getrlimit */
	case 194:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setrlimit */
	case 195:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __syscall */
	case 198:
	/* __sysctl */
	case 202:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mlock */
	case 203:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* munlock */
	case 204:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* undelete */
	case 205:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* futimes */
	case 206:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getpgid */
	case 207:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* poll */
	case 209:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* lkmnosys */
	case 210:
	/* lkmnosys */
	case 211:
	/* lkmnosys */
	case 212:
	/* lkmnosys */
	case 213:
	/* lkmnosys */
	case 214:
	/* lkmnosys */
	case 215:
	/* lkmnosys */
	case 216:
	/* lkmnosys */
	case 217:
	/* lkmnosys */
	case 218:
	/* lkmnosys */
	case 219:
	/* semget */
	case 221:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* semop */
	case 222:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* msgget */
	case 225:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* msgsnd */
	case 226:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* msgrcv */
	case 227:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* shmat */
	case 228:
		if (ndx == 0 || ndx == 1)
			p = "void *";
		break;
	/* shmdt */
	case 230:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* shmget */
	case 231:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* clock_gettime */
	case 232:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* clock_settime */
	case 233:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* clock_getres */
	case 234:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ktimer_create */
	case 235:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ktimer_delete */
	case 236:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ktimer_settime */
	case 237:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ktimer_gettime */
	case 238:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ktimer_getoverrun */
	case 239:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* nanosleep */
	case 240:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ffclock_getcounter */
	case 241:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ffclock_setestimate */
	case 242:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ffclock_getestimate */
	case 243:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* clock_nanosleep */
	case 244:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* clock_getcpuclockid2 */
	case 247:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ntp_gettime */
	case 248:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* minherit */
	case 250:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* rfork */
	case 251:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* issetugid */
	case 253:
	/* lchown */
	case 254:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* aio_read */
	case 255:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* aio_write */
	case 256:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* lio_listio */
	case 257:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* lchmod */
	case 274:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* lutimes */
	case 276:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* preadv */
	case 289:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* pwritev */
	case 290:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* fhopen */
	case 298:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* modnext */
	case 300:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* modstat */
	case 301:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* modfnext */
	case 302:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* modfind */
	case 303:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kldload */
	case 304:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kldunload */
	case 305:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kldfind */
	case 306:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kldnext */
	case 307:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kldstat */
	case 308:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kldfirstmod */
	case 309:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getsid */
	case 310:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setresuid */
	case 311:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setresgid */
	case 312:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* aio_return */
	case 314:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* aio_suspend */
	case 315:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* aio_cancel */
	case 316:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* aio_error */
	case 317:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* yield */
	case 321:
	/* mlockall */
	case 324:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* munlockall */
	case 325:
	/* __getcwd */
	case 326:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sched_setparam */
	case 327:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sched_getparam */
	case 328:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sched_setscheduler */
	case 329:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sched_getscheduler */
	case 330:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sched_yield */
	case 331:
	/* sched_get_priority_max */
	case 332:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sched_get_priority_min */
	case 333:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sched_rr_get_interval */
	case 334:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* utrace */
	case 335:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kldsym */
	case 337:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* jail */
	case 338:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* nnpfs_syscall */
	case 339:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sigprocmask */
	case 340:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sigsuspend */
	case 341:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sigpending */
	case 343:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sigtimedwait */
	case 345:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sigwaitinfo */
	case 346:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_get_file */
	case 347:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_set_file */
	case 348:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_get_fd */
	case 349:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_set_fd */
	case 350:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_delete_file */
	case 351:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_delete_fd */
	case 352:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_aclcheck_file */
	case 353:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_aclcheck_fd */
	case 354:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* extattrctl */
	case 355:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* extattr_set_file */
	case 356:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* extattr_get_file */
	case 357:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* extattr_delete_file */
	case 358:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* aio_waitcomplete */
	case 359:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* getresuid */
	case 360:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getresgid */
	case 361:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kqueue */
	case 362:
	/* extattr_set_fd */
	case 371:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* extattr_get_fd */
	case 372:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* extattr_delete_fd */
	case 373:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __setugid */
	case 374:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* eaccess */
	case 376:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* afs3_syscall */
	case 377:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* nmount */
	case 378:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __mac_get_proc */
	case 384:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __mac_set_proc */
	case 385:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __mac_get_fd */
	case 386:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __mac_get_file */
	case 387:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __mac_set_fd */
	case 388:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __mac_set_file */
	case 389:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kenv */
	case 390:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* lchflags */
	case 391:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* uuidgen */
	case 392:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sendfile */
	case 393:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mac_syscall */
	case 394:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ksem_close */
	case 400:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ksem_post */
	case 401:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ksem_wait */
	case 402:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ksem_trywait */
	case 403:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ksem_init */
	case 404:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ksem_open */
	case 405:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ksem_unlink */
	case 406:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ksem_getvalue */
	case 407:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ksem_destroy */
	case 408:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __mac_get_pid */
	case 409:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __mac_get_link */
	case 410:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __mac_set_link */
	case 411:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* extattr_set_link */
	case 412:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* extattr_get_link */
	case 413:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* extattr_delete_link */
	case 414:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __mac_execve */
	case 415:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sigaction */
	case 416:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sigreturn */
	case 417:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getcontext */
	case 421:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setcontext */
	case 422:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* swapcontext */
	case 423:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_get_link */
	case 425:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_set_link */
	case 426:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_delete_link */
	case 427:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __acl_aclcheck_link */
	case 428:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sigwait */
	case 429:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* thr_create */
	case 430:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* thr_exit */
	case 431:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* thr_self */
	case 432:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* thr_kill */
	case 433:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* jail_attach */
	case 436:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* extattr_list_fd */
	case 437:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* extattr_list_file */
	case 438:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* extattr_list_link */
	case 439:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* ksem_timedwait */
	case 441:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* thr_suspend */
	case 442:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* thr_wake */
	case 443:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kldunloadf */
	case 444:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* audit */
	case 445:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* auditon */
	case 446:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getauid */
	case 447:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setauid */
	case 448:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getaudit */
	case 449:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setaudit */
	case 450:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getaudit_addr */
	case 451:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setaudit_addr */
	case 452:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* auditctl */
	case 453:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* _umtx_op */
	case 454:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* thr_new */
	case 455:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sigqueue */
	case 456:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kmq_open */
	case 457:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kmq_setattr */
	case 458:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kmq_timedreceive */
	case 459:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kmq_timedsend */
	case 460:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kmq_notify */
	case 461:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kmq_unlink */
	case 462:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* abort2 */
	case 463:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* thr_set_name */
	case 464:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* aio_fsync */
	case 465:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* rtprio_thread */
	case 466:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sctp_peeloff */
	case 471:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sctp_generic_sendmsg */
	case 472:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sctp_generic_sendmsg_iov */
	case 473:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sctp_generic_recvmsg */
	case 474:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* pread */
	case 475:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* pwrite */
	case 476:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* mmap */
	case 477:
		if (ndx == 0 || ndx == 1)
			p = "void *";
		break;
	/* lseek */
	case 478:
		if (ndx == 0 || ndx == 1)
			p = "off_t";
		break;
	/* truncate */
	case 479:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ftruncate */
	case 480:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* thr_kill2 */
	case 481:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* shm_unlink */
	case 483:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cpuset */
	case 484:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cpuset_setid */
	case 485:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cpuset_getid */
	case 486:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cpuset_getaffinity */
	case 487:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cpuset_setaffinity */
	case 488:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* faccessat */
	case 489:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchmodat */
	case 490:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchownat */
	case 491:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fexecve */
	case 492:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* futimesat */
	case 494:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linkat */
	case 495:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mkdirat */
	case 496:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mkfifoat */
	case 497:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* openat */
	case 499:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* readlinkat */
	case 500:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* renameat */
	case 501:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* symlinkat */
	case 502:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* unlinkat */
	case 503:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* posix_openpt */
	case 504:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* jail_get */
	case 506:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* jail_set */
	case 507:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* jail_remove */
	case 508:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __semctl */
	case 510:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* msgctl */
	case 511:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* shmctl */
	case 512:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* lpathconf */
	case 513:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __cap_rights_get */
	case 515:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cap_enter */
	case 516:
	/* cap_getmode */
	case 517:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* pdfork */
	case 518:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* pdkill */
	case 519:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* pdgetpid */
	case 520:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* pselect */
	case 522:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getloginclass */
	case 523:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setloginclass */
	case 524:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* rctl_get_racct */
	case 525:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* rctl_get_rules */
	case 526:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* rctl_get_limits */
	case 527:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* rctl_add_rule */
	case 528:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* rctl_remove_rule */
	case 529:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* posix_fallocate */
	case 530:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* posix_fadvise */
	case 531:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* wait6 */
	case 532:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cap_rights_limit */
	case 533:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cap_ioctls_limit */
	case 534:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cap_ioctls_get */
	case 535:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* cap_fcntls_limit */
	case 536:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cap_fcntls_get */
	case 537:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* bindat */
	case 538:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* connectat */
	case 539:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* chflagsat */
	case 540:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* accept4 */
	case 541:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* pipe2 */
	case 542:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* aio_mlock */
	case 543:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* procctl */
	case 544:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* ppoll */
	case 545:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* futimens */
	case 546:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* utimensat */
	case 547:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fdatasync */
	case 550:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fstat */
	case 551:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fstatat */
	case 552:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fhstat */
	case 553:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getdirentries */
	case 554:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* statfs */
	case 555:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fstatfs */
	case 556:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getfsstat */
	case 557:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fhstatfs */
	case 558:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mknodat */
	case 559:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kevent */
	case 560:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cpuset_getdomain */
	case 561:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* cpuset_setdomain */
	case 562:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getrandom */
	case 563:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getfhat */
	case 564:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fhlink */
	case 565:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fhlinkat */
	case 566:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fhreadlink */
	case 567:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* funlinkat */
	case 568:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* copy_file_range */
	case 569:
		if (ndx == 0 || ndx == 1)
			p = "ssize_t";
		break;
	/* __sysctlbyname */
	case 570:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* shm_open2 */
	case 571:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* shm_rename */
	case 572:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sigfastblock */
	case 573:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __realpathat */
	case 574:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* close_range */
	case 575:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* rpctls_syscall */
	case 576:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* __specialfd */
	case 577:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* aio_writev */
	case 578:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* aio_readv */
	case 579:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fspacectl */
	case 580:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sched_getcpu */
	case 581:
	/* swapoff */
	case 582:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kqueuex */
	case 583:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* membarrier */
	case 584:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* timerfd_create */
	case 585:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* timerfd_gettime */
	case 586:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* timerfd_settime */
	case 587:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* kcmp */
	case 588:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getrlimitusage */
	case 589:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchroot */
	case 590:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setcred */
	case 591:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* exterrctl */
	case 592:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* inotify_add_watch_at */
	case 593:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* inotify_rm_watch */
	case 594:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
