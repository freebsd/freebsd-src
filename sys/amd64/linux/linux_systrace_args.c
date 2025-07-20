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
	/* read */
	case 0: {
		struct read_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->nbyte; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* linux_write */
	case 1: {
		struct linux_write_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->nbyte; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* linux_open */
	case 2: {
		struct linux_open_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		iarg[a++] = p->flags; /* l_int */
		iarg[a++] = p->mode; /* l_mode_t */
		*n_args = 3;
		break;
	}
	/* close */
	case 3: {
		struct close_args *p = params;
		iarg[a++] = p->fd; /* int */
		*n_args = 1;
		break;
	}
	/* linux_newstat */
	case 4: {
		struct linux_newstat_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		uarg[a++] = (intptr_t)p->buf; /* struct l_newstat * */
		*n_args = 2;
		break;
	}
	/* linux_newfstat */
	case 5: {
		struct linux_newfstat_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		uarg[a++] = (intptr_t)p->buf; /* struct l_newstat * */
		*n_args = 2;
		break;
	}
	/* linux_newlstat */
	case 6: {
		struct linux_newlstat_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		uarg[a++] = (intptr_t)p->buf; /* struct l_newstat * */
		*n_args = 2;
		break;
	}
	/* linux_poll */
	case 7: {
		struct linux_poll_args *p = params;
		uarg[a++] = (intptr_t)p->fds; /* struct pollfd * */
		uarg[a++] = p->nfds; /* u_int */
		iarg[a++] = p->timeout; /* int */
		*n_args = 3;
		break;
	}
	/* linux_lseek */
	case 8: {
		struct linux_lseek_args *p = params;
		iarg[a++] = p->fdes; /* l_uint */
		iarg[a++] = p->off; /* l_off_t */
		iarg[a++] = p->whence; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_mmap2 */
	case 9: {
		struct linux_mmap2_args *p = params;
		iarg[a++] = p->addr; /* l_ulong */
		iarg[a++] = p->len; /* l_ulong */
		iarg[a++] = p->prot; /* l_ulong */
		iarg[a++] = p->flags; /* l_ulong */
		iarg[a++] = p->fd; /* l_ulong */
		iarg[a++] = p->pgoff; /* l_ulong */
		*n_args = 6;
		break;
	}
	/* linux_mprotect */
	case 10: {
		struct linux_mprotect_args *p = params;
		iarg[a++] = p->addr; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->prot; /* l_ulong */
		*n_args = 3;
		break;
	}
	/* munmap */
	case 11: {
		struct munmap_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* void * */
		iarg[a++] = p->len; /* l_size_t */
		*n_args = 2;
		break;
	}
	/* linux_brk */
	case 12: {
		struct linux_brk_args *p = params;
		iarg[a++] = p->dsend; /* l_ulong */
		*n_args = 1;
		break;
	}
	/* linux_rt_sigaction */
	case 13: {
		struct linux_rt_sigaction_args *p = params;
		iarg[a++] = p->sig; /* l_int */
		uarg[a++] = (intptr_t)p->act; /* l_sigaction_t * */
		uarg[a++] = (intptr_t)p->oact; /* l_sigaction_t * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_rt_sigprocmask */
	case 14: {
		struct linux_rt_sigprocmask_args *p = params;
		iarg[a++] = p->how; /* l_int */
		uarg[a++] = (intptr_t)p->mask; /* l_sigset_t * */
		uarg[a++] = (intptr_t)p->omask; /* l_sigset_t * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_rt_sigreturn */
	case 15: {
		struct linux_rt_sigreturn_args *p = params;
		uarg[a++] = (intptr_t)p->ucp; /* struct l_ucontext * */
		*n_args = 1;
		break;
	}
	/* linux_ioctl */
	case 16: {
		struct linux_ioctl_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		iarg[a++] = p->cmd; /* l_uint */
		iarg[a++] = p->arg; /* l_ulong */
		*n_args = 3;
		break;
	}
	/* linux_pread */
	case 17: {
		struct linux_pread_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->nbyte; /* l_size_t */
		iarg[a++] = p->offset; /* l_loff_t */
		*n_args = 4;
		break;
	}
	/* linux_pwrite */
	case 18: {
		struct linux_pwrite_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->nbyte; /* l_size_t */
		iarg[a++] = p->offset; /* l_loff_t */
		*n_args = 4;
		break;
	}
	/* readv */
	case 19: {
		struct readv_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->iovp; /* struct iovec * */
		uarg[a++] = p->iovcnt; /* u_int */
		*n_args = 3;
		break;
	}
	/* linux_writev */
	case 20: {
		struct linux_writev_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->iovp; /* struct iovec * */
		uarg[a++] = p->iovcnt; /* u_int */
		*n_args = 3;
		break;
	}
	/* linux_access */
	case 21: {
		struct linux_access_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		iarg[a++] = p->amode; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_pipe */
	case 22: {
		struct linux_pipe_args *p = params;
		uarg[a++] = (intptr_t)p->pipefds; /* l_int * */
		*n_args = 1;
		break;
	}
	/* linux_select */
	case 23: {
		struct linux_select_args *p = params;
		iarg[a++] = p->nfds; /* l_int */
		uarg[a++] = (intptr_t)p->readfds; /* l_fd_set * */
		uarg[a++] = (intptr_t)p->writefds; /* l_fd_set * */
		uarg[a++] = (intptr_t)p->exceptfds; /* l_fd_set * */
		uarg[a++] = (intptr_t)p->timeout; /* struct l_timeval * */
		*n_args = 5;
		break;
	}
	/* sched_yield */
	case 24: {
		*n_args = 0;
		break;
	}
	/* linux_mremap */
	case 25: {
		struct linux_mremap_args *p = params;
		iarg[a++] = p->addr; /* l_ulong */
		iarg[a++] = p->old_len; /* l_ulong */
		iarg[a++] = p->new_len; /* l_ulong */
		iarg[a++] = p->flags; /* l_ulong */
		iarg[a++] = p->new_addr; /* l_ulong */
		*n_args = 5;
		break;
	}
	/* linux_msync */
	case 26: {
		struct linux_msync_args *p = params;
		iarg[a++] = p->addr; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->fl; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_mincore */
	case 27: {
		struct linux_mincore_args *p = params;
		iarg[a++] = p->start; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		uarg[a++] = (intptr_t)p->vec; /* u_char * */
		*n_args = 3;
		break;
	}
	/* linux_madvise */
	case 28: {
		struct linux_madvise_args *p = params;
		iarg[a++] = p->addr; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->behav; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_shmget */
	case 29: {
		struct linux_shmget_args *p = params;
		iarg[a++] = p->key; /* l_key_t */
		iarg[a++] = p->size; /* l_size_t */
		iarg[a++] = p->shmflg; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_shmat */
	case 30: {
		struct linux_shmat_args *p = params;
		iarg[a++] = p->shmid; /* l_int */
		uarg[a++] = (intptr_t)p->shmaddr; /* char * */
		iarg[a++] = p->shmflg; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_shmctl */
	case 31: {
		struct linux_shmctl_args *p = params;
		iarg[a++] = p->shmid; /* l_int */
		iarg[a++] = p->cmd; /* l_int */
		uarg[a++] = (intptr_t)p->buf; /* struct l_shmid_ds * */
		*n_args = 3;
		break;
	}
	/* dup */
	case 32: {
		struct dup_args *p = params;
		uarg[a++] = p->fd; /* u_int */
		*n_args = 1;
		break;
	}
	/* dup2 */
	case 33: {
		struct dup2_args *p = params;
		uarg[a++] = p->from; /* u_int */
		uarg[a++] = p->to; /* u_int */
		*n_args = 2;
		break;
	}
	/* linux_pause */
	case 34: {
		*n_args = 0;
		break;
	}
	/* linux_nanosleep */
	case 35: {
		struct linux_nanosleep_args *p = params;
		uarg[a++] = (intptr_t)p->rqtp; /* const struct l_timespec * */
		uarg[a++] = (intptr_t)p->rmtp; /* struct l_timespec * */
		*n_args = 2;
		break;
	}
	/* linux_getitimer */
	case 36: {
		struct linux_getitimer_args *p = params;
		iarg[a++] = p->which; /* l_int */
		uarg[a++] = (intptr_t)p->itv; /* struct l_itimerval * */
		*n_args = 2;
		break;
	}
	/* linux_alarm */
	case 37: {
		struct linux_alarm_args *p = params;
		iarg[a++] = p->secs; /* l_uint */
		*n_args = 1;
		break;
	}
	/* linux_setitimer */
	case 38: {
		struct linux_setitimer_args *p = params;
		iarg[a++] = p->which; /* l_int */
		uarg[a++] = (intptr_t)p->itv; /* struct l_itimerval * */
		uarg[a++] = (intptr_t)p->oitv; /* struct l_itimerval * */
		*n_args = 3;
		break;
	}
	/* linux_getpid */
	case 39: {
		*n_args = 0;
		break;
	}
	/* linux_sendfile */
	case 40: {
		struct linux_sendfile_args *p = params;
		iarg[a++] = p->out; /* l_int */
		iarg[a++] = p->in; /* l_int */
		uarg[a++] = (intptr_t)p->offset; /* l_off_t * */
		iarg[a++] = p->count; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_socket */
	case 41: {
		struct linux_socket_args *p = params;
		iarg[a++] = p->domain; /* l_int */
		iarg[a++] = p->type; /* l_int */
		iarg[a++] = p->protocol; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_connect */
	case 42: {
		struct linux_connect_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->name; /* l_uintptr_t */
		iarg[a++] = p->namelen; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_accept */
	case 43: {
		struct linux_accept_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->addr; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->namelen; /* l_uintptr_t */
		*n_args = 3;
		break;
	}
	/* linux_sendto */
	case 44: {
		struct linux_sendto_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->msg; /* l_uintptr_t */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->flags; /* l_uint */
		uarg[a++] = (intptr_t)p->to; /* l_uintptr_t */
		iarg[a++] = p->tolen; /* l_int */
		*n_args = 6;
		break;
	}
	/* linux_recvfrom */
	case 45: {
		struct linux_recvfrom_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->buf; /* l_uintptr_t */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->flags; /* l_uint */
		uarg[a++] = (intptr_t)p->from; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->fromlen; /* l_uintptr_t */
		*n_args = 6;
		break;
	}
	/* linux_sendmsg */
	case 46: {
		struct linux_sendmsg_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->msg; /* l_uintptr_t */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_recvmsg */
	case 47: {
		struct linux_recvmsg_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->msg; /* l_uintptr_t */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_shutdown */
	case 48: {
		struct linux_shutdown_args *p = params;
		iarg[a++] = p->s; /* l_int */
		iarg[a++] = p->how; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_bind */
	case 49: {
		struct linux_bind_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->name; /* l_uintptr_t */
		iarg[a++] = p->namelen; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_listen */
	case 50: {
		struct linux_listen_args *p = params;
		iarg[a++] = p->s; /* l_int */
		iarg[a++] = p->backlog; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_getsockname */
	case 51: {
		struct linux_getsockname_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->addr; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->namelen; /* l_uintptr_t */
		*n_args = 3;
		break;
	}
	/* linux_getpeername */
	case 52: {
		struct linux_getpeername_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->addr; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->namelen; /* l_uintptr_t */
		*n_args = 3;
		break;
	}
	/* linux_socketpair */
	case 53: {
		struct linux_socketpair_args *p = params;
		iarg[a++] = p->domain; /* l_int */
		iarg[a++] = p->type; /* l_int */
		iarg[a++] = p->protocol; /* l_int */
		uarg[a++] = (intptr_t)p->rsv; /* l_uintptr_t */
		*n_args = 4;
		break;
	}
	/* linux_setsockopt */
	case 54: {
		struct linux_setsockopt_args *p = params;
		iarg[a++] = p->s; /* l_int */
		iarg[a++] = p->level; /* l_int */
		iarg[a++] = p->optname; /* l_int */
		uarg[a++] = (intptr_t)p->optval; /* l_uintptr_t */
		iarg[a++] = p->optlen; /* l_int */
		*n_args = 5;
		break;
	}
	/* linux_getsockopt */
	case 55: {
		struct linux_getsockopt_args *p = params;
		iarg[a++] = p->s; /* l_int */
		iarg[a++] = p->level; /* l_int */
		iarg[a++] = p->optname; /* l_int */
		uarg[a++] = (intptr_t)p->optval; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->optlen; /* l_uintptr_t */
		*n_args = 5;
		break;
	}
	/* linux_clone */
	case 56: {
		struct linux_clone_args *p = params;
		iarg[a++] = p->flags; /* l_ulong */
		iarg[a++] = p->stack; /* l_ulong */
		uarg[a++] = (intptr_t)p->parent_tidptr; /* l_int * */
		uarg[a++] = (intptr_t)p->child_tidptr; /* l_int * */
		iarg[a++] = p->tls; /* l_ulong */
		*n_args = 5;
		break;
	}
	/* linux_fork */
	case 57: {
		*n_args = 0;
		break;
	}
	/* linux_vfork */
	case 58: {
		*n_args = 0;
		break;
	}
	/* linux_execve */
	case 59: {
		struct linux_execve_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		uarg[a++] = (intptr_t)p->argp; /* l_uintptr_t * */
		uarg[a++] = (intptr_t)p->envp; /* l_uintptr_t * */
		*n_args = 3;
		break;
	}
	/* linux_exit */
	case 60: {
		struct linux_exit_args *p = params;
		iarg[a++] = p->rval; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_wait4 */
	case 61: {
		struct linux_wait4_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->status; /* l_int * */
		iarg[a++] = p->options; /* l_int */
		uarg[a++] = (intptr_t)p->rusage; /* struct rusage * */
		*n_args = 4;
		break;
	}
	/* linux_kill */
	case 62: {
		struct linux_kill_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->signum; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_newuname */
	case 63: {
		struct linux_newuname_args *p = params;
		uarg[a++] = (intptr_t)p->buf; /* struct l_new_utsname * */
		*n_args = 1;
		break;
	}
	/* linux_semget */
	case 64: {
		struct linux_semget_args *p = params;
		iarg[a++] = p->key; /* l_key_t */
		iarg[a++] = p->nsems; /* l_int */
		iarg[a++] = p->semflg; /* l_int */
		*n_args = 3;
		break;
	}
	/* semop */
	case 65: {
		struct semop_args *p = params;
		iarg[a++] = p->semid; /* l_int */
		uarg[a++] = (intptr_t)p->sops; /* struct sembuf * */
		iarg[a++] = p->nsops; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* linux_semctl */
	case 66: {
		struct linux_semctl_args *p = params;
		iarg[a++] = p->semid; /* l_int */
		iarg[a++] = p->semnum; /* l_int */
		iarg[a++] = p->cmd; /* l_int */
		uarg[a++] = p->arg.buf; /* union l_semun */
		*n_args = 4;
		break;
	}
	/* linux_shmdt */
	case 67: {
		struct linux_shmdt_args *p = params;
		uarg[a++] = (intptr_t)p->shmaddr; /* char * */
		*n_args = 1;
		break;
	}
	/* linux_msgget */
	case 68: {
		struct linux_msgget_args *p = params;
		iarg[a++] = p->key; /* l_key_t */
		iarg[a++] = p->msgflg; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_msgsnd */
	case 69: {
		struct linux_msgsnd_args *p = params;
		iarg[a++] = p->msqid; /* l_int */
		uarg[a++] = (intptr_t)p->msgp; /* struct l_msgbuf * */
		iarg[a++] = p->msgsz; /* l_size_t */
		iarg[a++] = p->msgflg; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_msgrcv */
	case 70: {
		struct linux_msgrcv_args *p = params;
		iarg[a++] = p->msqid; /* l_int */
		uarg[a++] = (intptr_t)p->msgp; /* struct l_msgbuf * */
		iarg[a++] = p->msgsz; /* l_size_t */
		iarg[a++] = p->msgtyp; /* l_long */
		iarg[a++] = p->msgflg; /* l_int */
		*n_args = 5;
		break;
	}
	/* linux_msgctl */
	case 71: {
		struct linux_msgctl_args *p = params;
		iarg[a++] = p->msqid; /* l_int */
		iarg[a++] = p->cmd; /* l_int */
		uarg[a++] = (intptr_t)p->buf; /* struct l_msqid_ds * */
		*n_args = 3;
		break;
	}
	/* linux_fcntl */
	case 72: {
		struct linux_fcntl_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		iarg[a++] = p->cmd; /* l_uint */
		iarg[a++] = p->arg; /* l_ulong */
		*n_args = 3;
		break;
	}
	/* flock */
	case 73: {
		struct flock_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->how; /* int */
		*n_args = 2;
		break;
	}
	/* fsync */
	case 74: {
		struct fsync_args *p = params;
		iarg[a++] = p->fd; /* int */
		*n_args = 1;
		break;
	}
	/* linux_fdatasync */
	case 75: {
		struct linux_fdatasync_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		*n_args = 1;
		break;
	}
	/* linux_truncate */
	case 76: {
		struct linux_truncate_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		iarg[a++] = p->length; /* l_long */
		*n_args = 2;
		break;
	}
	/* linux_ftruncate */
	case 77: {
		struct linux_ftruncate_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->length; /* l_ulong */
		*n_args = 2;
		break;
	}
	/* linux_getdents */
	case 78: {
		struct linux_getdents_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		uarg[a++] = (intptr_t)p->dent; /* void * */
		iarg[a++] = p->count; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_getcwd */
	case 79: {
		struct linux_getcwd_args *p = params;
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->bufsize; /* l_ulong */
		*n_args = 2;
		break;
	}
	/* linux_chdir */
	case 80: {
		struct linux_chdir_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		*n_args = 1;
		break;
	}
	/* fchdir */
	case 81: {
		struct fchdir_args *p = params;
		iarg[a++] = p->fd; /* int */
		*n_args = 1;
		break;
	}
	/* linux_rename */
	case 82: {
		struct linux_rename_args *p = params;
		uarg[a++] = (intptr_t)p->from; /* char * */
		uarg[a++] = (intptr_t)p->to; /* char * */
		*n_args = 2;
		break;
	}
	/* linux_mkdir */
	case 83: {
		struct linux_mkdir_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		iarg[a++] = p->mode; /* l_mode_t */
		*n_args = 2;
		break;
	}
	/* linux_rmdir */
	case 84: {
		struct linux_rmdir_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		*n_args = 1;
		break;
	}
	/* linux_creat */
	case 85: {
		struct linux_creat_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		iarg[a++] = p->mode; /* l_mode_t */
		*n_args = 2;
		break;
	}
	/* linux_link */
	case 86: {
		struct linux_link_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		uarg[a++] = (intptr_t)p->to; /* char * */
		*n_args = 2;
		break;
	}
	/* linux_unlink */
	case 87: {
		struct linux_unlink_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		*n_args = 1;
		break;
	}
	/* linux_symlink */
	case 88: {
		struct linux_symlink_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		uarg[a++] = (intptr_t)p->to; /* char * */
		*n_args = 2;
		break;
	}
	/* linux_readlink */
	case 89: {
		struct linux_readlink_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* char * */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->count; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_chmod */
	case 90: {
		struct linux_chmod_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		iarg[a++] = p->mode; /* l_mode_t */
		*n_args = 2;
		break;
	}
	/* fchmod */
	case 91: {
		struct fchmod_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->mode; /* int */
		*n_args = 2;
		break;
	}
	/* linux_chown */
	case 92: {
		struct linux_chown_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		iarg[a++] = p->uid; /* l_uid_t */
		iarg[a++] = p->gid; /* l_gid_t */
		*n_args = 3;
		break;
	}
	/* fchown */
	case 93: {
		struct fchown_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->uid; /* int */
		iarg[a++] = p->gid; /* int */
		*n_args = 3;
		break;
	}
	/* linux_lchown */
	case 94: {
		struct linux_lchown_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		iarg[a++] = p->uid; /* l_uid_t */
		iarg[a++] = p->gid; /* l_gid_t */
		*n_args = 3;
		break;
	}
	/* umask */
	case 95: {
		struct umask_args *p = params;
		iarg[a++] = p->newmask; /* int */
		*n_args = 1;
		break;
	}
	/* gettimeofday */
	case 96: {
		struct gettimeofday_args *p = params;
		uarg[a++] = (intptr_t)p->tp; /* struct l_timeval * */
		uarg[a++] = (intptr_t)p->tzp; /* struct timezone * */
		*n_args = 2;
		break;
	}
	/* linux_getrlimit */
	case 97: {
		struct linux_getrlimit_args *p = params;
		iarg[a++] = p->resource; /* l_uint */
		uarg[a++] = (intptr_t)p->rlim; /* struct l_rlimit * */
		*n_args = 2;
		break;
	}
	/* getrusage */
	case 98: {
		struct getrusage_args *p = params;
		iarg[a++] = p->who; /* int */
		uarg[a++] = (intptr_t)p->rusage; /* struct rusage * */
		*n_args = 2;
		break;
	}
	/* linux_sysinfo */
	case 99: {
		struct linux_sysinfo_args *p = params;
		uarg[a++] = (intptr_t)p->info; /* struct l_sysinfo * */
		*n_args = 1;
		break;
	}
	/* linux_times */
	case 100: {
		struct linux_times_args *p = params;
		uarg[a++] = (intptr_t)p->buf; /* struct l_times_argv * */
		*n_args = 1;
		break;
	}
	/* linux_ptrace */
	case 101: {
		struct linux_ptrace_args *p = params;
		iarg[a++] = p->req; /* l_long */
		iarg[a++] = p->pid; /* l_long */
		iarg[a++] = p->addr; /* l_ulong */
		iarg[a++] = p->data; /* l_ulong */
		*n_args = 4;
		break;
	}
	/* linux_getuid */
	case 102: {
		*n_args = 0;
		break;
	}
	/* linux_syslog */
	case 103: {
		struct linux_syslog_args *p = params;
		iarg[a++] = p->type; /* l_int */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->len; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_getgid */
	case 104: {
		*n_args = 0;
		break;
	}
	/* setuid */
	case 105: {
		struct setuid_args *p = params;
		uarg[a++] = p->uid; /* uid_t */
		*n_args = 1;
		break;
	}
	/* setgid */
	case 106: {
		struct setgid_args *p = params;
		iarg[a++] = p->gid; /* gid_t */
		*n_args = 1;
		break;
	}
	/* geteuid */
	case 107: {
		*n_args = 0;
		break;
	}
	/* getegid */
	case 108: {
		*n_args = 0;
		break;
	}
	/* setpgid */
	case 109: {
		struct setpgid_args *p = params;
		iarg[a++] = p->pid; /* int */
		iarg[a++] = p->pgid; /* int */
		*n_args = 2;
		break;
	}
	/* linux_getppid */
	case 110: {
		*n_args = 0;
		break;
	}
	/* getpgrp */
	case 111: {
		*n_args = 0;
		break;
	}
	/* setsid */
	case 112: {
		*n_args = 0;
		break;
	}
	/* setreuid */
	case 113: {
		struct setreuid_args *p = params;
		uarg[a++] = p->ruid; /* uid_t */
		uarg[a++] = p->euid; /* uid_t */
		*n_args = 2;
		break;
	}
	/* setregid */
	case 114: {
		struct setregid_args *p = params;
		iarg[a++] = p->rgid; /* gid_t */
		iarg[a++] = p->egid; /* gid_t */
		*n_args = 2;
		break;
	}
	/* linux_getgroups */
	case 115: {
		struct linux_getgroups_args *p = params;
		iarg[a++] = p->gidsetsize; /* l_int */
		uarg[a++] = (intptr_t)p->grouplist; /* l_gid_t * */
		*n_args = 2;
		break;
	}
	/* linux_setgroups */
	case 116: {
		struct linux_setgroups_args *p = params;
		iarg[a++] = p->gidsetsize; /* l_int */
		uarg[a++] = (intptr_t)p->grouplist; /* l_gid_t * */
		*n_args = 2;
		break;
	}
	/* setresuid */
	case 117: {
		struct setresuid_args *p = params;
		uarg[a++] = p->ruid; /* uid_t */
		uarg[a++] = p->euid; /* uid_t */
		uarg[a++] = p->suid; /* uid_t */
		*n_args = 3;
		break;
	}
	/* getresuid */
	case 118: {
		struct getresuid_args *p = params;
		uarg[a++] = (intptr_t)p->ruid; /* uid_t * */
		uarg[a++] = (intptr_t)p->euid; /* uid_t * */
		uarg[a++] = (intptr_t)p->suid; /* uid_t * */
		*n_args = 3;
		break;
	}
	/* setresgid */
	case 119: {
		struct setresgid_args *p = params;
		iarg[a++] = p->rgid; /* gid_t */
		iarg[a++] = p->egid; /* gid_t */
		iarg[a++] = p->sgid; /* gid_t */
		*n_args = 3;
		break;
	}
	/* getresgid */
	case 120: {
		struct getresgid_args *p = params;
		uarg[a++] = (intptr_t)p->rgid; /* gid_t * */
		uarg[a++] = (intptr_t)p->egid; /* gid_t * */
		uarg[a++] = (intptr_t)p->sgid; /* gid_t * */
		*n_args = 3;
		break;
	}
	/* getpgid */
	case 121: {
		struct getpgid_args *p = params;
		iarg[a++] = p->pid; /* int */
		*n_args = 1;
		break;
	}
	/* linux_setfsuid */
	case 122: {
		struct linux_setfsuid_args *p = params;
		iarg[a++] = p->uid; /* l_uid_t */
		*n_args = 1;
		break;
	}
	/* linux_setfsgid */
	case 123: {
		struct linux_setfsgid_args *p = params;
		iarg[a++] = p->gid; /* l_gid_t */
		*n_args = 1;
		break;
	}
	/* linux_getsid */
	case 124: {
		struct linux_getsid_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		*n_args = 1;
		break;
	}
	/* linux_capget */
	case 125: {
		struct linux_capget_args *p = params;
		uarg[a++] = (intptr_t)p->hdrp; /* struct l_user_cap_header * */
		uarg[a++] = (intptr_t)p->datap; /* struct l_user_cap_data * */
		*n_args = 2;
		break;
	}
	/* linux_capset */
	case 126: {
		struct linux_capset_args *p = params;
		uarg[a++] = (intptr_t)p->hdrp; /* struct l_user_cap_header * */
		uarg[a++] = (intptr_t)p->datap; /* struct l_user_cap_data * */
		*n_args = 2;
		break;
	}
	/* linux_rt_sigpending */
	case 127: {
		struct linux_rt_sigpending_args *p = params;
		uarg[a++] = (intptr_t)p->set; /* l_sigset_t * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 2;
		break;
	}
	/* linux_rt_sigtimedwait */
	case 128: {
		struct linux_rt_sigtimedwait_args *p = params;
		uarg[a++] = (intptr_t)p->mask; /* l_sigset_t * */
		uarg[a++] = (intptr_t)p->ptr; /* l_siginfo_t * */
		uarg[a++] = (intptr_t)p->timeout; /* struct l_timespec * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_rt_sigqueueinfo */
	case 129: {
		struct linux_rt_sigqueueinfo_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->sig; /* l_int */
		uarg[a++] = (intptr_t)p->info; /* l_siginfo_t * */
		*n_args = 3;
		break;
	}
	/* linux_rt_sigsuspend */
	case 130: {
		struct linux_rt_sigsuspend_args *p = params;
		uarg[a++] = (intptr_t)p->newset; /* l_sigset_t * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 2;
		break;
	}
	/* linux_sigaltstack */
	case 131: {
		struct linux_sigaltstack_args *p = params;
		uarg[a++] = (intptr_t)p->uss; /* l_stack_t * */
		uarg[a++] = (intptr_t)p->uoss; /* l_stack_t * */
		*n_args = 2;
		break;
	}
	/* linux_utime */
	case 132: {
		struct linux_utime_args *p = params;
		uarg[a++] = (intptr_t)p->fname; /* char * */
		uarg[a++] = (intptr_t)p->times; /* struct l_utimbuf * */
		*n_args = 2;
		break;
	}
	/* linux_mknod */
	case 133: {
		struct linux_mknod_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		iarg[a++] = p->mode; /* l_mode_t */
		iarg[a++] = p->dev; /* l_dev_t */
		*n_args = 3;
		break;
	}
	/* linux_personality */
	case 135: {
		struct linux_personality_args *p = params;
		iarg[a++] = p->per; /* l_uint */
		*n_args = 1;
		break;
	}
	/* linux_ustat */
	case 136: {
		struct linux_ustat_args *p = params;
		iarg[a++] = p->dev; /* l_uint */
		uarg[a++] = (intptr_t)p->ubuf; /* struct l_ustat * */
		*n_args = 2;
		break;
	}
	/* linux_statfs */
	case 137: {
		struct linux_statfs_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		uarg[a++] = (intptr_t)p->buf; /* struct l_statfs_buf * */
		*n_args = 2;
		break;
	}
	/* linux_fstatfs */
	case 138: {
		struct linux_fstatfs_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		uarg[a++] = (intptr_t)p->buf; /* struct l_statfs_buf * */
		*n_args = 2;
		break;
	}
	/* linux_sysfs */
	case 139: {
		struct linux_sysfs_args *p = params;
		iarg[a++] = p->option; /* l_int */
		iarg[a++] = p->arg1; /* l_ulong */
		iarg[a++] = p->arg2; /* l_ulong */
		*n_args = 3;
		break;
	}
	/* linux_getpriority */
	case 140: {
		struct linux_getpriority_args *p = params;
		iarg[a++] = p->which; /* l_int */
		iarg[a++] = p->who; /* l_int */
		*n_args = 2;
		break;
	}
	/* setpriority */
	case 141: {
		struct setpriority_args *p = params;
		iarg[a++] = p->which; /* int */
		iarg[a++] = p->who; /* int */
		iarg[a++] = p->prio; /* int */
		*n_args = 3;
		break;
	}
	/* linux_sched_setparam */
	case 142: {
		struct linux_sched_setparam_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->param; /* struct sched_param * */
		*n_args = 2;
		break;
	}
	/* linux_sched_getparam */
	case 143: {
		struct linux_sched_getparam_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->param; /* struct sched_param * */
		*n_args = 2;
		break;
	}
	/* linux_sched_setscheduler */
	case 144: {
		struct linux_sched_setscheduler_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->policy; /* l_int */
		uarg[a++] = (intptr_t)p->param; /* struct sched_param * */
		*n_args = 3;
		break;
	}
	/* linux_sched_getscheduler */
	case 145: {
		struct linux_sched_getscheduler_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		*n_args = 1;
		break;
	}
	/* linux_sched_get_priority_max */
	case 146: {
		struct linux_sched_get_priority_max_args *p = params;
		iarg[a++] = p->policy; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_sched_get_priority_min */
	case 147: {
		struct linux_sched_get_priority_min_args *p = params;
		iarg[a++] = p->policy; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_sched_rr_get_interval */
	case 148: {
		struct linux_sched_rr_get_interval_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->interval; /* struct l_timespec * */
		*n_args = 2;
		break;
	}
	/* mlock */
	case 149: {
		struct mlock_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* const void * */
		uarg[a++] = p->len; /* size_t */
		*n_args = 2;
		break;
	}
	/* munlock */
	case 150: {
		struct munlock_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* const void * */
		uarg[a++] = p->len; /* size_t */
		*n_args = 2;
		break;
	}
	/* mlockall */
	case 151: {
		struct mlockall_args *p = params;
		iarg[a++] = p->how; /* int */
		*n_args = 1;
		break;
	}
	/* munlockall */
	case 152: {
		*n_args = 0;
		break;
	}
	/* linux_vhangup */
	case 153: {
		*n_args = 0;
		break;
	}
	/* linux_modify_ldt */
	case 154: {
		*n_args = 0;
		break;
	}
	/* linux_pivot_root */
	case 155: {
		*n_args = 0;
		break;
	}
	/* linux_sysctl */
	case 156: {
		struct linux_sysctl_args *p = params;
		uarg[a++] = (intptr_t)p->args; /* struct l___sysctl_args * */
		*n_args = 1;
		break;
	}
	/* linux_prctl */
	case 157: {
		struct linux_prctl_args *p = params;
		iarg[a++] = p->option; /* l_int */
		uarg[a++] = (intptr_t)p->arg2; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->arg3; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->arg4; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->arg5; /* l_uintptr_t */
		*n_args = 5;
		break;
	}
	/* linux_arch_prctl */
	case 158: {
		struct linux_arch_prctl_args *p = params;
		iarg[a++] = p->code; /* l_int */
		iarg[a++] = p->addr; /* l_ulong */
		*n_args = 2;
		break;
	}
	/* linux_adjtimex */
	case 159: {
		*n_args = 0;
		break;
	}
	/* linux_setrlimit */
	case 160: {
		struct linux_setrlimit_args *p = params;
		iarg[a++] = p->resource; /* l_uint */
		uarg[a++] = (intptr_t)p->rlim; /* struct l_rlimit * */
		*n_args = 2;
		break;
	}
	/* chroot */
	case 161: {
		struct chroot_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		*n_args = 1;
		break;
	}
	/* sync */
	case 162: {
		*n_args = 0;
		break;
	}
	/* acct */
	case 163: {
		struct acct_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		*n_args = 1;
		break;
	}
	/* settimeofday */
	case 164: {
		struct settimeofday_args *p = params;
		uarg[a++] = (intptr_t)p->tv; /* struct l_timeval * */
		uarg[a++] = (intptr_t)p->tzp; /* struct timezone * */
		*n_args = 2;
		break;
	}
	/* linux_mount */
	case 165: {
		struct linux_mount_args *p = params;
		uarg[a++] = (intptr_t)p->specialfile; /* char * */
		uarg[a++] = (intptr_t)p->dir; /* char * */
		uarg[a++] = (intptr_t)p->filesystemtype; /* char * */
		iarg[a++] = p->rwflag; /* l_ulong */
		uarg[a++] = (intptr_t)p->data; /* void * */
		*n_args = 5;
		break;
	}
	/* linux_umount */
	case 166: {
		struct linux_umount_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 2;
		break;
	}
	/* swapon */
	case 167: {
		struct swapon_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* char * */
		*n_args = 1;
		break;
	}
	/* linux_swapoff */
	case 168: {
		*n_args = 0;
		break;
	}
	/* linux_reboot */
	case 169: {
		struct linux_reboot_args *p = params;
		iarg[a++] = p->magic1; /* l_int */
		iarg[a++] = p->magic2; /* l_int */
		iarg[a++] = p->cmd; /* l_uint */
		uarg[a++] = (intptr_t)p->arg; /* void * */
		*n_args = 4;
		break;
	}
	/* linux_sethostname */
	case 170: {
		struct linux_sethostname_args *p = params;
		uarg[a++] = (intptr_t)p->hostname; /* char * */
		iarg[a++] = p->len; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_setdomainname */
	case 171: {
		struct linux_setdomainname_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* char * */
		iarg[a++] = p->len; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_iopl */
	case 172: {
		struct linux_iopl_args *p = params;
		iarg[a++] = p->level; /* l_uint */
		*n_args = 1;
		break;
	}
	/* linux_ioperm */
	case 173: {
		*n_args = 0;
		break;
	}
	/* linux_init_module */
	case 175: {
		*n_args = 0;
		break;
	}
	/* linux_delete_module */
	case 176: {
		*n_args = 0;
		break;
	}
	/* linux_quotactl */
	case 179: {
		*n_args = 0;
		break;
	}
	/* linux_gettid */
	case 186: {
		*n_args = 0;
		break;
	}
	/* linux_readahead */
	case 187: {
		*n_args = 0;
		break;
	}
	/* linux_setxattr */
	case 188: {
		struct linux_setxattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = (intptr_t)p->value; /* void * */
		iarg[a++] = p->size; /* l_size_t */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 5;
		break;
	}
	/* linux_lsetxattr */
	case 189: {
		struct linux_lsetxattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = (intptr_t)p->value; /* void * */
		iarg[a++] = p->size; /* l_size_t */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 5;
		break;
	}
	/* linux_fsetxattr */
	case 190: {
		struct linux_fsetxattr_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = (intptr_t)p->value; /* void * */
		iarg[a++] = p->size; /* l_size_t */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 5;
		break;
	}
	/* linux_getxattr */
	case 191: {
		struct linux_getxattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = (intptr_t)p->value; /* void * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_lgetxattr */
	case 192: {
		struct linux_lgetxattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = (intptr_t)p->value; /* void * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_fgetxattr */
	case 193: {
		struct linux_fgetxattr_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = (intptr_t)p->value; /* void * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_listxattr */
	case 194: {
		struct linux_listxattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->list; /* char * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* linux_llistxattr */
	case 195: {
		struct linux_llistxattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->list; /* char * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* linux_flistxattr */
	case 196: {
		struct linux_flistxattr_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->list; /* char * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* linux_removexattr */
	case 197: {
		struct linux_removexattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 2;
		break;
	}
	/* linux_lremovexattr */
	case 198: {
		struct linux_lremovexattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 2;
		break;
	}
	/* linux_fremovexattr */
	case 199: {
		struct linux_fremovexattr_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 2;
		break;
	}
	/* linux_tkill */
	case 200: {
		struct linux_tkill_args *p = params;
		iarg[a++] = p->tid; /* l_pid_t */
		iarg[a++] = p->sig; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_time */
	case 201: {
		struct linux_time_args *p = params;
		uarg[a++] = (intptr_t)p->tm; /* l_time_t * */
		*n_args = 1;
		break;
	}
	/* linux_sys_futex */
	case 202: {
		struct linux_sys_futex_args *p = params;
		uarg[a++] = (intptr_t)p->uaddr; /* uint32_t * */
		iarg[a++] = p->op; /* l_int */
		uarg[a++] = p->val; /* uint32_t */
		uarg[a++] = (intptr_t)p->timeout; /* struct l_timespec * */
		uarg[a++] = (intptr_t)p->uaddr2; /* uint32_t * */
		uarg[a++] = p->val3; /* uint32_t */
		*n_args = 6;
		break;
	}
	/* linux_sched_setaffinity */
	case 203: {
		struct linux_sched_setaffinity_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->len; /* l_uint */
		uarg[a++] = (intptr_t)p->user_mask_ptr; /* l_ulong * */
		*n_args = 3;
		break;
	}
	/* linux_sched_getaffinity */
	case 204: {
		struct linux_sched_getaffinity_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->len; /* l_uint */
		uarg[a++] = (intptr_t)p->user_mask_ptr; /* l_ulong * */
		*n_args = 3;
		break;
	}
	/* linux_io_setup */
	case 206: {
		*n_args = 0;
		break;
	}
	/* linux_io_destroy */
	case 207: {
		*n_args = 0;
		break;
	}
	/* linux_io_getevents */
	case 208: {
		*n_args = 0;
		break;
	}
	/* linux_io_submit */
	case 209: {
		*n_args = 0;
		break;
	}
	/* linux_io_cancel */
	case 210: {
		*n_args = 0;
		break;
	}
	/* linux_lookup_dcookie */
	case 212: {
		*n_args = 0;
		break;
	}
	/* linux_epoll_create */
	case 213: {
		struct linux_epoll_create_args *p = params;
		iarg[a++] = p->size; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_remap_file_pages */
	case 216: {
		*n_args = 0;
		break;
	}
	/* linux_getdents64 */
	case 217: {
		struct linux_getdents64_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		uarg[a++] = (intptr_t)p->dirent; /* void * */
		iarg[a++] = p->count; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_set_tid_address */
	case 218: {
		struct linux_set_tid_address_args *p = params;
		uarg[a++] = (intptr_t)p->tidptr; /* l_int * */
		*n_args = 1;
		break;
	}
	/* linux_restart_syscall */
	case 219: {
		*n_args = 0;
		break;
	}
	/* linux_semtimedop */
	case 220: {
		struct linux_semtimedop_args *p = params;
		iarg[a++] = p->semid; /* l_int */
		uarg[a++] = (intptr_t)p->tsops; /* struct sembuf * */
		iarg[a++] = p->nsops; /* l_size_t */
		uarg[a++] = (intptr_t)p->timeout; /* struct l_timespec * */
		*n_args = 4;
		break;
	}
	/* linux_fadvise64 */
	case 221: {
		struct linux_fadvise64_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->offset; /* l_loff_t */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->advice; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_timer_create */
	case 222: {
		struct linux_timer_create_args *p = params;
		iarg[a++] = p->clock_id; /* clockid_t */
		uarg[a++] = (intptr_t)p->evp; /* struct l_sigevent * */
		uarg[a++] = (intptr_t)p->timerid; /* l_timer_t * */
		*n_args = 3;
		break;
	}
	/* linux_timer_settime */
	case 223: {
		struct linux_timer_settime_args *p = params;
		iarg[a++] = p->timerid; /* l_timer_t */
		iarg[a++] = p->flags; /* l_int */
		uarg[a++] = (intptr_t)p->new; /* const struct itimerspec * */
		uarg[a++] = (intptr_t)p->old; /* struct itimerspec * */
		*n_args = 4;
		break;
	}
	/* linux_timer_gettime */
	case 224: {
		struct linux_timer_gettime_args *p = params;
		iarg[a++] = p->timerid; /* l_timer_t */
		uarg[a++] = (intptr_t)p->setting; /* struct itimerspec * */
		*n_args = 2;
		break;
	}
	/* linux_timer_getoverrun */
	case 225: {
		struct linux_timer_getoverrun_args *p = params;
		iarg[a++] = p->timerid; /* l_timer_t */
		*n_args = 1;
		break;
	}
	/* linux_timer_delete */
	case 226: {
		struct linux_timer_delete_args *p = params;
		iarg[a++] = p->timerid; /* l_timer_t */
		*n_args = 1;
		break;
	}
	/* linux_clock_settime */
	case 227: {
		struct linux_clock_settime_args *p = params;
		iarg[a++] = p->which; /* clockid_t */
		uarg[a++] = (intptr_t)p->tp; /* struct l_timespec * */
		*n_args = 2;
		break;
	}
	/* linux_clock_gettime */
	case 228: {
		struct linux_clock_gettime_args *p = params;
		iarg[a++] = p->which; /* clockid_t */
		uarg[a++] = (intptr_t)p->tp; /* struct l_timespec * */
		*n_args = 2;
		break;
	}
	/* linux_clock_getres */
	case 229: {
		struct linux_clock_getres_args *p = params;
		iarg[a++] = p->which; /* clockid_t */
		uarg[a++] = (intptr_t)p->tp; /* struct l_timespec * */
		*n_args = 2;
		break;
	}
	/* linux_clock_nanosleep */
	case 230: {
		struct linux_clock_nanosleep_args *p = params;
		iarg[a++] = p->which; /* clockid_t */
		iarg[a++] = p->flags; /* l_int */
		uarg[a++] = (intptr_t)p->rqtp; /* struct l_timespec * */
		uarg[a++] = (intptr_t)p->rmtp; /* struct l_timespec * */
		*n_args = 4;
		break;
	}
	/* linux_exit_group */
	case 231: {
		struct linux_exit_group_args *p = params;
		iarg[a++] = p->error_code; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_epoll_wait */
	case 232: {
		struct linux_epoll_wait_args *p = params;
		iarg[a++] = p->epfd; /* l_int */
		uarg[a++] = (intptr_t)p->events; /* struct epoll_event * */
		iarg[a++] = p->maxevents; /* l_int */
		iarg[a++] = p->timeout; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_epoll_ctl */
	case 233: {
		struct linux_epoll_ctl_args *p = params;
		iarg[a++] = p->epfd; /* l_int */
		iarg[a++] = p->op; /* l_int */
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->event; /* struct epoll_event * */
		*n_args = 4;
		break;
	}
	/* linux_tgkill */
	case 234: {
		struct linux_tgkill_args *p = params;
		iarg[a++] = p->tgid; /* l_pid_t */
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->sig; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_utimes */
	case 235: {
		struct linux_utimes_args *p = params;
		uarg[a++] = (intptr_t)p->fname; /* char * */
		uarg[a++] = (intptr_t)p->tptr; /* struct l_timeval * */
		*n_args = 2;
		break;
	}
	/* linux_mbind */
	case 237: {
		*n_args = 0;
		break;
	}
	/* linux_set_mempolicy */
	case 238: {
		*n_args = 0;
		break;
	}
	/* linux_get_mempolicy */
	case 239: {
		*n_args = 0;
		break;
	}
	/* linux_mq_open */
	case 240: {
		struct linux_mq_open_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* const char * */
		iarg[a++] = p->oflag; /* l_int */
		iarg[a++] = p->mode; /* l_mode_t */
		uarg[a++] = (intptr_t)p->attr; /* struct mq_attr * */
		*n_args = 4;
		break;
	}
	/* linux_mq_unlink */
	case 241: {
		struct linux_mq_unlink_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 1;
		break;
	}
	/* linux_mq_timedsend */
	case 242: {
		struct linux_mq_timedsend_args *p = params;
		iarg[a++] = p->mqd; /* l_mqd_t */
		uarg[a++] = (intptr_t)p->msg_ptr; /* const char * */
		iarg[a++] = p->msg_len; /* l_size_t */
		iarg[a++] = p->msg_prio; /* l_uint */
		uarg[a++] = (intptr_t)p->abs_timeout; /* const struct l_timespec * */
		*n_args = 5;
		break;
	}
	/* linux_mq_timedreceive */
	case 243: {
		struct linux_mq_timedreceive_args *p = params;
		iarg[a++] = p->mqd; /* l_mqd_t */
		uarg[a++] = (intptr_t)p->msg_ptr; /* char * */
		iarg[a++] = p->msg_len; /* l_size_t */
		uarg[a++] = (intptr_t)p->msg_prio; /* l_uint * */
		uarg[a++] = (intptr_t)p->abs_timeout; /* const struct l_timespec * */
		*n_args = 5;
		break;
	}
	/* linux_mq_notify */
	case 244: {
		struct linux_mq_notify_args *p = params;
		iarg[a++] = p->mqd; /* l_mqd_t */
		uarg[a++] = (intptr_t)p->sevp; /* const struct l_sigevent * */
		*n_args = 2;
		break;
	}
	/* linux_mq_getsetattr */
	case 245: {
		struct linux_mq_getsetattr_args *p = params;
		iarg[a++] = p->mqd; /* l_mqd_t */
		uarg[a++] = (intptr_t)p->attr; /* const struct mq_attr * */
		uarg[a++] = (intptr_t)p->oattr; /* struct mq_attr * */
		*n_args = 3;
		break;
	}
	/* linux_kexec_load */
	case 246: {
		*n_args = 0;
		break;
	}
	/* linux_waitid */
	case 247: {
		struct linux_waitid_args *p = params;
		iarg[a++] = p->idtype; /* l_int */
		iarg[a++] = p->id; /* l_pid_t */
		uarg[a++] = (intptr_t)p->info; /* l_siginfo_t * */
		iarg[a++] = p->options; /* l_int */
		uarg[a++] = (intptr_t)p->rusage; /* struct rusage * */
		*n_args = 5;
		break;
	}
	/* linux_add_key */
	case 248: {
		*n_args = 0;
		break;
	}
	/* linux_request_key */
	case 249: {
		*n_args = 0;
		break;
	}
	/* linux_keyctl */
	case 250: {
		*n_args = 0;
		break;
	}
	/* linux_ioprio_set */
	case 251: {
		struct linux_ioprio_set_args *p = params;
		iarg[a++] = p->which; /* l_int */
		iarg[a++] = p->who; /* l_int */
		iarg[a++] = p->ioprio; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_ioprio_get */
	case 252: {
		struct linux_ioprio_get_args *p = params;
		iarg[a++] = p->which; /* l_int */
		iarg[a++] = p->who; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_inotify_init */
	case 253: {
		*n_args = 0;
		break;
	}
	/* linux_inotify_add_watch */
	case 254: {
		struct linux_inotify_add_watch_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->pathname; /* const char * */
		uarg[a++] = p->mask; /* uint32_t */
		*n_args = 3;
		break;
	}
	/* linux_inotify_rm_watch */
	case 255: {
		struct linux_inotify_rm_watch_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = p->wd; /* uint32_t */
		*n_args = 2;
		break;
	}
	/* linux_migrate_pages */
	case 256: {
		*n_args = 0;
		break;
	}
	/* linux_openat */
	case 257: {
		struct linux_openat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->flags; /* l_int */
		iarg[a++] = p->mode; /* l_mode_t */
		*n_args = 4;
		break;
	}
	/* linux_mkdirat */
	case 258: {
		struct linux_mkdirat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->pathname; /* const char * */
		iarg[a++] = p->mode; /* l_mode_t */
		*n_args = 3;
		break;
	}
	/* linux_mknodat */
	case 259: {
		struct linux_mknodat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->mode; /* l_mode_t */
		iarg[a++] = p->dev; /* l_dev_t */
		*n_args = 4;
		break;
	}
	/* linux_fchownat */
	case 260: {
		struct linux_fchownat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->uid; /* l_uid_t */
		iarg[a++] = p->gid; /* l_gid_t */
		iarg[a++] = p->flag; /* l_int */
		*n_args = 5;
		break;
	}
	/* linux_futimesat */
	case 261: {
		struct linux_futimesat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* char * */
		uarg[a++] = (intptr_t)p->utimes; /* struct l_timeval * */
		*n_args = 3;
		break;
	}
	/* linux_newfstatat */
	case 262: {
		struct linux_newfstatat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->pathname; /* char * */
		uarg[a++] = (intptr_t)p->statbuf; /* struct l_stat64 * */
		iarg[a++] = p->flag; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_unlinkat */
	case 263: {
		struct linux_unlinkat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->pathname; /* const char * */
		iarg[a++] = p->flag; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_renameat */
	case 264: {
		struct linux_renameat_args *p = params;
		iarg[a++] = p->olddfd; /* l_int */
		uarg[a++] = (intptr_t)p->oldname; /* const char * */
		iarg[a++] = p->newdfd; /* l_int */
		uarg[a++] = (intptr_t)p->newname; /* const char * */
		*n_args = 4;
		break;
	}
	/* linux_linkat */
	case 265: {
		struct linux_linkat_args *p = params;
		iarg[a++] = p->olddfd; /* l_int */
		uarg[a++] = (intptr_t)p->oldname; /* const char * */
		iarg[a++] = p->newdfd; /* l_int */
		uarg[a++] = (intptr_t)p->newname; /* const char * */
		iarg[a++] = p->flag; /* l_int */
		*n_args = 5;
		break;
	}
	/* linux_symlinkat */
	case 266: {
		struct linux_symlinkat_args *p = params;
		uarg[a++] = (intptr_t)p->oldname; /* const char * */
		iarg[a++] = p->newdfd; /* l_int */
		uarg[a++] = (intptr_t)p->newname; /* const char * */
		*n_args = 3;
		break;
	}
	/* linux_readlinkat */
	case 267: {
		struct linux_readlinkat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->bufsiz; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_fchmodat */
	case 268: {
		struct linux_fchmodat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->mode; /* l_mode_t */
		*n_args = 3;
		break;
	}
	/* linux_faccessat */
	case 269: {
		struct linux_faccessat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->amode; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_pselect6 */
	case 270: {
		struct linux_pselect6_args *p = params;
		iarg[a++] = p->nfds; /* l_int */
		uarg[a++] = (intptr_t)p->readfds; /* l_fd_set * */
		uarg[a++] = (intptr_t)p->writefds; /* l_fd_set * */
		uarg[a++] = (intptr_t)p->exceptfds; /* l_fd_set * */
		uarg[a++] = (intptr_t)p->tsp; /* struct l_timespec * */
		uarg[a++] = (intptr_t)p->sig; /* l_uintptr_t * */
		*n_args = 6;
		break;
	}
	/* linux_ppoll */
	case 271: {
		struct linux_ppoll_args *p = params;
		uarg[a++] = (intptr_t)p->fds; /* struct pollfd * */
		iarg[a++] = p->nfds; /* l_uint */
		uarg[a++] = (intptr_t)p->tsp; /* struct l_timespec * */
		uarg[a++] = (intptr_t)p->sset; /* l_sigset_t * */
		iarg[a++] = p->ssize; /* l_size_t */
		*n_args = 5;
		break;
	}
	/* linux_unshare */
	case 272: {
		*n_args = 0;
		break;
	}
	/* linux_set_robust_list */
	case 273: {
		struct linux_set_robust_list_args *p = params;
		uarg[a++] = (intptr_t)p->head; /* struct linux_robust_list_head * */
		iarg[a++] = p->len; /* l_size_t */
		*n_args = 2;
		break;
	}
	/* linux_get_robust_list */
	case 274: {
		struct linux_get_robust_list_args *p = params;
		iarg[a++] = p->pid; /* l_int */
		uarg[a++] = (intptr_t)p->head; /* struct linux_robust_list_head ** */
		uarg[a++] = (intptr_t)p->len; /* l_size_t * */
		*n_args = 3;
		break;
	}
	/* linux_splice */
	case 275: {
		struct linux_splice_args *p = params;
		iarg[a++] = p->fd_in; /* int */
		uarg[a++] = (intptr_t)p->off_in; /* l_loff_t * */
		iarg[a++] = p->fd_out; /* int */
		uarg[a++] = (intptr_t)p->off_out; /* l_loff_t * */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 6;
		break;
	}
	/* linux_tee */
	case 276: {
		*n_args = 0;
		break;
	}
	/* linux_sync_file_range */
	case 277: {
		struct linux_sync_file_range_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->offset; /* l_loff_t */
		iarg[a++] = p->nbytes; /* l_loff_t */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 4;
		break;
	}
	/* linux_vmsplice */
	case 278: {
		*n_args = 0;
		break;
	}
	/* linux_move_pages */
	case 279: {
		*n_args = 0;
		break;
	}
	/* linux_utimensat */
	case 280: {
		struct linux_utimensat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->pathname; /* const char * */
		uarg[a++] = (intptr_t)p->times; /* const struct l_timespec * */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_epoll_pwait */
	case 281: {
		struct linux_epoll_pwait_args *p = params;
		iarg[a++] = p->epfd; /* l_int */
		uarg[a++] = (intptr_t)p->events; /* struct epoll_event * */
		iarg[a++] = p->maxevents; /* l_int */
		iarg[a++] = p->timeout; /* l_int */
		uarg[a++] = (intptr_t)p->mask; /* l_sigset_t * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 6;
		break;
	}
	/* linux_signalfd */
	case 282: {
		*n_args = 0;
		break;
	}
	/* linux_timerfd_create */
	case 283: {
		struct linux_timerfd_create_args *p = params;
		iarg[a++] = p->clockid; /* l_int */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_eventfd */
	case 284: {
		struct linux_eventfd_args *p = params;
		iarg[a++] = p->initval; /* l_uint */
		*n_args = 1;
		break;
	}
	/* linux_fallocate */
	case 285: {
		struct linux_fallocate_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->mode; /* l_int */
		iarg[a++] = p->offset; /* l_loff_t */
		iarg[a++] = p->len; /* l_loff_t */
		*n_args = 4;
		break;
	}
	/* linux_timerfd_settime */
	case 286: {
		struct linux_timerfd_settime_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->flags; /* l_int */
		uarg[a++] = (intptr_t)p->new_value; /* const struct l_itimerspec * */
		uarg[a++] = (intptr_t)p->old_value; /* struct l_itimerspec * */
		*n_args = 4;
		break;
	}
	/* linux_timerfd_gettime */
	case 287: {
		struct linux_timerfd_gettime_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->old_value; /* struct l_itimerspec * */
		*n_args = 2;
		break;
	}
	/* linux_accept4 */
	case 288: {
		struct linux_accept4_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->addr; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->namelen; /* l_uintptr_t */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_signalfd4 */
	case 289: {
		*n_args = 0;
		break;
	}
	/* linux_eventfd2 */
	case 290: {
		struct linux_eventfd2_args *p = params;
		iarg[a++] = p->initval; /* l_uint */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_epoll_create1 */
	case 291: {
		struct linux_epoll_create1_args *p = params;
		iarg[a++] = p->flags; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_dup3 */
	case 292: {
		struct linux_dup3_args *p = params;
		iarg[a++] = p->oldfd; /* l_uint */
		iarg[a++] = p->newfd; /* l_uint */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_pipe2 */
	case 293: {
		struct linux_pipe2_args *p = params;
		uarg[a++] = (intptr_t)p->pipefds; /* l_int * */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_inotify_init1 */
	case 294: {
		struct linux_inotify_init1_args *p = params;
		iarg[a++] = p->flags; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_preadv */
	case 295: {
		struct linux_preadv_args *p = params;
		iarg[a++] = p->fd; /* l_ulong */
		uarg[a++] = (intptr_t)p->vec; /* struct iovec * */
		iarg[a++] = p->vlen; /* l_ulong */
		iarg[a++] = p->pos_l; /* l_ulong */
		iarg[a++] = p->pos_h; /* l_ulong */
		*n_args = 5;
		break;
	}
	/* linux_pwritev */
	case 296: {
		struct linux_pwritev_args *p = params;
		iarg[a++] = p->fd; /* l_ulong */
		uarg[a++] = (intptr_t)p->vec; /* struct iovec * */
		iarg[a++] = p->vlen; /* l_ulong */
		iarg[a++] = p->pos_l; /* l_ulong */
		iarg[a++] = p->pos_h; /* l_ulong */
		*n_args = 5;
		break;
	}
	/* linux_rt_tgsigqueueinfo */
	case 297: {
		struct linux_rt_tgsigqueueinfo_args *p = params;
		iarg[a++] = p->tgid; /* l_pid_t */
		iarg[a++] = p->tid; /* l_pid_t */
		iarg[a++] = p->sig; /* l_int */
		uarg[a++] = (intptr_t)p->uinfo; /* l_siginfo_t * */
		*n_args = 4;
		break;
	}
	/* linux_perf_event_open */
	case 298: {
		*n_args = 0;
		break;
	}
	/* linux_recvmmsg */
	case 299: {
		struct linux_recvmmsg_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->msg; /* struct l_mmsghdr * */
		iarg[a++] = p->vlen; /* l_uint */
		iarg[a++] = p->flags; /* l_uint */
		uarg[a++] = (intptr_t)p->timeout; /* struct l_timespec * */
		*n_args = 5;
		break;
	}
	/* linux_fanotify_init */
	case 300: {
		*n_args = 0;
		break;
	}
	/* linux_fanotify_mark */
	case 301: {
		*n_args = 0;
		break;
	}
	/* linux_prlimit64 */
	case 302: {
		struct linux_prlimit64_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->resource; /* l_uint */
		uarg[a++] = (intptr_t)p->new; /* struct rlimit * */
		uarg[a++] = (intptr_t)p->old; /* struct rlimit * */
		*n_args = 4;
		break;
	}
	/* linux_name_to_handle_at */
	case 303: {
		struct linux_name_to_handle_at_args *p = params;
		iarg[a++] = p->dirfd; /* l_int */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = (intptr_t)p->handle; /* struct l_file_handle * */
		uarg[a++] = (intptr_t)p->mnt_id; /* l_int * */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 5;
		break;
	}
	/* linux_open_by_handle_at */
	case 304: {
		struct linux_open_by_handle_at_args *p = params;
		iarg[a++] = p->mountdirfd; /* l_int */
		uarg[a++] = (intptr_t)p->handle; /* struct l_file_handle * */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_clock_adjtime */
	case 305: {
		*n_args = 0;
		break;
	}
	/* linux_syncfs */
	case 306: {
		struct linux_syncfs_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_sendmmsg */
	case 307: {
		struct linux_sendmmsg_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->msg; /* struct l_mmsghdr * */
		iarg[a++] = p->vlen; /* l_uint */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 4;
		break;
	}
	/* linux_setns */
	case 308: {
		struct linux_setns_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->nstype; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_getcpu */
	case 309: {
		struct linux_getcpu_args *p = params;
		uarg[a++] = (intptr_t)p->cpu; /* l_uint * */
		uarg[a++] = (intptr_t)p->node; /* l_uint * */
		uarg[a++] = (intptr_t)p->cache; /* void * */
		*n_args = 3;
		break;
	}
	/* linux_process_vm_readv */
	case 310: {
		struct linux_process_vm_readv_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->lvec; /* const struct iovec * */
		iarg[a++] = p->liovcnt; /* l_ulong */
		uarg[a++] = (intptr_t)p->rvec; /* const struct iovec * */
		iarg[a++] = p->riovcnt; /* l_ulong */
		iarg[a++] = p->flags; /* l_ulong */
		*n_args = 6;
		break;
	}
	/* linux_process_vm_writev */
	case 311: {
		struct linux_process_vm_writev_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->lvec; /* const struct iovec * */
		iarg[a++] = p->liovcnt; /* l_ulong */
		uarg[a++] = (intptr_t)p->rvec; /* const struct iovec * */
		iarg[a++] = p->riovcnt; /* l_ulong */
		iarg[a++] = p->flags; /* l_ulong */
		*n_args = 6;
		break;
	}
	/* linux_kcmp */
	case 312: {
		struct linux_kcmp_args *p = params;
		iarg[a++] = p->pid1; /* l_pid_t */
		iarg[a++] = p->pid2; /* l_pid_t */
		iarg[a++] = p->type; /* l_int */
		iarg[a++] = p->idx1; /* l_ulong */
		iarg[a++] = p->idx; /* l_ulong */
		*n_args = 5;
		break;
	}
	/* linux_finit_module */
	case 313: {
		struct linux_finit_module_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->uargs; /* const char * */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_sched_setattr */
	case 314: {
		struct linux_sched_setattr_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->attr; /* void * */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_sched_getattr */
	case 315: {
		struct linux_sched_getattr_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->attr; /* void * */
		iarg[a++] = p->size; /* l_uint */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 4;
		break;
	}
	/* linux_renameat2 */
	case 316: {
		struct linux_renameat2_args *p = params;
		iarg[a++] = p->olddfd; /* l_int */
		uarg[a++] = (intptr_t)p->oldname; /* const char * */
		iarg[a++] = p->newdfd; /* l_int */
		uarg[a++] = (intptr_t)p->newname; /* const char * */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 5;
		break;
	}
	/* linux_seccomp */
	case 317: {
		struct linux_seccomp_args *p = params;
		iarg[a++] = p->op; /* l_uint */
		iarg[a++] = p->flags; /* l_uint */
		uarg[a++] = (intptr_t)p->uargs; /* const char * */
		*n_args = 3;
		break;
	}
	/* linux_getrandom */
	case 318: {
		struct linux_getrandom_args *p = params;
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->count; /* l_size_t */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_memfd_create */
	case 319: {
		struct linux_memfd_create_args *p = params;
		uarg[a++] = (intptr_t)p->uname_ptr; /* const char * */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 2;
		break;
	}
	/* linux_kexec_file_load */
	case 320: {
		struct linux_kexec_file_load_args *p = params;
		iarg[a++] = p->kernel_fd; /* l_int */
		iarg[a++] = p->initrd_fd; /* l_int */
		iarg[a++] = p->cmdline_len; /* l_ulong */
		uarg[a++] = (intptr_t)p->cmdline_ptr; /* const char * */
		iarg[a++] = p->flags; /* l_ulong */
		*n_args = 5;
		break;
	}
	/* linux_bpf */
	case 321: {
		struct linux_bpf_args *p = params;
		iarg[a++] = p->cmd; /* l_int */
		uarg[a++] = (intptr_t)p->attr; /* void * */
		iarg[a++] = p->size; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_execveat */
	case 322: {
		struct linux_execveat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		uarg[a++] = (intptr_t)p->argv; /* const char ** */
		uarg[a++] = (intptr_t)p->envp; /* const char ** */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 5;
		break;
	}
	/* linux_userfaultfd */
	case 323: {
		struct linux_userfaultfd_args *p = params;
		iarg[a++] = p->flags; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_membarrier */
	case 324: {
		struct linux_membarrier_args *p = params;
		iarg[a++] = p->cmd; /* l_int */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_mlock2 */
	case 325: {
		struct linux_mlock2_args *p = params;
		iarg[a++] = p->start; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_copy_file_range */
	case 326: {
		struct linux_copy_file_range_args *p = params;
		iarg[a++] = p->fd_in; /* l_int */
		uarg[a++] = (intptr_t)p->off_in; /* l_loff_t * */
		iarg[a++] = p->fd_out; /* l_int */
		uarg[a++] = (intptr_t)p->off_out; /* l_loff_t * */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 6;
		break;
	}
	/* linux_preadv2 */
	case 327: {
		struct linux_preadv2_args *p = params;
		iarg[a++] = p->fd; /* l_ulong */
		uarg[a++] = (intptr_t)p->vec; /* const struct iovec * */
		iarg[a++] = p->vlen; /* l_ulong */
		iarg[a++] = p->pos_l; /* l_ulong */
		iarg[a++] = p->pos_h; /* l_ulong */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 6;
		break;
	}
	/* linux_pwritev2 */
	case 328: {
		struct linux_pwritev2_args *p = params;
		iarg[a++] = p->fd; /* l_ulong */
		uarg[a++] = (intptr_t)p->vec; /* const struct iovec * */
		iarg[a++] = p->vlen; /* l_ulong */
		iarg[a++] = p->pos_l; /* l_ulong */
		iarg[a++] = p->pos_h; /* l_ulong */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 6;
		break;
	}
	/* linux_pkey_mprotect */
	case 329: {
		struct linux_pkey_mprotect_args *p = params;
		iarg[a++] = p->start; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->prot; /* l_ulong */
		iarg[a++] = p->pkey; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_pkey_alloc */
	case 330: {
		struct linux_pkey_alloc_args *p = params;
		iarg[a++] = p->flags; /* l_ulong */
		iarg[a++] = p->init_val; /* l_ulong */
		*n_args = 2;
		break;
	}
	/* linux_pkey_free */
	case 331: {
		struct linux_pkey_free_args *p = params;
		iarg[a++] = p->pkey; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_statx */
	case 332: {
		struct linux_statx_args *p = params;
		iarg[a++] = p->dirfd; /* l_int */
		uarg[a++] = (intptr_t)p->pathname; /* const char * */
		iarg[a++] = p->flags; /* l_uint */
		iarg[a++] = p->mask; /* l_uint */
		uarg[a++] = (intptr_t)p->statxbuf; /* void * */
		*n_args = 5;
		break;
	}
	/* linux_io_pgetevents */
	case 333: {
		*n_args = 0;
		break;
	}
	/* linux_rseq */
	case 334: {
		struct linux_rseq_args *p = params;
		uarg[a++] = (intptr_t)p->rseq; /* struct linux_rseq * */
		uarg[a++] = p->rseq_len; /* uint32_t */
		iarg[a++] = p->flags; /* l_int */
		uarg[a++] = p->sig; /* uint32_t */
		*n_args = 4;
		break;
	}
	/* linux_pidfd_send_signal */
	case 424: {
		struct linux_pidfd_send_signal_args *p = params;
		iarg[a++] = p->pidfd; /* l_int */
		iarg[a++] = p->sig; /* l_int */
		uarg[a++] = (intptr_t)p->info; /* l_siginfo_t * */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 4;
		break;
	}
	/* linux_io_uring_setup */
	case 425: {
		*n_args = 0;
		break;
	}
	/* linux_io_uring_enter */
	case 426: {
		*n_args = 0;
		break;
	}
	/* linux_io_uring_register */
	case 427: {
		*n_args = 0;
		break;
	}
	/* linux_open_tree */
	case 428: {
		*n_args = 0;
		break;
	}
	/* linux_move_mount */
	case 429: {
		*n_args = 0;
		break;
	}
	/* linux_fsopen */
	case 430: {
		*n_args = 0;
		break;
	}
	/* linux_fsconfig */
	case 431: {
		*n_args = 0;
		break;
	}
	/* linux_fsmount */
	case 432: {
		*n_args = 0;
		break;
	}
	/* linux_fspick */
	case 433: {
		*n_args = 0;
		break;
	}
	/* linux_pidfd_open */
	case 434: {
		*n_args = 0;
		break;
	}
	/* linux_clone3 */
	case 435: {
		struct linux_clone3_args *p = params;
		uarg[a++] = (intptr_t)p->uargs; /* struct l_user_clone_args * */
		iarg[a++] = p->usize; /* l_size_t */
		*n_args = 2;
		break;
	}
	/* linux_close_range */
	case 436: {
		struct linux_close_range_args *p = params;
		iarg[a++] = p->first; /* l_uint */
		iarg[a++] = p->last; /* l_uint */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_openat2 */
	case 437: {
		*n_args = 0;
		break;
	}
	/* linux_pidfd_getfd */
	case 438: {
		*n_args = 0;
		break;
	}
	/* linux_faccessat2 */
	case 439: {
		struct linux_faccessat2_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->amode; /* l_int */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_process_madvise */
	case 440: {
		*n_args = 0;
		break;
	}
	/* linux_epoll_pwait2 */
	case 441: {
		struct linux_epoll_pwait2_args *p = params;
		iarg[a++] = p->epfd; /* l_int */
		uarg[a++] = (intptr_t)p->events; /* struct epoll_event * */
		iarg[a++] = p->maxevents; /* l_int */
		uarg[a++] = (intptr_t)p->timeout; /* struct l_timespec * */
		uarg[a++] = (intptr_t)p->mask; /* l_sigset_t * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 6;
		break;
	}
	/* linux_mount_setattr */
	case 442: {
		*n_args = 0;
		break;
	}
	/* linux_quotactl_fd */
	case 443: {
		*n_args = 0;
		break;
	}
	/* linux_landlock_create_ruleset */
	case 444: {
		*n_args = 0;
		break;
	}
	/* linux_landlock_add_rule */
	case 445: {
		*n_args = 0;
		break;
	}
	/* linux_landlock_restrict_self */
	case 446: {
		*n_args = 0;
		break;
	}
	/* linux_memfd_secret */
	case 447: {
		*n_args = 0;
		break;
	}
	/* linux_process_mrelease */
	case 448: {
		*n_args = 0;
		break;
	}
	/* linux_futex_waitv */
	case 449: {
		*n_args = 0;
		break;
	}
	/* linux_set_mempolicy_home_node */
	case 450: {
		*n_args = 0;
		break;
	}
	/* linux_cachestat */
	case 451: {
		*n_args = 0;
		break;
	}
	/* linux_fchmodat2 */
	case 452: {
		*n_args = 0;
		break;
	}
	/* linux_map_shadow_stack */
	case 453: {
		*n_args = 0;
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
	/* read */
	case 0:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_write */
	case 1:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_open */
	case 2:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_mode_t";
			break;
		default:
			break;
		};
		break;
	/* close */
	case 3:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* linux_newstat */
	case 4:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "userland struct l_newstat *";
			break;
		default:
			break;
		};
		break;
	/* linux_newfstat */
	case 5:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "userland struct l_newstat *";
			break;
		default:
			break;
		};
		break;
	/* linux_newlstat */
	case 6:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "userland struct l_newstat *";
			break;
		default:
			break;
		};
		break;
	/* linux_poll */
	case 7:
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
	/* linux_lseek */
	case 8:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "l_off_t";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_mmap2 */
	case 9:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "l_ulong";
			break;
		case 2:
			p = "l_ulong";
			break;
		case 3:
			p = "l_ulong";
			break;
		case 4:
			p = "l_ulong";
			break;
		case 5:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_mprotect */
	case 10:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "l_size_t";
			break;
		case 2:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* munmap */
	case 11:
		switch (ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_brk */
	case 12:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_rt_sigaction */
	case 13:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland l_sigaction_t *";
			break;
		case 2:
			p = "userland l_sigaction_t *";
			break;
		case 3:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_rt_sigprocmask */
	case 14:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland l_sigset_t *";
			break;
		case 2:
			p = "userland l_sigset_t *";
			break;
		case 3:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_rt_sigreturn */
	case 15:
		switch (ndx) {
		case 0:
			p = "userland struct l_ucontext *";
			break;
		default:
			break;
		};
		break;
	/* linux_ioctl */
	case 16:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "l_uint";
			break;
		case 2:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_pread */
	case 17:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "l_size_t";
			break;
		case 3:
			p = "l_loff_t";
			break;
		default:
			break;
		};
		break;
	/* linux_pwrite */
	case 18:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "l_size_t";
			break;
		case 3:
			p = "l_loff_t";
			break;
		default:
			break;
		};
		break;
	/* readv */
	case 19:
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
		default:
			break;
		};
		break;
	/* linux_writev */
	case 20:
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
		default:
			break;
		};
		break;
	/* linux_access */
	case 21:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_pipe */
	case 22:
		switch (ndx) {
		case 0:
			p = "userland l_int *";
			break;
		default:
			break;
		};
		break;
	/* linux_select */
	case 23:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland l_fd_set *";
			break;
		case 2:
			p = "userland l_fd_set *";
			break;
		case 3:
			p = "userland l_fd_set *";
			break;
		case 4:
			p = "userland struct l_timeval *";
			break;
		default:
			break;
		};
		break;
	/* sched_yield */
	case 24:
		break;
	/* linux_mremap */
	case 25:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "l_ulong";
			break;
		case 2:
			p = "l_ulong";
			break;
		case 3:
			p = "l_ulong";
			break;
		case 4:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_msync */
	case 26:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "l_size_t";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_mincore */
	case 27:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "l_size_t";
			break;
		case 2:
			p = "userland u_char *";
			break;
		default:
			break;
		};
		break;
	/* linux_madvise */
	case 28:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "l_size_t";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_shmget */
	case 29:
		switch (ndx) {
		case 0:
			p = "l_key_t";
			break;
		case 1:
			p = "l_size_t";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_shmat */
	case 30:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_shmctl */
	case 31:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "userland struct l_shmid_ds *";
			break;
		default:
			break;
		};
		break;
	/* dup */
	case 32:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* dup2 */
	case 33:
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
	/* linux_pause */
	case 34:
		break;
	/* linux_nanosleep */
	case 35:
		switch (ndx) {
		case 0:
			p = "userland const struct l_timespec *";
			break;
		case 1:
			p = "userland struct l_timespec *";
			break;
		default:
			break;
		};
		break;
	/* linux_getitimer */
	case 36:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct l_itimerval *";
			break;
		default:
			break;
		};
		break;
	/* linux_alarm */
	case 37:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_setitimer */
	case 38:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct l_itimerval *";
			break;
		case 2:
			p = "userland struct l_itimerval *";
			break;
		default:
			break;
		};
		break;
	/* linux_getpid */
	case 39:
		break;
	/* linux_sendfile */
	case 40:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "userland l_off_t *";
			break;
		case 3:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_socket */
	case 41:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_connect */
	case 42:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_uintptr_t";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_accept */
	case 43:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_uintptr_t";
			break;
		case 2:
			p = "l_uintptr_t";
			break;
		default:
			break;
		};
		break;
	/* linux_sendto */
	case 44:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_uintptr_t";
			break;
		case 2:
			p = "l_size_t";
			break;
		case 3:
			p = "l_uint";
			break;
		case 4:
			p = "l_uintptr_t";
			break;
		case 5:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_recvfrom */
	case 45:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_uintptr_t";
			break;
		case 2:
			p = "l_size_t";
			break;
		case 3:
			p = "l_uint";
			break;
		case 4:
			p = "l_uintptr_t";
			break;
		case 5:
			p = "l_uintptr_t";
			break;
		default:
			break;
		};
		break;
	/* linux_sendmsg */
	case 46:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_uintptr_t";
			break;
		case 2:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_recvmsg */
	case 47:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_uintptr_t";
			break;
		case 2:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_shutdown */
	case 48:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_bind */
	case 49:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_uintptr_t";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_listen */
	case 50:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_getsockname */
	case 51:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_uintptr_t";
			break;
		case 2:
			p = "l_uintptr_t";
			break;
		default:
			break;
		};
		break;
	/* linux_getpeername */
	case 52:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_uintptr_t";
			break;
		case 2:
			p = "l_uintptr_t";
			break;
		default:
			break;
		};
		break;
	/* linux_socketpair */
	case 53:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "l_uintptr_t";
			break;
		default:
			break;
		};
		break;
	/* linux_setsockopt */
	case 54:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "l_uintptr_t";
			break;
		case 4:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_getsockopt */
	case 55:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "l_uintptr_t";
			break;
		case 4:
			p = "l_uintptr_t";
			break;
		default:
			break;
		};
		break;
	/* linux_clone */
	case 56:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "l_ulong";
			break;
		case 2:
			p = "userland l_int *";
			break;
		case 3:
			p = "userland l_int *";
			break;
		case 4:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_fork */
	case 57:
		break;
	/* linux_vfork */
	case 58:
		break;
	/* linux_execve */
	case 59:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "userland l_uintptr_t *";
			break;
		case 2:
			p = "userland l_uintptr_t *";
			break;
		default:
			break;
		};
		break;
	/* linux_exit */
	case 60:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_wait4 */
	case 61:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "userland l_int *";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "userland struct rusage *";
			break;
		default:
			break;
		};
		break;
	/* linux_kill */
	case 62:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_newuname */
	case 63:
		switch (ndx) {
		case 0:
			p = "userland struct l_new_utsname *";
			break;
		default:
			break;
		};
		break;
	/* linux_semget */
	case 64:
		switch (ndx) {
		case 0:
			p = "l_key_t";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* semop */
	case 65:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct sembuf *";
			break;
		case 2:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_semctl */
	case 66:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "union l_semun";
			break;
		default:
			break;
		};
		break;
	/* linux_shmdt */
	case 67:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* linux_msgget */
	case 68:
		switch (ndx) {
		case 0:
			p = "l_key_t";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_msgsnd */
	case 69:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct l_msgbuf *";
			break;
		case 2:
			p = "l_size_t";
			break;
		case 3:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_msgrcv */
	case 70:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct l_msgbuf *";
			break;
		case 2:
			p = "l_size_t";
			break;
		case 3:
			p = "l_long";
			break;
		case 4:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_msgctl */
	case 71:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "userland struct l_msqid_ds *";
			break;
		default:
			break;
		};
		break;
	/* linux_fcntl */
	case 72:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "l_uint";
			break;
		case 2:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* flock */
	case 73:
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
	/* fsync */
	case 74:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* linux_fdatasync */
	case 75:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_truncate */
	case 76:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_long";
			break;
		default:
			break;
		};
		break;
	/* linux_ftruncate */
	case 77:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_getdents */
	case 78:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_getcwd */
	case 79:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_chdir */
	case 80:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* fchdir */
	case 81:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* linux_rename */
	case 82:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* linux_mkdir */
	case 83:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_mode_t";
			break;
		default:
			break;
		};
		break;
	/* linux_rmdir */
	case 84:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* linux_creat */
	case 85:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_mode_t";
			break;
		default:
			break;
		};
		break;
	/* linux_link */
	case 86:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* linux_unlink */
	case 87:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* linux_symlink */
	case 88:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* linux_readlink */
	case 89:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_chmod */
	case 90:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_mode_t";
			break;
		default:
			break;
		};
		break;
	/* fchmod */
	case 91:
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
	/* linux_chown */
	case 92:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_uid_t";
			break;
		case 2:
			p = "l_gid_t";
			break;
		default:
			break;
		};
		break;
	/* fchown */
	case 93:
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
	/* linux_lchown */
	case 94:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_uid_t";
			break;
		case 2:
			p = "l_gid_t";
			break;
		default:
			break;
		};
		break;
	/* umask */
	case 95:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* gettimeofday */
	case 96:
		switch (ndx) {
		case 0:
			p = "userland struct l_timeval *";
			break;
		case 1:
			p = "userland struct timezone *";
			break;
		default:
			break;
		};
		break;
	/* linux_getrlimit */
	case 97:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "userland struct l_rlimit *";
			break;
		default:
			break;
		};
		break;
	/* getrusage */
	case 98:
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
	/* linux_sysinfo */
	case 99:
		switch (ndx) {
		case 0:
			p = "userland struct l_sysinfo *";
			break;
		default:
			break;
		};
		break;
	/* linux_times */
	case 100:
		switch (ndx) {
		case 0:
			p = "userland struct l_times_argv *";
			break;
		default:
			break;
		};
		break;
	/* linux_ptrace */
	case 101:
		switch (ndx) {
		case 0:
			p = "l_long";
			break;
		case 1:
			p = "l_long";
			break;
		case 2:
			p = "l_ulong";
			break;
		case 3:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_getuid */
	case 102:
		break;
	/* linux_syslog */
	case 103:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_getgid */
	case 104:
		break;
	/* setuid */
	case 105:
		switch (ndx) {
		case 0:
			p = "uid_t";
			break;
		default:
			break;
		};
		break;
	/* setgid */
	case 106:
		switch (ndx) {
		case 0:
			p = "gid_t";
			break;
		default:
			break;
		};
		break;
	/* geteuid */
	case 107:
		break;
	/* getegid */
	case 108:
		break;
	/* setpgid */
	case 109:
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
	/* linux_getppid */
	case 110:
		break;
	/* getpgrp */
	case 111:
		break;
	/* setsid */
	case 112:
		break;
	/* setreuid */
	case 113:
		switch (ndx) {
		case 0:
			p = "uid_t";
			break;
		case 1:
			p = "uid_t";
			break;
		default:
			break;
		};
		break;
	/* setregid */
	case 114:
		switch (ndx) {
		case 0:
			p = "gid_t";
			break;
		case 1:
			p = "gid_t";
			break;
		default:
			break;
		};
		break;
	/* linux_getgroups */
	case 115:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland l_gid_t *";
			break;
		default:
			break;
		};
		break;
	/* linux_setgroups */
	case 116:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland l_gid_t *";
			break;
		default:
			break;
		};
		break;
	/* setresuid */
	case 117:
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
	/* getresuid */
	case 118:
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
	/* setresgid */
	case 119:
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
	/* getresgid */
	case 120:
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
	/* getpgid */
	case 121:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* linux_setfsuid */
	case 122:
		switch (ndx) {
		case 0:
			p = "l_uid_t";
			break;
		default:
			break;
		};
		break;
	/* linux_setfsgid */
	case 123:
		switch (ndx) {
		case 0:
			p = "l_gid_t";
			break;
		default:
			break;
		};
		break;
	/* linux_getsid */
	case 124:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		default:
			break;
		};
		break;
	/* linux_capget */
	case 125:
		switch (ndx) {
		case 0:
			p = "userland struct l_user_cap_header *";
			break;
		case 1:
			p = "userland struct l_user_cap_data *";
			break;
		default:
			break;
		};
		break;
	/* linux_capset */
	case 126:
		switch (ndx) {
		case 0:
			p = "userland struct l_user_cap_header *";
			break;
		case 1:
			p = "userland struct l_user_cap_data *";
			break;
		default:
			break;
		};
		break;
	/* linux_rt_sigpending */
	case 127:
		switch (ndx) {
		case 0:
			p = "userland l_sigset_t *";
			break;
		case 1:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_rt_sigtimedwait */
	case 128:
		switch (ndx) {
		case 0:
			p = "userland l_sigset_t *";
			break;
		case 1:
			p = "userland l_siginfo_t *";
			break;
		case 2:
			p = "userland struct l_timespec *";
			break;
		case 3:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_rt_sigqueueinfo */
	case 129:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "userland l_siginfo_t *";
			break;
		default:
			break;
		};
		break;
	/* linux_rt_sigsuspend */
	case 130:
		switch (ndx) {
		case 0:
			p = "userland l_sigset_t *";
			break;
		case 1:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_sigaltstack */
	case 131:
		switch (ndx) {
		case 0:
			p = "userland l_stack_t *";
			break;
		case 1:
			p = "userland l_stack_t *";
			break;
		default:
			break;
		};
		break;
	/* linux_utime */
	case 132:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "userland struct l_utimbuf *";
			break;
		default:
			break;
		};
		break;
	/* linux_mknod */
	case 133:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_mode_t";
			break;
		case 2:
			p = "l_dev_t";
			break;
		default:
			break;
		};
		break;
	/* linux_personality */
	case 135:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_ustat */
	case 136:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "userland struct l_ustat *";
			break;
		default:
			break;
		};
		break;
	/* linux_statfs */
	case 137:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "userland struct l_statfs_buf *";
			break;
		default:
			break;
		};
		break;
	/* linux_fstatfs */
	case 138:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "userland struct l_statfs_buf *";
			break;
		default:
			break;
		};
		break;
	/* linux_sysfs */
	case 139:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_ulong";
			break;
		case 2:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_getpriority */
	case 140:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* setpriority */
	case 141:
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
	/* linux_sched_setparam */
	case 142:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "userland struct sched_param *";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_getparam */
	case 143:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "userland struct sched_param *";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_setscheduler */
	case 144:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "userland struct sched_param *";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_getscheduler */
	case 145:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_get_priority_max */
	case 146:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_get_priority_min */
	case 147:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_rr_get_interval */
	case 148:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "userland struct l_timespec *";
			break;
		default:
			break;
		};
		break;
	/* mlock */
	case 149:
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
	case 150:
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
	/* mlockall */
	case 151:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* munlockall */
	case 152:
		break;
	/* linux_vhangup */
	case 153:
		break;
	/* linux_modify_ldt */
	case 154:
		break;
	/* linux_pivot_root */
	case 155:
		break;
	/* linux_sysctl */
	case 156:
		switch (ndx) {
		case 0:
			p = "userland struct l___sysctl_args *";
			break;
		default:
			break;
		};
		break;
	/* linux_prctl */
	case 157:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_uintptr_t";
			break;
		case 2:
			p = "l_uintptr_t";
			break;
		case 3:
			p = "l_uintptr_t";
			break;
		case 4:
			p = "l_uintptr_t";
			break;
		default:
			break;
		};
		break;
	/* linux_arch_prctl */
	case 158:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_adjtimex */
	case 159:
		break;
	/* linux_setrlimit */
	case 160:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "userland struct l_rlimit *";
			break;
		default:
			break;
		};
		break;
	/* chroot */
	case 161:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* sync */
	case 162:
		break;
	/* acct */
	case 163:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* settimeofday */
	case 164:
		switch (ndx) {
		case 0:
			p = "userland struct l_timeval *";
			break;
		case 1:
			p = "userland struct timezone *";
			break;
		default:
			break;
		};
		break;
	/* linux_mount */
	case 165:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "userland char *";
			break;
		case 3:
			p = "l_ulong";
			break;
		case 4:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* linux_umount */
	case 166:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* swapon */
	case 167:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* linux_swapoff */
	case 168:
		break;
	/* linux_reboot */
	case 169:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_uint";
			break;
		case 3:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* linux_sethostname */
	case 170:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_setdomainname */
	case 171:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_iopl */
	case 172:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_ioperm */
	case 173:
		break;
	/* linux_init_module */
	case 175:
		break;
	/* linux_delete_module */
	case 176:
		break;
	/* linux_quotactl */
	case 179:
		break;
	/* linux_gettid */
	case 186:
		break;
	/* linux_readahead */
	case 187:
		break;
	/* linux_setxattr */
	case 188:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "l_size_t";
			break;
		case 4:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_lsetxattr */
	case 189:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "l_size_t";
			break;
		case 4:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_fsetxattr */
	case 190:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "l_size_t";
			break;
		case 4:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_getxattr */
	case 191:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_lgetxattr */
	case 192:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_fgetxattr */
	case 193:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland void *";
			break;
		case 3:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_listxattr */
	case 194:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_llistxattr */
	case 195:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_flistxattr */
	case 196:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_removexattr */
	case 197:
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
	/* linux_lremovexattr */
	case 198:
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
	/* linux_fremovexattr */
	case 199:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* linux_tkill */
	case 200:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_time */
	case 201:
		switch (ndx) {
		case 0:
			p = "userland l_time_t *";
			break;
		default:
			break;
		};
		break;
	/* linux_sys_futex */
	case 202:
		switch (ndx) {
		case 0:
			p = "userland uint32_t *";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "uint32_t";
			break;
		case 3:
			p = "userland struct l_timespec *";
			break;
		case 4:
			p = "userland uint32_t *";
			break;
		case 5:
			p = "uint32_t";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_setaffinity */
	case 203:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "l_uint";
			break;
		case 2:
			p = "userland l_ulong *";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_getaffinity */
	case 204:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "l_uint";
			break;
		case 2:
			p = "userland l_ulong *";
			break;
		default:
			break;
		};
		break;
	/* linux_io_setup */
	case 206:
		break;
	/* linux_io_destroy */
	case 207:
		break;
	/* linux_io_getevents */
	case 208:
		break;
	/* linux_io_submit */
	case 209:
		break;
	/* linux_io_cancel */
	case 210:
		break;
	/* linux_lookup_dcookie */
	case 212:
		break;
	/* linux_epoll_create */
	case 213:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_remap_file_pages */
	case 216:
		break;
	/* linux_getdents64 */
	case 217:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_set_tid_address */
	case 218:
		switch (ndx) {
		case 0:
			p = "userland l_int *";
			break;
		default:
			break;
		};
		break;
	/* linux_restart_syscall */
	case 219:
		break;
	/* linux_semtimedop */
	case 220:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct sembuf *";
			break;
		case 2:
			p = "l_size_t";
			break;
		case 3:
			p = "userland struct l_timespec *";
			break;
		default:
			break;
		};
		break;
	/* linux_fadvise64 */
	case 221:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_loff_t";
			break;
		case 2:
			p = "l_size_t";
			break;
		case 3:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_timer_create */
	case 222:
		switch (ndx) {
		case 0:
			p = "clockid_t";
			break;
		case 1:
			p = "userland struct l_sigevent *";
			break;
		case 2:
			p = "userland l_timer_t *";
			break;
		default:
			break;
		};
		break;
	/* linux_timer_settime */
	case 223:
		switch (ndx) {
		case 0:
			p = "l_timer_t";
			break;
		case 1:
			p = "l_int";
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
	/* linux_timer_gettime */
	case 224:
		switch (ndx) {
		case 0:
			p = "l_timer_t";
			break;
		case 1:
			p = "userland struct itimerspec *";
			break;
		default:
			break;
		};
		break;
	/* linux_timer_getoverrun */
	case 225:
		switch (ndx) {
		case 0:
			p = "l_timer_t";
			break;
		default:
			break;
		};
		break;
	/* linux_timer_delete */
	case 226:
		switch (ndx) {
		case 0:
			p = "l_timer_t";
			break;
		default:
			break;
		};
		break;
	/* linux_clock_settime */
	case 227:
		switch (ndx) {
		case 0:
			p = "clockid_t";
			break;
		case 1:
			p = "userland struct l_timespec *";
			break;
		default:
			break;
		};
		break;
	/* linux_clock_gettime */
	case 228:
		switch (ndx) {
		case 0:
			p = "clockid_t";
			break;
		case 1:
			p = "userland struct l_timespec *";
			break;
		default:
			break;
		};
		break;
	/* linux_clock_getres */
	case 229:
		switch (ndx) {
		case 0:
			p = "clockid_t";
			break;
		case 1:
			p = "userland struct l_timespec *";
			break;
		default:
			break;
		};
		break;
	/* linux_clock_nanosleep */
	case 230:
		switch (ndx) {
		case 0:
			p = "clockid_t";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "userland struct l_timespec *";
			break;
		case 3:
			p = "userland struct l_timespec *";
			break;
		default:
			break;
		};
		break;
	/* linux_exit_group */
	case 231:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_epoll_wait */
	case 232:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct epoll_event *";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_epoll_ctl */
	case 233:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "userland struct epoll_event *";
			break;
		default:
			break;
		};
		break;
	/* linux_tgkill */
	case 234:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "l_pid_t";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_utimes */
	case 235:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "userland struct l_timeval *";
			break;
		default:
			break;
		};
		break;
	/* linux_mbind */
	case 237:
		break;
	/* linux_set_mempolicy */
	case 238:
		break;
	/* linux_get_mempolicy */
	case 239:
		break;
	/* linux_mq_open */
	case 240:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_mode_t";
			break;
		case 3:
			p = "userland struct mq_attr *";
			break;
		default:
			break;
		};
		break;
	/* linux_mq_unlink */
	case 241:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* linux_mq_timedsend */
	case 242:
		switch (ndx) {
		case 0:
			p = "l_mqd_t";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_size_t";
			break;
		case 3:
			p = "l_uint";
			break;
		case 4:
			p = "userland const struct l_timespec *";
			break;
		default:
			break;
		};
		break;
	/* linux_mq_timedreceive */
	case 243:
		switch (ndx) {
		case 0:
			p = "l_mqd_t";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "l_size_t";
			break;
		case 3:
			p = "userland l_uint *";
			break;
		case 4:
			p = "userland const struct l_timespec *";
			break;
		default:
			break;
		};
		break;
	/* linux_mq_notify */
	case 244:
		switch (ndx) {
		case 0:
			p = "l_mqd_t";
			break;
		case 1:
			p = "userland const struct l_sigevent *";
			break;
		default:
			break;
		};
		break;
	/* linux_mq_getsetattr */
	case 245:
		switch (ndx) {
		case 0:
			p = "l_mqd_t";
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
	/* linux_kexec_load */
	case 246:
		break;
	/* linux_waitid */
	case 247:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_pid_t";
			break;
		case 2:
			p = "userland l_siginfo_t *";
			break;
		case 3:
			p = "l_int";
			break;
		case 4:
			p = "userland struct rusage *";
			break;
		default:
			break;
		};
		break;
	/* linux_add_key */
	case 248:
		break;
	/* linux_request_key */
	case 249:
		break;
	/* linux_keyctl */
	case 250:
		break;
	/* linux_ioprio_set */
	case 251:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_ioprio_get */
	case 252:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_inotify_init */
	case 253:
		break;
	/* linux_inotify_add_watch */
	case 254:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "uint32_t";
			break;
		default:
			break;
		};
		break;
	/* linux_inotify_rm_watch */
	case 255:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "uint32_t";
			break;
		default:
			break;
		};
		break;
	/* linux_migrate_pages */
	case 256:
		break;
	/* linux_openat */
	case 257:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "l_mode_t";
			break;
		default:
			break;
		};
		break;
	/* linux_mkdirat */
	case 258:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_mode_t";
			break;
		default:
			break;
		};
		break;
	/* linux_mknodat */
	case 259:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_mode_t";
			break;
		case 3:
			p = "l_dev_t";
			break;
		default:
			break;
		};
		break;
	/* linux_fchownat */
	case 260:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_uid_t";
			break;
		case 3:
			p = "l_gid_t";
			break;
		case 4:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_futimesat */
	case 261:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "userland struct l_timeval *";
			break;
		default:
			break;
		};
		break;
	/* linux_newfstatat */
	case 262:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland char *";
			break;
		case 2:
			p = "userland struct l_stat64 *";
			break;
		case 3:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_unlinkat */
	case 263:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_renameat */
	case 264:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* linux_linkat */
	case 265:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "userland const char *";
			break;
		case 4:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_symlinkat */
	case 266:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* linux_readlinkat */
	case 267:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland char *";
			break;
		case 3:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_fchmodat */
	case 268:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_mode_t";
			break;
		default:
			break;
		};
		break;
	/* linux_faccessat */
	case 269:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_pselect6 */
	case 270:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland l_fd_set *";
			break;
		case 2:
			p = "userland l_fd_set *";
			break;
		case 3:
			p = "userland l_fd_set *";
			break;
		case 4:
			p = "userland struct l_timespec *";
			break;
		case 5:
			p = "userland l_uintptr_t *";
			break;
		default:
			break;
		};
		break;
	/* linux_ppoll */
	case 271:
		switch (ndx) {
		case 0:
			p = "userland struct pollfd *";
			break;
		case 1:
			p = "l_uint";
			break;
		case 2:
			p = "userland struct l_timespec *";
			break;
		case 3:
			p = "userland l_sigset_t *";
			break;
		case 4:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_unshare */
	case 272:
		break;
	/* linux_set_robust_list */
	case 273:
		switch (ndx) {
		case 0:
			p = "userland struct linux_robust_list_head *";
			break;
		case 1:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_get_robust_list */
	case 274:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct linux_robust_list_head **";
			break;
		case 2:
			p = "userland l_size_t *";
			break;
		default:
			break;
		};
		break;
	/* linux_splice */
	case 275:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		case 1:
			p = "userland l_loff_t *";
			break;
		case 2:
			p = "int";
			break;
		case 3:
			p = "userland l_loff_t *";
			break;
		case 4:
			p = "l_size_t";
			break;
		case 5:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_tee */
	case 276:
		break;
	/* linux_sync_file_range */
	case 277:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_loff_t";
			break;
		case 2:
			p = "l_loff_t";
			break;
		case 3:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_vmsplice */
	case 278:
		break;
	/* linux_move_pages */
	case 279:
		break;
	/* linux_utimensat */
	case 280:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland const struct l_timespec *";
			break;
		case 3:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_epoll_pwait */
	case 281:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct epoll_event *";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "l_int";
			break;
		case 4:
			p = "userland l_sigset_t *";
			break;
		case 5:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_signalfd */
	case 282:
		break;
	/* linux_timerfd_create */
	case 283:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_eventfd */
	case 284:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_fallocate */
	case 285:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_loff_t";
			break;
		case 3:
			p = "l_loff_t";
			break;
		default:
			break;
		};
		break;
	/* linux_timerfd_settime */
	case 286:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "userland const struct l_itimerspec *";
			break;
		case 3:
			p = "userland struct l_itimerspec *";
			break;
		default:
			break;
		};
		break;
	/* linux_timerfd_gettime */
	case 287:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct l_itimerspec *";
			break;
		default:
			break;
		};
		break;
	/* linux_accept4 */
	case 288:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_uintptr_t";
			break;
		case 2:
			p = "l_uintptr_t";
			break;
		case 3:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_signalfd4 */
	case 289:
		break;
	/* linux_eventfd2 */
	case 290:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_epoll_create1 */
	case 291:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_dup3 */
	case 292:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "l_uint";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_pipe2 */
	case 293:
		switch (ndx) {
		case 0:
			p = "userland l_int *";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_inotify_init1 */
	case 294:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_preadv */
	case 295:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "userland struct iovec *";
			break;
		case 2:
			p = "l_ulong";
			break;
		case 3:
			p = "l_ulong";
			break;
		case 4:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_pwritev */
	case 296:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "userland struct iovec *";
			break;
		case 2:
			p = "l_ulong";
			break;
		case 3:
			p = "l_ulong";
			break;
		case 4:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_rt_tgsigqueueinfo */
	case 297:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "l_pid_t";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "userland l_siginfo_t *";
			break;
		default:
			break;
		};
		break;
	/* linux_perf_event_open */
	case 298:
		break;
	/* linux_recvmmsg */
	case 299:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct l_mmsghdr *";
			break;
		case 2:
			p = "l_uint";
			break;
		case 3:
			p = "l_uint";
			break;
		case 4:
			p = "userland struct l_timespec *";
			break;
		default:
			break;
		};
		break;
	/* linux_fanotify_init */
	case 300:
		break;
	/* linux_fanotify_mark */
	case 301:
		break;
	/* linux_prlimit64 */
	case 302:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "l_uint";
			break;
		case 2:
			p = "userland struct rlimit *";
			break;
		case 3:
			p = "userland struct rlimit *";
			break;
		default:
			break;
		};
		break;
	/* linux_name_to_handle_at */
	case 303:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland struct l_file_handle *";
			break;
		case 3:
			p = "userland l_int *";
			break;
		case 4:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_open_by_handle_at */
	case 304:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct l_file_handle *";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_clock_adjtime */
	case 305:
		break;
	/* linux_syncfs */
	case 306:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_sendmmsg */
	case 307:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct l_mmsghdr *";
			break;
		case 2:
			p = "l_uint";
			break;
		case 3:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_setns */
	case 308:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_getcpu */
	case 309:
		switch (ndx) {
		case 0:
			p = "userland l_uint *";
			break;
		case 1:
			p = "userland l_uint *";
			break;
		case 2:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* linux_process_vm_readv */
	case 310:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "userland const struct iovec *";
			break;
		case 2:
			p = "l_ulong";
			break;
		case 3:
			p = "userland const struct iovec *";
			break;
		case 4:
			p = "l_ulong";
			break;
		case 5:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_process_vm_writev */
	case 311:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "userland const struct iovec *";
			break;
		case 2:
			p = "l_ulong";
			break;
		case 3:
			p = "userland const struct iovec *";
			break;
		case 4:
			p = "l_ulong";
			break;
		case 5:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_kcmp */
	case 312:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "l_pid_t";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "l_ulong";
			break;
		case 4:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_finit_module */
	case 313:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_setattr */
	case 314:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_getattr */
	case 315:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "l_uint";
			break;
		case 3:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_renameat2 */
	case 316:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "userland const char *";
			break;
		case 4:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_seccomp */
	case 317:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "l_uint";
			break;
		case 2:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* linux_getrandom */
	case 318:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_size_t";
			break;
		case 2:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_memfd_create */
	case 319:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_kexec_file_load */
	case 320:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "l_ulong";
			break;
		case 3:
			p = "userland const char *";
			break;
		case 4:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_bpf */
	case 321:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_execveat */
	case 322:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "userland const char **";
			break;
		case 3:
			p = "userland const char **";
			break;
		case 4:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_userfaultfd */
	case 323:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_membarrier */
	case 324:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_mlock2 */
	case 325:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "l_size_t";
			break;
		case 2:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_copy_file_range */
	case 326:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland l_loff_t *";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "userland l_loff_t *";
			break;
		case 4:
			p = "l_size_t";
			break;
		case 5:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_preadv2 */
	case 327:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "userland const struct iovec *";
			break;
		case 2:
			p = "l_ulong";
			break;
		case 3:
			p = "l_ulong";
			break;
		case 4:
			p = "l_ulong";
			break;
		case 5:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_pwritev2 */
	case 328:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "userland const struct iovec *";
			break;
		case 2:
			p = "l_ulong";
			break;
		case 3:
			p = "l_ulong";
			break;
		case 4:
			p = "l_ulong";
			break;
		case 5:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_pkey_mprotect */
	case 329:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "l_size_t";
			break;
		case 2:
			p = "l_ulong";
			break;
		case 3:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_pkey_alloc */
	case 330:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		case 1:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* linux_pkey_free */
	case 331:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_statx */
	case 332:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_uint";
			break;
		case 3:
			p = "l_uint";
			break;
		case 4:
			p = "userland void *";
			break;
		default:
			break;
		};
		break;
	/* linux_io_pgetevents */
	case 333:
		break;
	/* linux_rseq */
	case 334:
		switch (ndx) {
		case 0:
			p = "userland struct linux_rseq *";
			break;
		case 1:
			p = "uint32_t";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "uint32_t";
			break;
		default:
			break;
		};
		break;
	/* linux_pidfd_send_signal */
	case 424:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_int";
			break;
		case 2:
			p = "userland l_siginfo_t *";
			break;
		case 3:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_io_uring_setup */
	case 425:
		break;
	/* linux_io_uring_enter */
	case 426:
		break;
	/* linux_io_uring_register */
	case 427:
		break;
	/* linux_open_tree */
	case 428:
		break;
	/* linux_move_mount */
	case 429:
		break;
	/* linux_fsopen */
	case 430:
		break;
	/* linux_fsconfig */
	case 431:
		break;
	/* linux_fsmount */
	case 432:
		break;
	/* linux_fspick */
	case 433:
		break;
	/* linux_pidfd_open */
	case 434:
		break;
	/* linux_clone3 */
	case 435:
		switch (ndx) {
		case 0:
			p = "userland struct l_user_clone_args *";
			break;
		case 1:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_close_range */
	case 436:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		case 1:
			p = "l_uint";
			break;
		case 2:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_openat2 */
	case 437:
		break;
	/* linux_pidfd_getfd */
	case 438:
		break;
	/* linux_faccessat2 */
	case 439:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_process_madvise */
	case 440:
		break;
	/* linux_epoll_pwait2 */
	case 441:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "userland struct epoll_event *";
			break;
		case 2:
			p = "l_int";
			break;
		case 3:
			p = "userland struct l_timespec *";
			break;
		case 4:
			p = "userland l_sigset_t *";
			break;
		case 5:
			p = "l_size_t";
			break;
		default:
			break;
		};
		break;
	/* linux_mount_setattr */
	case 442:
		break;
	/* linux_quotactl_fd */
	case 443:
		break;
	/* linux_landlock_create_ruleset */
	case 444:
		break;
	/* linux_landlock_add_rule */
	case 445:
		break;
	/* linux_landlock_restrict_self */
	case 446:
		break;
	/* linux_memfd_secret */
	case 447:
		break;
	/* linux_process_mrelease */
	case 448:
		break;
	/* linux_futex_waitv */
	case 449:
		break;
	/* linux_set_mempolicy_home_node */
	case 450:
		break;
	/* linux_cachestat */
	case 451:
		break;
	/* linux_fchmodat2 */
	case 452:
		break;
	/* linux_map_shadow_stack */
	case 453:
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
	/* read */
	case 0:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_write */
	case 1:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_open */
	case 2:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* close */
	case 3:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_newstat */
	case 4:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_newfstat */
	case 5:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_newlstat */
	case 6:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_poll */
	case 7:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_lseek */
	case 8:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mmap2 */
	case 9:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mprotect */
	case 10:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* munmap */
	case 11:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_brk */
	case 12:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigaction */
	case 13:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigprocmask */
	case 14:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigreturn */
	case 15:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ioctl */
	case 16:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pread */
	case 17:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pwrite */
	case 18:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* readv */
	case 19:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_writev */
	case 20:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_access */
	case 21:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pipe */
	case 22:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_select */
	case 23:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sched_yield */
	case 24:
	/* linux_mremap */
	case 25:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_msync */
	case 26:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mincore */
	case 27:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_madvise */
	case 28:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_shmget */
	case 29:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_shmat */
	case 30:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_shmctl */
	case 31:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* dup */
	case 32:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* dup2 */
	case 33:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pause */
	case 34:
	/* linux_nanosleep */
	case 35:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getitimer */
	case 36:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_alarm */
	case 37:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setitimer */
	case 38:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getpid */
	case 39:
	/* linux_sendfile */
	case 40:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_socket */
	case 41:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_connect */
	case 42:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_accept */
	case 43:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sendto */
	case 44:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_recvfrom */
	case 45:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sendmsg */
	case 46:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_recvmsg */
	case 47:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_shutdown */
	case 48:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_bind */
	case 49:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_listen */
	case 50:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getsockname */
	case 51:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getpeername */
	case 52:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_socketpair */
	case 53:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setsockopt */
	case 54:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getsockopt */
	case 55:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_clone */
	case 56:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fork */
	case 57:
	/* linux_vfork */
	case 58:
	/* linux_execve */
	case 59:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_exit */
	case 60:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* linux_wait4 */
	case 61:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_kill */
	case 62:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_newuname */
	case 63:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_semget */
	case 64:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* semop */
	case 65:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_semctl */
	case 66:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_shmdt */
	case 67:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_msgget */
	case 68:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_msgsnd */
	case 69:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_msgrcv */
	case 70:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_msgctl */
	case 71:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fcntl */
	case 72:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* flock */
	case 73:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fsync */
	case 74:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fdatasync */
	case 75:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_truncate */
	case 76:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ftruncate */
	case 77:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getdents */
	case 78:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getcwd */
	case 79:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_chdir */
	case 80:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchdir */
	case 81:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rename */
	case 82:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mkdir */
	case 83:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rmdir */
	case 84:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_creat */
	case 85:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_link */
	case 86:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_unlink */
	case 87:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_symlink */
	case 88:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_readlink */
	case 89:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_chmod */
	case 90:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchmod */
	case 91:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_chown */
	case 92:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchown */
	case 93:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_lchown */
	case 94:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* umask */
	case 95:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* gettimeofday */
	case 96:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getrlimit */
	case 97:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getrusage */
	case 98:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sysinfo */
	case 99:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_times */
	case 100:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ptrace */
	case 101:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getuid */
	case 102:
	/* linux_syslog */
	case 103:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getgid */
	case 104:
	/* setuid */
	case 105:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setgid */
	case 106:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* geteuid */
	case 107:
	/* getegid */
	case 108:
	/* setpgid */
	case 109:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getppid */
	case 110:
	/* getpgrp */
	case 111:
	/* setsid */
	case 112:
	/* setreuid */
	case 113:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setregid */
	case 114:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getgroups */
	case 115:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setgroups */
	case 116:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setresuid */
	case 117:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getresuid */
	case 118:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setresgid */
	case 119:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getresgid */
	case 120:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getpgid */
	case 121:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setfsuid */
	case 122:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setfsgid */
	case 123:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getsid */
	case 124:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_capget */
	case 125:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_capset */
	case 126:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigpending */
	case 127:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigtimedwait */
	case 128:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigqueueinfo */
	case 129:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigsuspend */
	case 130:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sigaltstack */
	case 131:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_utime */
	case 132:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mknod */
	case 133:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_personality */
	case 135:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ustat */
	case 136:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_statfs */
	case 137:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fstatfs */
	case 138:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sysfs */
	case 139:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getpriority */
	case 140:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setpriority */
	case 141:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_setparam */
	case 142:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_getparam */
	case 143:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_setscheduler */
	case 144:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_getscheduler */
	case 145:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_get_priority_max */
	case 146:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_get_priority_min */
	case 147:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_rr_get_interval */
	case 148:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mlock */
	case 149:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* munlock */
	case 150:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mlockall */
	case 151:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* munlockall */
	case 152:
	/* linux_vhangup */
	case 153:
	/* linux_modify_ldt */
	case 154:
	/* linux_pivot_root */
	case 155:
	/* linux_sysctl */
	case 156:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_prctl */
	case 157:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_arch_prctl */
	case 158:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_adjtimex */
	case 159:
	/* linux_setrlimit */
	case 160:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* chroot */
	case 161:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sync */
	case 162:
	/* acct */
	case 163:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* settimeofday */
	case 164:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mount */
	case 165:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_umount */
	case 166:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* swapon */
	case 167:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_swapoff */
	case 168:
	/* linux_reboot */
	case 169:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sethostname */
	case 170:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setdomainname */
	case 171:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_iopl */
	case 172:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ioperm */
	case 173:
	/* linux_init_module */
	case 175:
	/* linux_delete_module */
	case 176:
	/* linux_quotactl */
	case 179:
	/* linux_gettid */
	case 186:
	/* linux_readahead */
	case 187:
	/* linux_setxattr */
	case 188:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_lsetxattr */
	case 189:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fsetxattr */
	case 190:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getxattr */
	case 191:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_lgetxattr */
	case 192:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fgetxattr */
	case 193:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_listxattr */
	case 194:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_llistxattr */
	case 195:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_flistxattr */
	case 196:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_removexattr */
	case 197:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_lremovexattr */
	case 198:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fremovexattr */
	case 199:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_tkill */
	case 200:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_time */
	case 201:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sys_futex */
	case 202:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_setaffinity */
	case 203:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_getaffinity */
	case 204:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_io_setup */
	case 206:
	/* linux_io_destroy */
	case 207:
	/* linux_io_getevents */
	case 208:
	/* linux_io_submit */
	case 209:
	/* linux_io_cancel */
	case 210:
	/* linux_lookup_dcookie */
	case 212:
	/* linux_epoll_create */
	case 213:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_remap_file_pages */
	case 216:
	/* linux_getdents64 */
	case 217:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_set_tid_address */
	case 218:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_restart_syscall */
	case 219:
	/* linux_semtimedop */
	case 220:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fadvise64 */
	case 221:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timer_create */
	case 222:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timer_settime */
	case 223:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timer_gettime */
	case 224:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timer_getoverrun */
	case 225:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timer_delete */
	case 226:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_clock_settime */
	case 227:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_clock_gettime */
	case 228:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_clock_getres */
	case 229:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_clock_nanosleep */
	case 230:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_exit_group */
	case 231:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_epoll_wait */
	case 232:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_epoll_ctl */
	case 233:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_tgkill */
	case 234:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_utimes */
	case 235:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mbind */
	case 237:
	/* linux_set_mempolicy */
	case 238:
	/* linux_get_mempolicy */
	case 239:
	/* linux_mq_open */
	case 240:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mq_unlink */
	case 241:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mq_timedsend */
	case 242:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mq_timedreceive */
	case 243:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mq_notify */
	case 244:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mq_getsetattr */
	case 245:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_kexec_load */
	case 246:
	/* linux_waitid */
	case 247:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_add_key */
	case 248:
	/* linux_request_key */
	case 249:
	/* linux_keyctl */
	case 250:
	/* linux_ioprio_set */
	case 251:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ioprio_get */
	case 252:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_inotify_init */
	case 253:
	/* linux_inotify_add_watch */
	case 254:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_inotify_rm_watch */
	case 255:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_migrate_pages */
	case 256:
	/* linux_openat */
	case 257:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mkdirat */
	case 258:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mknodat */
	case 259:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fchownat */
	case 260:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_futimesat */
	case 261:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_newfstatat */
	case 262:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_unlinkat */
	case 263:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_renameat */
	case 264:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_linkat */
	case 265:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_symlinkat */
	case 266:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_readlinkat */
	case 267:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fchmodat */
	case 268:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_faccessat */
	case 269:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pselect6 */
	case 270:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ppoll */
	case 271:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_unshare */
	case 272:
	/* linux_set_robust_list */
	case 273:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_get_robust_list */
	case 274:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_splice */
	case 275:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_tee */
	case 276:
	/* linux_sync_file_range */
	case 277:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_vmsplice */
	case 278:
	/* linux_move_pages */
	case 279:
	/* linux_utimensat */
	case 280:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_epoll_pwait */
	case 281:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_signalfd */
	case 282:
	/* linux_timerfd_create */
	case 283:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_eventfd */
	case 284:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fallocate */
	case 285:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timerfd_settime */
	case 286:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timerfd_gettime */
	case 287:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_accept4 */
	case 288:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_signalfd4 */
	case 289:
	/* linux_eventfd2 */
	case 290:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_epoll_create1 */
	case 291:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_dup3 */
	case 292:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pipe2 */
	case 293:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_inotify_init1 */
	case 294:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_preadv */
	case 295:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pwritev */
	case 296:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_tgsigqueueinfo */
	case 297:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_perf_event_open */
	case 298:
	/* linux_recvmmsg */
	case 299:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fanotify_init */
	case 300:
	/* linux_fanotify_mark */
	case 301:
	/* linux_prlimit64 */
	case 302:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_name_to_handle_at */
	case 303:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_open_by_handle_at */
	case 304:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_clock_adjtime */
	case 305:
	/* linux_syncfs */
	case 306:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sendmmsg */
	case 307:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setns */
	case 308:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getcpu */
	case 309:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_process_vm_readv */
	case 310:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_process_vm_writev */
	case 311:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_kcmp */
	case 312:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_finit_module */
	case 313:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_setattr */
	case 314:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_getattr */
	case 315:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_renameat2 */
	case 316:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_seccomp */
	case 317:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getrandom */
	case 318:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_memfd_create */
	case 319:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_kexec_file_load */
	case 320:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_bpf */
	case 321:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_execveat */
	case 322:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_userfaultfd */
	case 323:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_membarrier */
	case 324:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mlock2 */
	case 325:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_copy_file_range */
	case 326:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_preadv2 */
	case 327:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pwritev2 */
	case 328:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pkey_mprotect */
	case 329:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pkey_alloc */
	case 330:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pkey_free */
	case 331:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_statx */
	case 332:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_io_pgetevents */
	case 333:
	/* linux_rseq */
	case 334:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pidfd_send_signal */
	case 424:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_io_uring_setup */
	case 425:
	/* linux_io_uring_enter */
	case 426:
	/* linux_io_uring_register */
	case 427:
	/* linux_open_tree */
	case 428:
	/* linux_move_mount */
	case 429:
	/* linux_fsopen */
	case 430:
	/* linux_fsconfig */
	case 431:
	/* linux_fsmount */
	case 432:
	/* linux_fspick */
	case 433:
	/* linux_pidfd_open */
	case 434:
	/* linux_clone3 */
	case 435:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_close_range */
	case 436:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_openat2 */
	case 437:
	/* linux_pidfd_getfd */
	case 438:
	/* linux_faccessat2 */
	case 439:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_process_madvise */
	case 440:
	/* linux_epoll_pwait2 */
	case 441:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mount_setattr */
	case 442:
	/* linux_quotactl_fd */
	case 443:
	/* linux_landlock_create_ruleset */
	case 444:
	/* linux_landlock_add_rule */
	case 445:
	/* linux_landlock_restrict_self */
	case 446:
	/* linux_memfd_secret */
	case 447:
	/* linux_process_mrelease */
	case 448:
	/* linux_futex_waitv */
	case 449:
	/* linux_set_mempolicy_home_node */
	case 450:
	/* linux_cachestat */
	case 451:
	/* linux_fchmodat2 */
	case 452:
	/* linux_map_shadow_stack */
	case 453:
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
