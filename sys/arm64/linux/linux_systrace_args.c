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
	/* linux_setxattr */
	case 5: {
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
	case 6: {
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
	case 7: {
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
	case 8: {
		struct linux_getxattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = (intptr_t)p->value; /* void * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_lgetxattr */
	case 9: {
		struct linux_lgetxattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = (intptr_t)p->value; /* void * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_fgetxattr */
	case 10: {
		struct linux_fgetxattr_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		uarg[a++] = (intptr_t)p->value; /* void * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_listxattr */
	case 11: {
		struct linux_listxattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->list; /* char * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* linux_llistxattr */
	case 12: {
		struct linux_llistxattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->list; /* char * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* linux_flistxattr */
	case 13: {
		struct linux_flistxattr_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->list; /* char * */
		iarg[a++] = p->size; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* linux_removexattr */
	case 14: {
		struct linux_removexattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 2;
		break;
	}
	/* linux_lremovexattr */
	case 15: {
		struct linux_lremovexattr_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 2;
		break;
	}
	/* linux_fremovexattr */
	case 16: {
		struct linux_fremovexattr_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 2;
		break;
	}
	/* linux_getcwd */
	case 17: {
		struct linux_getcwd_args *p = params;
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->bufsize; /* l_ulong */
		*n_args = 2;
		break;
	}
	/* linux_lookup_dcookie */
	case 18: {
		*n_args = 0;
		break;
	}
	/* linux_eventfd2 */
	case 19: {
		struct linux_eventfd2_args *p = params;
		iarg[a++] = p->initval; /* l_uint */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_epoll_create1 */
	case 20: {
		struct linux_epoll_create1_args *p = params;
		iarg[a++] = p->flags; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_epoll_ctl */
	case 21: {
		struct linux_epoll_ctl_args *p = params;
		iarg[a++] = p->epfd; /* l_int */
		iarg[a++] = p->op; /* l_int */
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->event; /* struct epoll_event * */
		*n_args = 4;
		break;
	}
	/* linux_epoll_pwait */
	case 22: {
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
	/* dup */
	case 23: {
		struct dup_args *p = params;
		uarg[a++] = p->fd; /* u_int */
		*n_args = 1;
		break;
	}
	/* linux_dup3 */
	case 24: {
		struct linux_dup3_args *p = params;
		iarg[a++] = p->oldfd; /* l_int */
		iarg[a++] = p->newfd; /* l_int */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_fcntl */
	case 25: {
		struct linux_fcntl_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		iarg[a++] = p->cmd; /* l_uint */
		iarg[a++] = p->arg; /* l_ulong */
		*n_args = 3;
		break;
	}
	/* linux_inotify_init1 */
	case 26: {
		struct linux_inotify_init1_args *p = params;
		iarg[a++] = p->flags; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_inotify_add_watch */
	case 27: {
		struct linux_inotify_add_watch_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->pathname; /* const char * */
		uarg[a++] = p->mask; /* uint32_t */
		*n_args = 3;
		break;
	}
	/* linux_inotify_rm_watch */
	case 28: {
		struct linux_inotify_rm_watch_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = p->wd; /* uint32_t */
		*n_args = 2;
		break;
	}
	/* linux_ioctl */
	case 29: {
		struct linux_ioctl_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		iarg[a++] = p->cmd; /* l_uint */
		iarg[a++] = p->arg; /* l_ulong */
		*n_args = 3;
		break;
	}
	/* linux_ioprio_set */
	case 30: {
		struct linux_ioprio_set_args *p = params;
		iarg[a++] = p->which; /* l_int */
		iarg[a++] = p->who; /* l_int */
		iarg[a++] = p->ioprio; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_ioprio_get */
	case 31: {
		struct linux_ioprio_get_args *p = params;
		iarg[a++] = p->which; /* l_int */
		iarg[a++] = p->who; /* l_int */
		*n_args = 2;
		break;
	}
	/* flock */
	case 32: {
		struct flock_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->how; /* int */
		*n_args = 2;
		break;
	}
	/* linux_mknodat */
	case 33: {
		struct linux_mknodat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->mode; /* l_int */
		iarg[a++] = p->dev; /* l_dev_t */
		*n_args = 4;
		break;
	}
	/* linux_mkdirat */
	case 34: {
		struct linux_mkdirat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->pathname; /* const char * */
		iarg[a++] = p->mode; /* l_mode_t */
		*n_args = 3;
		break;
	}
	/* linux_unlinkat */
	case 35: {
		struct linux_unlinkat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->pathname; /* const char * */
		iarg[a++] = p->flag; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_symlinkat */
	case 36: {
		struct linux_symlinkat_args *p = params;
		uarg[a++] = (intptr_t)p->oldname; /* const char * */
		iarg[a++] = p->newdfd; /* l_int */
		uarg[a++] = (intptr_t)p->newname; /* const char * */
		*n_args = 3;
		break;
	}
	/* linux_linkat */
	case 37: {
		struct linux_linkat_args *p = params;
		iarg[a++] = p->olddfd; /* l_int */
		uarg[a++] = (intptr_t)p->oldname; /* const char * */
		iarg[a++] = p->newdfd; /* l_int */
		uarg[a++] = (intptr_t)p->newname; /* const char * */
		iarg[a++] = p->flag; /* l_int */
		*n_args = 5;
		break;
	}
	/* linux_renameat */
	case 38: {
		struct linux_renameat_args *p = params;
		iarg[a++] = p->olddfd; /* l_int */
		uarg[a++] = (intptr_t)p->oldname; /* const char * */
		iarg[a++] = p->newdfd; /* l_int */
		uarg[a++] = (intptr_t)p->newname; /* const char * */
		*n_args = 4;
		break;
	}
	/* linux_mount */
	case 40: {
		struct linux_mount_args *p = params;
		uarg[a++] = (intptr_t)p->specialfile; /* char * */
		uarg[a++] = (intptr_t)p->dir; /* char * */
		uarg[a++] = (intptr_t)p->filesystemtype; /* char * */
		iarg[a++] = p->rwflag; /* l_ulong */
		uarg[a++] = (intptr_t)p->data; /* void * */
		*n_args = 5;
		break;
	}
	/* linux_pivot_root */
	case 41: {
		*n_args = 0;
		break;
	}
	/* linux_statfs */
	case 43: {
		struct linux_statfs_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		uarg[a++] = (intptr_t)p->buf; /* struct l_statfs_buf * */
		*n_args = 2;
		break;
	}
	/* linux_fstatfs */
	case 44: {
		struct linux_fstatfs_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		uarg[a++] = (intptr_t)p->buf; /* struct l_statfs_buf * */
		*n_args = 2;
		break;
	}
	/* linux_truncate */
	case 45: {
		struct linux_truncate_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		iarg[a++] = p->length; /* l_ulong */
		*n_args = 2;
		break;
	}
	/* linux_ftruncate */
	case 46: {
		struct linux_ftruncate_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->length; /* l_long */
		*n_args = 2;
		break;
	}
	/* linux_fallocate */
	case 47: {
		struct linux_fallocate_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->mode; /* l_int */
		iarg[a++] = p->offset; /* l_loff_t */
		iarg[a++] = p->len; /* l_loff_t */
		*n_args = 4;
		break;
	}
	/* linux_faccessat */
	case 48: {
		struct linux_faccessat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->amode; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_chdir */
	case 49: {
		struct linux_chdir_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		*n_args = 1;
		break;
	}
	/* fchdir */
	case 50: {
		struct fchdir_args *p = params;
		iarg[a++] = p->fd; /* int */
		*n_args = 1;
		break;
	}
	/* chroot */
	case 51: {
		struct chroot_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		*n_args = 1;
		break;
	}
	/* fchmod */
	case 52: {
		struct fchmod_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->mode; /* int */
		*n_args = 2;
		break;
	}
	/* linux_fchmodat */
	case 53: {
		struct linux_fchmodat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->mode; /* l_mode_t */
		*n_args = 3;
		break;
	}
	/* linux_fchownat */
	case 54: {
		struct linux_fchownat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->uid; /* l_uid_t */
		iarg[a++] = p->gid; /* l_gid_t */
		iarg[a++] = p->flag; /* l_int */
		*n_args = 5;
		break;
	}
	/* fchown */
	case 55: {
		struct fchown_args *p = params;
		iarg[a++] = p->fd; /* int */
		iarg[a++] = p->uid; /* int */
		iarg[a++] = p->gid; /* int */
		*n_args = 3;
		break;
	}
	/* linux_openat */
	case 56: {
		struct linux_openat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->filename; /* const char * */
		iarg[a++] = p->flags; /* l_int */
		iarg[a++] = p->mode; /* l_mode_t */
		*n_args = 4;
		break;
	}
	/* close */
	case 57: {
		struct close_args *p = params;
		iarg[a++] = p->fd; /* int */
		*n_args = 1;
		break;
	}
	/* linux_vhangup */
	case 58: {
		*n_args = 0;
		break;
	}
	/* linux_pipe2 */
	case 59: {
		struct linux_pipe2_args *p = params;
		uarg[a++] = (intptr_t)p->pipefds; /* l_int * */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_getdents64 */
	case 61: {
		struct linux_getdents64_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		uarg[a++] = (intptr_t)p->dirent; /* void * */
		iarg[a++] = p->count; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_lseek */
	case 62: {
		struct linux_lseek_args *p = params;
		iarg[a++] = p->fdes; /* l_uint */
		iarg[a++] = p->off; /* l_off_t */
		iarg[a++] = p->whence; /* l_int */
		*n_args = 3;
		break;
	}
	/* read */
	case 63: {
		struct read_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->nbyte; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* linux_write */
	case 64: {
		struct linux_write_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->nbyte; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* readv */
	case 65: {
		struct readv_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->iovp; /* struct iovec * */
		uarg[a++] = p->iovcnt; /* u_int */
		*n_args = 3;
		break;
	}
	/* linux_writev */
	case 66: {
		struct linux_writev_args *p = params;
		iarg[a++] = p->fd; /* int */
		uarg[a++] = (intptr_t)p->iovp; /* struct iovec * */
		uarg[a++] = p->iovcnt; /* u_int */
		*n_args = 3;
		break;
	}
	/* linux_pread */
	case 67: {
		struct linux_pread_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->nbyte; /* l_size_t */
		iarg[a++] = p->offset; /* l_loff_t */
		*n_args = 4;
		break;
	}
	/* linux_pwrite */
	case 68: {
		struct linux_pwrite_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->nbyte; /* l_size_t */
		iarg[a++] = p->offset; /* l_loff_t */
		*n_args = 4;
		break;
	}
	/* linux_preadv */
	case 69: {
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
	case 70: {
		struct linux_pwritev_args *p = params;
		iarg[a++] = p->fd; /* l_ulong */
		uarg[a++] = (intptr_t)p->vec; /* struct iovec * */
		iarg[a++] = p->vlen; /* l_ulong */
		iarg[a++] = p->pos_l; /* l_ulong */
		iarg[a++] = p->pos_h; /* l_ulong */
		*n_args = 5;
		break;
	}
	/* linux_sendfile */
	case 71: {
		struct linux_sendfile_args *p = params;
		iarg[a++] = p->out; /* l_int */
		iarg[a++] = p->in; /* l_int */
		uarg[a++] = (intptr_t)p->offset; /* l_off_t * */
		iarg[a++] = p->count; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_pselect6 */
	case 72: {
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
	case 73: {
		struct linux_ppoll_args *p = params;
		uarg[a++] = (intptr_t)p->fds; /* struct pollfd * */
		iarg[a++] = p->nfds; /* l_uint */
		uarg[a++] = (intptr_t)p->tsp; /* struct l_timespec * */
		uarg[a++] = (intptr_t)p->sset; /* l_sigset_t * */
		iarg[a++] = p->ssize; /* l_size_t */
		*n_args = 5;
		break;
	}
	/* linux_signalfd4 */
	case 74: {
		*n_args = 0;
		break;
	}
	/* linux_vmsplice */
	case 75: {
		*n_args = 0;
		break;
	}
	/* linux_splice */
	case 76: {
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
	case 77: {
		*n_args = 0;
		break;
	}
	/* linux_readlinkat */
	case 78: {
		struct linux_readlinkat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->path; /* const char * */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->bufsiz; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_newfstatat */
	case 79: {
		struct linux_newfstatat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->pathname; /* char * */
		uarg[a++] = (intptr_t)p->statbuf; /* struct l_stat64 * */
		iarg[a++] = p->flag; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_newfstat */
	case 80: {
		struct linux_newfstat_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		uarg[a++] = (intptr_t)p->buf; /* struct l_newstat * */
		*n_args = 2;
		break;
	}
	/* fsync */
	case 82: {
		struct fsync_args *p = params;
		iarg[a++] = p->fd; /* int */
		*n_args = 1;
		break;
	}
	/* linux_fdatasync */
	case 83: {
		struct linux_fdatasync_args *p = params;
		iarg[a++] = p->fd; /* l_uint */
		*n_args = 1;
		break;
	}
	/* linux_sync_file_range */
	case 84: {
		struct linux_sync_file_range_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->offset; /* l_loff_t */
		iarg[a++] = p->nbytes; /* l_loff_t */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 4;
		break;
	}
	/* linux_timerfd_create */
	case 85: {
		struct linux_timerfd_create_args *p = params;
		iarg[a++] = p->clockid; /* l_int */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_timerfd_settime */
	case 86: {
		struct linux_timerfd_settime_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->flags; /* l_int */
		uarg[a++] = (intptr_t)p->new_value; /* const struct l_itimerspec * */
		uarg[a++] = (intptr_t)p->old_value; /* struct l_itimerspec * */
		*n_args = 4;
		break;
	}
	/* linux_timerfd_gettime */
	case 87: {
		struct linux_timerfd_gettime_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->old_value; /* struct l_itimerspec * */
		*n_args = 2;
		break;
	}
	/* linux_utimensat */
	case 88: {
		struct linux_utimensat_args *p = params;
		iarg[a++] = p->dfd; /* l_int */
		uarg[a++] = (intptr_t)p->pathname; /* const char * */
		uarg[a++] = (intptr_t)p->times; /* const struct l_timespec * */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 4;
		break;
	}
	/* acct */
	case 89: {
		struct acct_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		*n_args = 1;
		break;
	}
	/* linux_capget */
	case 90: {
		struct linux_capget_args *p = params;
		uarg[a++] = (intptr_t)p->hdrp; /* struct l_user_cap_header * */
		uarg[a++] = (intptr_t)p->datap; /* struct l_user_cap_data * */
		*n_args = 2;
		break;
	}
	/* linux_capset */
	case 91: {
		struct linux_capset_args *p = params;
		uarg[a++] = (intptr_t)p->hdrp; /* struct l_user_cap_header * */
		uarg[a++] = (intptr_t)p->datap; /* struct l_user_cap_data * */
		*n_args = 2;
		break;
	}
	/* linux_personality */
	case 92: {
		struct linux_personality_args *p = params;
		iarg[a++] = p->per; /* l_uint */
		*n_args = 1;
		break;
	}
	/* linux_exit */
	case 93: {
		struct linux_exit_args *p = params;
		uarg[a++] = p->rval; /* u_int */
		*n_args = 1;
		break;
	}
	/* linux_exit_group */
	case 94: {
		struct linux_exit_group_args *p = params;
		iarg[a++] = p->error_code; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_waitid */
	case 95: {
		struct linux_waitid_args *p = params;
		iarg[a++] = p->idtype; /* l_int */
		iarg[a++] = p->id; /* l_pid_t */
		uarg[a++] = (intptr_t)p->info; /* l_siginfo_t * */
		iarg[a++] = p->options; /* l_int */
		uarg[a++] = (intptr_t)p->rusage; /* struct rusage * */
		*n_args = 5;
		break;
	}
	/* linux_set_tid_address */
	case 96: {
		struct linux_set_tid_address_args *p = params;
		uarg[a++] = (intptr_t)p->tidptr; /* l_int * */
		*n_args = 1;
		break;
	}
	/* linux_unshare */
	case 97: {
		*n_args = 0;
		break;
	}
	/* linux_sys_futex */
	case 98: {
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
	/* linux_set_robust_list */
	case 99: {
		struct linux_set_robust_list_args *p = params;
		uarg[a++] = (intptr_t)p->head; /* struct linux_robust_list_head * */
		iarg[a++] = p->len; /* l_size_t */
		*n_args = 2;
		break;
	}
	/* linux_get_robust_list */
	case 100: {
		struct linux_get_robust_list_args *p = params;
		iarg[a++] = p->pid; /* l_int */
		uarg[a++] = (intptr_t)p->head; /* struct linux_robust_list_head ** */
		uarg[a++] = (intptr_t)p->len; /* l_size_t * */
		*n_args = 3;
		break;
	}
	/* linux_nanosleep */
	case 101: {
		struct linux_nanosleep_args *p = params;
		uarg[a++] = (intptr_t)p->rqtp; /* const struct l_timespec * */
		uarg[a++] = (intptr_t)p->rmtp; /* struct l_timespec * */
		*n_args = 2;
		break;
	}
	/* linux_getitimer */
	case 102: {
		struct linux_getitimer_args *p = params;
		iarg[a++] = p->which; /* l_int */
		uarg[a++] = (intptr_t)p->itv; /* struct l_itimerval * */
		*n_args = 2;
		break;
	}
	/* linux_setitimer */
	case 103: {
		struct linux_setitimer_args *p = params;
		iarg[a++] = p->which; /* l_int */
		uarg[a++] = (intptr_t)p->itv; /* struct l_itimerval * */
		uarg[a++] = (intptr_t)p->oitv; /* struct l_itimerval * */
		*n_args = 3;
		break;
	}
	/* linux_kexec_load */
	case 104: {
		*n_args = 0;
		break;
	}
	/* linux_init_module */
	case 105: {
		*n_args = 0;
		break;
	}
	/* linux_delete_module */
	case 106: {
		*n_args = 0;
		break;
	}
	/* linux_timer_create */
	case 107: {
		struct linux_timer_create_args *p = params;
		iarg[a++] = p->clock_id; /* clockid_t */
		uarg[a++] = (intptr_t)p->evp; /* struct l_sigevent * */
		uarg[a++] = (intptr_t)p->timerid; /* l_timer_t * */
		*n_args = 3;
		break;
	}
	/* linux_timer_gettime */
	case 108: {
		struct linux_timer_gettime_args *p = params;
		iarg[a++] = p->timerid; /* l_timer_t */
		uarg[a++] = (intptr_t)p->setting; /* struct itimerspec * */
		*n_args = 2;
		break;
	}
	/* linux_timer_getoverrun */
	case 109: {
		struct linux_timer_getoverrun_args *p = params;
		iarg[a++] = p->timerid; /* l_timer_t */
		*n_args = 1;
		break;
	}
	/* linux_timer_settime */
	case 110: {
		struct linux_timer_settime_args *p = params;
		iarg[a++] = p->timerid; /* l_timer_t */
		iarg[a++] = p->flags; /* l_int */
		uarg[a++] = (intptr_t)p->new; /* const struct itimerspec * */
		uarg[a++] = (intptr_t)p->old; /* struct itimerspec * */
		*n_args = 4;
		break;
	}
	/* linux_timer_delete */
	case 111: {
		struct linux_timer_delete_args *p = params;
		iarg[a++] = p->timerid; /* l_timer_t */
		*n_args = 1;
		break;
	}
	/* linux_clock_settime */
	case 112: {
		struct linux_clock_settime_args *p = params;
		iarg[a++] = p->which; /* clockid_t */
		uarg[a++] = (intptr_t)p->tp; /* struct l_timespec * */
		*n_args = 2;
		break;
	}
	/* linux_clock_gettime */
	case 113: {
		struct linux_clock_gettime_args *p = params;
		iarg[a++] = p->which; /* clockid_t */
		uarg[a++] = (intptr_t)p->tp; /* struct l_timespec * */
		*n_args = 2;
		break;
	}
	/* linux_clock_getres */
	case 114: {
		struct linux_clock_getres_args *p = params;
		iarg[a++] = p->which; /* clockid_t */
		uarg[a++] = (intptr_t)p->tp; /* struct l_timespec * */
		*n_args = 2;
		break;
	}
	/* linux_clock_nanosleep */
	case 115: {
		struct linux_clock_nanosleep_args *p = params;
		iarg[a++] = p->which; /* clockid_t */
		iarg[a++] = p->flags; /* l_int */
		uarg[a++] = (intptr_t)p->rqtp; /* struct l_timespec * */
		uarg[a++] = (intptr_t)p->rmtp; /* struct l_timespec * */
		*n_args = 4;
		break;
	}
	/* linux_syslog */
	case 116: {
		struct linux_syslog_args *p = params;
		iarg[a++] = p->type; /* l_int */
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->len; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_ptrace */
	case 117: {
		struct linux_ptrace_args *p = params;
		iarg[a++] = p->req; /* l_long */
		iarg[a++] = p->pid; /* l_long */
		iarg[a++] = p->addr; /* l_ulong */
		iarg[a++] = p->data; /* l_ulong */
		*n_args = 4;
		break;
	}
	/* linux_sched_setparam */
	case 118: {
		struct linux_sched_setparam_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->param; /* struct sched_param * */
		*n_args = 2;
		break;
	}
	/* linux_sched_setscheduler */
	case 119: {
		struct linux_sched_setscheduler_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->policy; /* l_int */
		uarg[a++] = (intptr_t)p->param; /* struct sched_param * */
		*n_args = 3;
		break;
	}
	/* linux_sched_getscheduler */
	case 120: {
		struct linux_sched_getscheduler_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		*n_args = 1;
		break;
	}
	/* linux_sched_getparam */
	case 121: {
		struct linux_sched_getparam_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->param; /* struct sched_param * */
		*n_args = 2;
		break;
	}
	/* linux_sched_setaffinity */
	case 122: {
		struct linux_sched_setaffinity_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->len; /* l_uint */
		uarg[a++] = (intptr_t)p->user_mask_ptr; /* l_ulong * */
		*n_args = 3;
		break;
	}
	/* linux_sched_getaffinity */
	case 123: {
		struct linux_sched_getaffinity_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->len; /* l_uint */
		uarg[a++] = (intptr_t)p->user_mask_ptr; /* l_ulong * */
		*n_args = 3;
		break;
	}
	/* sched_yield */
	case 124: {
		*n_args = 0;
		break;
	}
	/* linux_sched_get_priority_max */
	case 125: {
		struct linux_sched_get_priority_max_args *p = params;
		iarg[a++] = p->policy; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_sched_get_priority_min */
	case 126: {
		struct linux_sched_get_priority_min_args *p = params;
		iarg[a++] = p->policy; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_sched_rr_get_interval */
	case 127: {
		struct linux_sched_rr_get_interval_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->interval; /* struct l_timespec * */
		*n_args = 2;
		break;
	}
	/* linux_kill */
	case 129: {
		struct linux_kill_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->signum; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_tkill */
	case 130: {
		struct linux_tkill_args *p = params;
		iarg[a++] = p->tid; /* l_pid_t */
		iarg[a++] = p->sig; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_tgkill */
	case 131: {
		struct linux_tgkill_args *p = params;
		iarg[a++] = p->tgid; /* l_pid_t */
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->sig; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_sigaltstack */
	case 132: {
		struct linux_sigaltstack_args *p = params;
		uarg[a++] = (intptr_t)p->uss; /* l_stack_t * */
		uarg[a++] = (intptr_t)p->uoss; /* l_stack_t * */
		*n_args = 2;
		break;
	}
	/* linux_rt_sigsuspend */
	case 133: {
		struct linux_rt_sigsuspend_args *p = params;
		uarg[a++] = (intptr_t)p->newset; /* l_sigset_t * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 2;
		break;
	}
	/* linux_rt_sigaction */
	case 134: {
		struct linux_rt_sigaction_args *p = params;
		iarg[a++] = p->sig; /* l_int */
		uarg[a++] = (intptr_t)p->act; /* l_sigaction_t * */
		uarg[a++] = (intptr_t)p->oact; /* l_sigaction_t * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_rt_sigprocmask */
	case 135: {
		struct linux_rt_sigprocmask_args *p = params;
		iarg[a++] = p->how; /* l_int */
		uarg[a++] = (intptr_t)p->mask; /* l_sigset_t * */
		uarg[a++] = (intptr_t)p->omask; /* l_sigset_t * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_rt_sigpending */
	case 136: {
		struct linux_rt_sigpending_args *p = params;
		uarg[a++] = (intptr_t)p->set; /* l_sigset_t * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 2;
		break;
	}
	/* linux_rt_sigtimedwait */
	case 137: {
		struct linux_rt_sigtimedwait_args *p = params;
		uarg[a++] = (intptr_t)p->mask; /* l_sigset_t * */
		uarg[a++] = (intptr_t)p->ptr; /* l_siginfo_t * */
		uarg[a++] = (intptr_t)p->timeout; /* struct l_timespec * */
		iarg[a++] = p->sigsetsize; /* l_size_t */
		*n_args = 4;
		break;
	}
	/* linux_rt_sigqueueinfo */
	case 138: {
		struct linux_rt_sigqueueinfo_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->sig; /* l_int */
		uarg[a++] = (intptr_t)p->info; /* l_siginfo_t * */
		*n_args = 3;
		break;
	}
	/* linux_rt_sigreturn */
	case 139: {
		*n_args = 0;
		break;
	}
	/* setpriority */
	case 140: {
		struct setpriority_args *p = params;
		iarg[a++] = p->which; /* int */
		iarg[a++] = p->who; /* int */
		iarg[a++] = p->prio; /* int */
		*n_args = 3;
		break;
	}
	/* linux_getpriority */
	case 141: {
		struct linux_getpriority_args *p = params;
		iarg[a++] = p->which; /* l_int */
		iarg[a++] = p->who; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_reboot */
	case 142: {
		struct linux_reboot_args *p = params;
		iarg[a++] = p->magic1; /* l_int */
		iarg[a++] = p->magic2; /* l_int */
		iarg[a++] = p->cmd; /* l_uint */
		uarg[a++] = (intptr_t)p->arg; /* void * */
		*n_args = 4;
		break;
	}
	/* setregid */
	case 143: {
		struct setregid_args *p = params;
		iarg[a++] = p->rgid; /* gid_t */
		iarg[a++] = p->egid; /* gid_t */
		*n_args = 2;
		break;
	}
	/* setgid */
	case 144: {
		struct setgid_args *p = params;
		iarg[a++] = p->gid; /* gid_t */
		*n_args = 1;
		break;
	}
	/* setreuid */
	case 145: {
		struct setreuid_args *p = params;
		uarg[a++] = p->ruid; /* uid_t */
		uarg[a++] = p->euid; /* uid_t */
		*n_args = 2;
		break;
	}
	/* setuid */
	case 146: {
		struct setuid_args *p = params;
		uarg[a++] = p->uid; /* uid_t */
		*n_args = 1;
		break;
	}
	/* setresuid */
	case 147: {
		struct setresuid_args *p = params;
		uarg[a++] = p->ruid; /* uid_t */
		uarg[a++] = p->euid; /* uid_t */
		uarg[a++] = p->suid; /* uid_t */
		*n_args = 3;
		break;
	}
	/* getresuid */
	case 148: {
		struct getresuid_args *p = params;
		uarg[a++] = (intptr_t)p->ruid; /* uid_t * */
		uarg[a++] = (intptr_t)p->euid; /* uid_t * */
		uarg[a++] = (intptr_t)p->suid; /* uid_t * */
		*n_args = 3;
		break;
	}
	/* setresgid */
	case 149: {
		struct setresgid_args *p = params;
		iarg[a++] = p->rgid; /* gid_t */
		iarg[a++] = p->egid; /* gid_t */
		iarg[a++] = p->sgid; /* gid_t */
		*n_args = 3;
		break;
	}
	/* getresgid */
	case 150: {
		struct getresgid_args *p = params;
		uarg[a++] = (intptr_t)p->rgid; /* gid_t * */
		uarg[a++] = (intptr_t)p->egid; /* gid_t * */
		uarg[a++] = (intptr_t)p->sgid; /* gid_t * */
		*n_args = 3;
		break;
	}
	/* linux_setfsuid */
	case 151: {
		struct linux_setfsuid_args *p = params;
		iarg[a++] = p->uid; /* l_uid_t */
		*n_args = 1;
		break;
	}
	/* linux_setfsgid */
	case 152: {
		struct linux_setfsgid_args *p = params;
		iarg[a++] = p->gid; /* l_gid_t */
		*n_args = 1;
		break;
	}
	/* linux_times */
	case 153: {
		struct linux_times_args *p = params;
		uarg[a++] = (intptr_t)p->buf; /* struct l_times_argv * */
		*n_args = 1;
		break;
	}
	/* setpgid */
	case 154: {
		struct setpgid_args *p = params;
		iarg[a++] = p->pid; /* int */
		iarg[a++] = p->pgid; /* int */
		*n_args = 2;
		break;
	}
	/* getpgid */
	case 155: {
		struct getpgid_args *p = params;
		iarg[a++] = p->pid; /* int */
		*n_args = 1;
		break;
	}
	/* linux_getsid */
	case 156: {
		struct linux_getsid_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		*n_args = 1;
		break;
	}
	/* setsid */
	case 157: {
		*n_args = 0;
		break;
	}
	/* linux_getgroups */
	case 158: {
		struct linux_getgroups_args *p = params;
		iarg[a++] = p->gidsetsize; /* l_int */
		uarg[a++] = (intptr_t)p->grouplist; /* l_gid_t * */
		*n_args = 2;
		break;
	}
	/* linux_setgroups */
	case 159: {
		struct linux_setgroups_args *p = params;
		iarg[a++] = p->gidsetsize; /* l_int */
		uarg[a++] = (intptr_t)p->grouplist; /* l_gid_t * */
		*n_args = 2;
		break;
	}
	/* linux_newuname */
	case 160: {
		struct linux_newuname_args *p = params;
		uarg[a++] = (intptr_t)p->buf; /* struct l_new_utsname * */
		*n_args = 1;
		break;
	}
	/* linux_sethostname */
	case 161: {
		struct linux_sethostname_args *p = params;
		uarg[a++] = (intptr_t)p->hostname; /* char * */
		iarg[a++] = p->len; /* l_uint */
		*n_args = 2;
		break;
	}
	/* linux_setdomainname */
	case 162: {
		struct linux_setdomainname_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* char * */
		iarg[a++] = p->len; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_getrlimit */
	case 163: {
		struct linux_getrlimit_args *p = params;
		iarg[a++] = p->resource; /* l_uint */
		uarg[a++] = (intptr_t)p->rlim; /* struct l_rlimit * */
		*n_args = 2;
		break;
	}
	/* linux_setrlimit */
	case 164: {
		struct linux_setrlimit_args *p = params;
		iarg[a++] = p->resource; /* l_uint */
		uarg[a++] = (intptr_t)p->rlim; /* struct l_rlimit * */
		*n_args = 2;
		break;
	}
	/* getrusage */
	case 165: {
		struct getrusage_args *p = params;
		iarg[a++] = p->who; /* int */
		uarg[a++] = (intptr_t)p->rusage; /* struct rusage * */
		*n_args = 2;
		break;
	}
	/* umask */
	case 166: {
		struct umask_args *p = params;
		iarg[a++] = p->newmask; /* int */
		*n_args = 1;
		break;
	}
	/* linux_prctl */
	case 167: {
		struct linux_prctl_args *p = params;
		iarg[a++] = p->option; /* l_int */
		uarg[a++] = (intptr_t)p->arg2; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->arg3; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->arg4; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->arg5; /* l_uintptr_t */
		*n_args = 5;
		break;
	}
	/* linux_getcpu */
	case 168: {
		struct linux_getcpu_args *p = params;
		uarg[a++] = (intptr_t)p->cpu; /* l_uint * */
		uarg[a++] = (intptr_t)p->node; /* l_uint * */
		uarg[a++] = (intptr_t)p->cache; /* void * */
		*n_args = 3;
		break;
	}
	/* gettimeofday */
	case 169: {
		struct gettimeofday_args *p = params;
		uarg[a++] = (intptr_t)p->tp; /* struct l_timeval * */
		uarg[a++] = (intptr_t)p->tzp; /* struct timezone * */
		*n_args = 2;
		break;
	}
	/* settimeofday */
	case 170: {
		struct settimeofday_args *p = params;
		uarg[a++] = (intptr_t)p->tv; /* struct l_timeval * */
		uarg[a++] = (intptr_t)p->tzp; /* struct timezone * */
		*n_args = 2;
		break;
	}
	/* linux_adjtimex */
	case 171: {
		*n_args = 0;
		break;
	}
	/* linux_getpid */
	case 172: {
		*n_args = 0;
		break;
	}
	/* linux_getppid */
	case 173: {
		*n_args = 0;
		break;
	}
	/* linux_getuid */
	case 174: {
		*n_args = 0;
		break;
	}
	/* geteuid */
	case 175: {
		*n_args = 0;
		break;
	}
	/* linux_getgid */
	case 176: {
		*n_args = 0;
		break;
	}
	/* getegid */
	case 177: {
		*n_args = 0;
		break;
	}
	/* linux_gettid */
	case 178: {
		*n_args = 0;
		break;
	}
	/* linux_sysinfo */
	case 179: {
		struct linux_sysinfo_args *p = params;
		uarg[a++] = (intptr_t)p->info; /* struct l_sysinfo * */
		*n_args = 1;
		break;
	}
	/* linux_mq_open */
	case 180: {
		struct linux_mq_open_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* const char * */
		iarg[a++] = p->oflag; /* l_int */
		iarg[a++] = p->mode; /* l_mode_t */
		uarg[a++] = (intptr_t)p->attr; /* struct mq_attr * */
		*n_args = 4;
		break;
	}
	/* linux_mq_unlink */
	case 181: {
		struct linux_mq_unlink_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* const char * */
		*n_args = 1;
		break;
	}
	/* linux_mq_timedsend */
	case 182: {
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
	case 183: {
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
	case 184: {
		struct linux_mq_notify_args *p = params;
		iarg[a++] = p->mqd; /* l_mqd_t */
		uarg[a++] = (intptr_t)p->sevp; /* const struct l_sigevent * */
		*n_args = 2;
		break;
	}
	/* linux_mq_getsetattr */
	case 185: {
		struct linux_mq_getsetattr_args *p = params;
		iarg[a++] = p->mqd; /* l_mqd_t */
		uarg[a++] = (intptr_t)p->attr; /* const struct mq_attr * */
		uarg[a++] = (intptr_t)p->oattr; /* struct mq_attr * */
		*n_args = 3;
		break;
	}
	/* linux_msgget */
	case 186: {
		struct linux_msgget_args *p = params;
		iarg[a++] = p->key; /* l_key_t */
		iarg[a++] = p->msgflg; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_msgctl */
	case 187: {
		struct linux_msgctl_args *p = params;
		iarg[a++] = p->msqid; /* l_int */
		iarg[a++] = p->cmd; /* l_int */
		uarg[a++] = (intptr_t)p->buf; /* struct l_msqid_ds * */
		*n_args = 3;
		break;
	}
	/* linux_msgrcv */
	case 188: {
		struct linux_msgrcv_args *p = params;
		iarg[a++] = p->msqid; /* l_int */
		uarg[a++] = (intptr_t)p->msgp; /* struct l_msgbuf * */
		iarg[a++] = p->msgsz; /* l_size_t */
		iarg[a++] = p->msgtyp; /* l_long */
		iarg[a++] = p->msgflg; /* l_int */
		*n_args = 5;
		break;
	}
	/* linux_msgsnd */
	case 189: {
		struct linux_msgsnd_args *p = params;
		iarg[a++] = p->msqid; /* l_int */
		uarg[a++] = (intptr_t)p->msgp; /* struct l_msgbuf * */
		iarg[a++] = p->msgsz; /* l_size_t */
		iarg[a++] = p->msgflg; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_semget */
	case 190: {
		struct linux_semget_args *p = params;
		iarg[a++] = p->key; /* l_key_t */
		iarg[a++] = p->nsems; /* l_int */
		iarg[a++] = p->semflg; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_semctl */
	case 191: {
		struct linux_semctl_args *p = params;
		iarg[a++] = p->semid; /* l_int */
		iarg[a++] = p->semnum; /* l_int */
		iarg[a++] = p->cmd; /* l_int */
		uarg[a++] = p->arg.buf; /* union l_semun */
		*n_args = 4;
		break;
	}
	/* linux_semtimedop */
	case 192: {
		struct linux_semtimedop_args *p = params;
		iarg[a++] = p->semid; /* l_int */
		uarg[a++] = (intptr_t)p->tsops; /* struct sembuf * */
		iarg[a++] = p->nsops; /* l_size_t */
		uarg[a++] = (intptr_t)p->timeout; /* struct l_timespec * */
		*n_args = 4;
		break;
	}
	/* semop */
	case 193: {
		struct semop_args *p = params;
		iarg[a++] = p->semid; /* l_int */
		uarg[a++] = (intptr_t)p->sops; /* struct sembuf * */
		iarg[a++] = p->nsops; /* l_size_t */
		*n_args = 3;
		break;
	}
	/* linux_shmget */
	case 194: {
		struct linux_shmget_args *p = params;
		iarg[a++] = p->key; /* l_key_t */
		iarg[a++] = p->size; /* l_size_t */
		iarg[a++] = p->shmflg; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_shmctl */
	case 195: {
		struct linux_shmctl_args *p = params;
		iarg[a++] = p->shmid; /* l_int */
		iarg[a++] = p->cmd; /* l_int */
		uarg[a++] = (intptr_t)p->buf; /* struct l_shmid_ds * */
		*n_args = 3;
		break;
	}
	/* linux_shmat */
	case 196: {
		struct linux_shmat_args *p = params;
		iarg[a++] = p->shmid; /* l_int */
		uarg[a++] = (intptr_t)p->shmaddr; /* char * */
		iarg[a++] = p->shmflg; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_shmdt */
	case 197: {
		struct linux_shmdt_args *p = params;
		uarg[a++] = (intptr_t)p->shmaddr; /* char * */
		*n_args = 1;
		break;
	}
	/* linux_socket */
	case 198: {
		struct linux_socket_args *p = params;
		iarg[a++] = p->domain; /* l_int */
		iarg[a++] = p->type; /* l_int */
		iarg[a++] = p->protocol; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_socketpair */
	case 199: {
		struct linux_socketpair_args *p = params;
		iarg[a++] = p->domain; /* l_int */
		iarg[a++] = p->type; /* l_int */
		iarg[a++] = p->protocol; /* l_int */
		uarg[a++] = (intptr_t)p->rsv; /* l_uintptr_t */
		*n_args = 4;
		break;
	}
	/* linux_bind */
	case 200: {
		struct linux_bind_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->name; /* l_uintptr_t */
		iarg[a++] = p->namelen; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_listen */
	case 201: {
		struct linux_listen_args *p = params;
		iarg[a++] = p->s; /* l_int */
		iarg[a++] = p->backlog; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_accept */
	case 202: {
		struct linux_accept_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->addr; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->namelen; /* l_uintptr_t */
		*n_args = 3;
		break;
	}
	/* linux_connect */
	case 203: {
		struct linux_connect_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->name; /* l_uintptr_t */
		iarg[a++] = p->namelen; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_getsockname */
	case 204: {
		struct linux_getsockname_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->addr; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->namelen; /* l_uintptr_t */
		*n_args = 3;
		break;
	}
	/* linux_getpeername */
	case 205: {
		struct linux_getpeername_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->addr; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->namelen; /* l_uintptr_t */
		*n_args = 3;
		break;
	}
	/* linux_sendto */
	case 206: {
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
	case 207: {
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
	/* linux_setsockopt */
	case 208: {
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
	case 209: {
		struct linux_getsockopt_args *p = params;
		iarg[a++] = p->s; /* l_int */
		iarg[a++] = p->level; /* l_int */
		iarg[a++] = p->optname; /* l_int */
		uarg[a++] = (intptr_t)p->optval; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->optlen; /* l_uintptr_t */
		*n_args = 5;
		break;
	}
	/* linux_shutdown */
	case 210: {
		struct linux_shutdown_args *p = params;
		iarg[a++] = p->s; /* l_int */
		iarg[a++] = p->how; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_sendmsg */
	case 211: {
		struct linux_sendmsg_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->msg; /* l_uintptr_t */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_recvmsg */
	case 212: {
		struct linux_recvmsg_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->msg; /* l_uintptr_t */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_brk */
	case 214: {
		struct linux_brk_args *p = params;
		iarg[a++] = p->dsend; /* l_ulong */
		*n_args = 1;
		break;
	}
	/* munmap */
	case 215: {
		struct munmap_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* void * */
		iarg[a++] = p->len; /* l_size_t */
		*n_args = 2;
		break;
	}
	/* linux_mremap */
	case 216: {
		struct linux_mremap_args *p = params;
		iarg[a++] = p->addr; /* l_ulong */
		iarg[a++] = p->old_len; /* l_ulong */
		iarg[a++] = p->new_len; /* l_ulong */
		iarg[a++] = p->flags; /* l_ulong */
		iarg[a++] = p->new_addr; /* l_ulong */
		*n_args = 5;
		break;
	}
	/* linux_add_key */
	case 217: {
		*n_args = 0;
		break;
	}
	/* linux_request_key */
	case 218: {
		*n_args = 0;
		break;
	}
	/* linux_keyctl */
	case 219: {
		*n_args = 0;
		break;
	}
	/* linux_clone */
	case 220: {
		struct linux_clone_args *p = params;
		iarg[a++] = p->flags; /* l_ulong */
		iarg[a++] = p->stack; /* l_ulong */
		uarg[a++] = (intptr_t)p->parent_tidptr; /* l_int * */
		iarg[a++] = p->tls; /* l_ulong */
		uarg[a++] = (intptr_t)p->child_tidptr; /* l_int * */
		*n_args = 5;
		break;
	}
	/* linux_execve */
	case 221: {
		struct linux_execve_args *p = params;
		uarg[a++] = (intptr_t)p->path; /* char * */
		uarg[a++] = (intptr_t)p->argp; /* l_uintptr_t * */
		uarg[a++] = (intptr_t)p->envp; /* l_uintptr_t * */
		*n_args = 3;
		break;
	}
	/* linux_mmap2 */
	case 222: {
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
	/* linux_fadvise64 */
	case 223: {
		struct linux_fadvise64_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->offset; /* l_loff_t */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->advice; /* l_int */
		*n_args = 4;
		break;
	}
	/* swapon */
	case 224: {
		struct swapon_args *p = params;
		uarg[a++] = (intptr_t)p->name; /* char * */
		*n_args = 1;
		break;
	}
	/* linux_swapoff */
	case 225: {
		*n_args = 0;
		break;
	}
	/* linux_mprotect */
	case 226: {
		struct linux_mprotect_args *p = params;
		iarg[a++] = p->addr; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->prot; /* l_ulong */
		*n_args = 3;
		break;
	}
	/* linux_msync */
	case 227: {
		struct linux_msync_args *p = params;
		iarg[a++] = p->addr; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->fl; /* l_int */
		*n_args = 3;
		break;
	}
	/* mlock */
	case 228: {
		struct mlock_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* const void * */
		uarg[a++] = p->len; /* size_t */
		*n_args = 2;
		break;
	}
	/* munlock */
	case 229: {
		struct munlock_args *p = params;
		uarg[a++] = (intptr_t)p->addr; /* const void * */
		uarg[a++] = p->len; /* size_t */
		*n_args = 2;
		break;
	}
	/* mlockall */
	case 230: {
		struct mlockall_args *p = params;
		iarg[a++] = p->how; /* int */
		*n_args = 1;
		break;
	}
	/* munlockall */
	case 231: {
		*n_args = 0;
		break;
	}
	/* linux_mincore */
	case 232: {
		struct linux_mincore_args *p = params;
		iarg[a++] = p->start; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		uarg[a++] = (intptr_t)p->vec; /* u_char * */
		*n_args = 3;
		break;
	}
	/* linux_madvise */
	case 233: {
		struct linux_madvise_args *p = params;
		iarg[a++] = p->addr; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->behav; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_remap_file_pages */
	case 234: {
		*n_args = 0;
		break;
	}
	/* linux_mbind */
	case 235: {
		*n_args = 0;
		break;
	}
	/* linux_get_mempolicy */
	case 236: {
		*n_args = 0;
		break;
	}
	/* linux_set_mempolicy */
	case 237: {
		*n_args = 0;
		break;
	}
	/* linux_migrate_pages */
	case 238: {
		*n_args = 0;
		break;
	}
	/* linux_move_pages */
	case 239: {
		*n_args = 0;
		break;
	}
	/* linux_rt_tgsigqueueinfo */
	case 240: {
		struct linux_rt_tgsigqueueinfo_args *p = params;
		iarg[a++] = p->tgid; /* l_pid_t */
		iarg[a++] = p->tid; /* l_pid_t */
		iarg[a++] = p->sig; /* l_int */
		uarg[a++] = (intptr_t)p->uinfo; /* l_siginfo_t * */
		*n_args = 4;
		break;
	}
	/* linux_perf_event_open */
	case 241: {
		*n_args = 0;
		break;
	}
	/* linux_accept4 */
	case 242: {
		struct linux_accept4_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->addr; /* l_uintptr_t */
		uarg[a++] = (intptr_t)p->namelen; /* l_uintptr_t */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_recvmmsg */
	case 243: {
		struct linux_recvmmsg_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->msg; /* struct l_mmsghdr * */
		iarg[a++] = p->vlen; /* l_uint */
		iarg[a++] = p->flags; /* l_uint */
		uarg[a++] = (intptr_t)p->timeout; /* struct l_timespec * */
		*n_args = 5;
		break;
	}
	/* linux_wait4 */
	case 260: {
		struct linux_wait4_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->status; /* l_int * */
		iarg[a++] = p->options; /* l_int */
		uarg[a++] = (intptr_t)p->rusage; /* struct rusage * */
		*n_args = 4;
		break;
	}
	/* linux_prlimit64 */
	case 261: {
		struct linux_prlimit64_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		iarg[a++] = p->resource; /* l_uint */
		uarg[a++] = (intptr_t)p->new; /* struct rlimit * */
		uarg[a++] = (intptr_t)p->old; /* struct rlimit * */
		*n_args = 4;
		break;
	}
	/* linux_fanotify_init */
	case 262: {
		*n_args = 0;
		break;
	}
	/* linux_fanotify_mark */
	case 263: {
		*n_args = 0;
		break;
	}
	/* linux_name_to_handle_at */
	case 264: {
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
	case 265: {
		struct linux_open_by_handle_at_args *p = params;
		iarg[a++] = p->mountdirfd; /* l_int */
		uarg[a++] = (intptr_t)p->handle; /* struct l_file_handle * */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_clock_adjtime */
	case 266: {
		*n_args = 0;
		break;
	}
	/* linux_syncfs */
	case 267: {
		struct linux_syncfs_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_setns */
	case 268: {
		struct linux_setns_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		iarg[a++] = p->nstype; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_sendmmsg */
	case 269: {
		struct linux_sendmmsg_args *p = params;
		iarg[a++] = p->s; /* l_int */
		uarg[a++] = (intptr_t)p->msg; /* struct l_mmsghdr * */
		iarg[a++] = p->vlen; /* l_uint */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 4;
		break;
	}
	/* linux_process_vm_readv */
	case 270: {
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
	case 271: {
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
	case 272: {
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
	case 273: {
		struct linux_finit_module_args *p = params;
		iarg[a++] = p->fd; /* l_int */
		uarg[a++] = (intptr_t)p->uargs; /* const char * */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_sched_setattr */
	case 274: {
		struct linux_sched_setattr_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->attr; /* void * */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_sched_getattr */
	case 275: {
		struct linux_sched_getattr_args *p = params;
		iarg[a++] = p->pid; /* l_pid_t */
		uarg[a++] = (intptr_t)p->attr; /* void * */
		iarg[a++] = p->size; /* l_uint */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 4;
		break;
	}
	/* linux_renameat2 */
	case 276: {
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
	case 277: {
		struct linux_seccomp_args *p = params;
		iarg[a++] = p->op; /* l_uint */
		iarg[a++] = p->flags; /* l_uint */
		uarg[a++] = (intptr_t)p->uargs; /* const char * */
		*n_args = 3;
		break;
	}
	/* linux_getrandom */
	case 278: {
		struct linux_getrandom_args *p = params;
		uarg[a++] = (intptr_t)p->buf; /* char * */
		iarg[a++] = p->count; /* l_size_t */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_memfd_create */
	case 279: {
		struct linux_memfd_create_args *p = params;
		uarg[a++] = (intptr_t)p->uname_ptr; /* const char * */
		iarg[a++] = p->flags; /* l_uint */
		*n_args = 2;
		break;
	}
	/* linux_bpf */
	case 280: {
		struct linux_bpf_args *p = params;
		iarg[a++] = p->cmd; /* l_int */
		uarg[a++] = (intptr_t)p->attr; /* void * */
		iarg[a++] = p->size; /* l_uint */
		*n_args = 3;
		break;
	}
	/* linux_execveat */
	case 281: {
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
	case 282: {
		struct linux_userfaultfd_args *p = params;
		iarg[a++] = p->flags; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_membarrier */
	case 283: {
		struct linux_membarrier_args *p = params;
		iarg[a++] = p->cmd; /* l_int */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 2;
		break;
	}
	/* linux_mlock2 */
	case 284: {
		struct linux_mlock2_args *p = params;
		iarg[a++] = p->start; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->flags; /* l_int */
		*n_args = 3;
		break;
	}
	/* linux_copy_file_range */
	case 285: {
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
	case 286: {
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
	case 287: {
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
	case 288: {
		struct linux_pkey_mprotect_args *p = params;
		iarg[a++] = p->start; /* l_ulong */
		iarg[a++] = p->len; /* l_size_t */
		iarg[a++] = p->prot; /* l_ulong */
		iarg[a++] = p->pkey; /* l_int */
		*n_args = 4;
		break;
	}
	/* linux_pkey_alloc */
	case 289: {
		struct linux_pkey_alloc_args *p = params;
		iarg[a++] = p->flags; /* l_ulong */
		iarg[a++] = p->init_val; /* l_ulong */
		*n_args = 2;
		break;
	}
	/* linux_pkey_free */
	case 290: {
		struct linux_pkey_free_args *p = params;
		iarg[a++] = p->pkey; /* l_int */
		*n_args = 1;
		break;
	}
	/* linux_statx */
	case 291: {
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
	case 292: {
		*n_args = 0;
		break;
	}
	/* linux_rseq */
	case 293: {
		struct linux_rseq_args *p = params;
		uarg[a++] = (intptr_t)p->rseq; /* struct linux_rseq * */
		uarg[a++] = p->rseq_len; /* uint32_t */
		iarg[a++] = p->flags; /* l_int */
		uarg[a++] = p->sig; /* uint32_t */
		*n_args = 4;
		break;
	}
	/* linux_kexec_file_load */
	case 294: {
		*n_args = 0;
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
	/* linux_setxattr */
	case 5:
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
	case 6:
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
	case 7:
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
	case 8:
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
	case 9:
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
	case 10:
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
	case 11:
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
	case 12:
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
	case 13:
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
	case 14:
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
	case 15:
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
	case 16:
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
	/* linux_getcwd */
	case 17:
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
	/* linux_lookup_dcookie */
	case 18:
		break;
	/* linux_eventfd2 */
	case 19:
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
	case 20:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_epoll_ctl */
	case 21:
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
	/* linux_epoll_pwait */
	case 22:
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
	/* dup */
	case 23:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* linux_dup3 */
	case 24:
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
	/* linux_fcntl */
	case 25:
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
	/* linux_inotify_init1 */
	case 26:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_inotify_add_watch */
	case 27:
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
	case 28:
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
	/* linux_ioctl */
	case 29:
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
	/* linux_ioprio_set */
	case 30:
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
	case 31:
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
	/* flock */
	case 32:
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
	/* linux_mknodat */
	case 33:
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
			p = "l_dev_t";
			break;
		default:
			break;
		};
		break;
	/* linux_mkdirat */
	case 34:
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
	/* linux_unlinkat */
	case 35:
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
	/* linux_symlinkat */
	case 36:
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
	/* linux_linkat */
	case 37:
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
	/* linux_renameat */
	case 38:
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
	/* linux_mount */
	case 40:
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
	/* linux_pivot_root */
	case 41:
		break;
	/* linux_statfs */
	case 43:
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
	case 44:
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
	/* linux_truncate */
	case 45:
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
	/* linux_ftruncate */
	case 46:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		case 1:
			p = "l_long";
			break;
		default:
			break;
		};
		break;
	/* linux_fallocate */
	case 47:
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
	/* linux_faccessat */
	case 48:
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
	/* linux_chdir */
	case 49:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* fchdir */
	case 50:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* chroot */
	case 51:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* fchmod */
	case 52:
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
	/* linux_fchmodat */
	case 53:
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
	/* linux_fchownat */
	case 54:
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
	/* fchown */
	case 55:
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
	/* linux_openat */
	case 56:
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
	/* close */
	case 57:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* linux_vhangup */
	case 58:
		break;
	/* linux_pipe2 */
	case 59:
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
	/* linux_getdents64 */
	case 61:
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
	/* linux_lseek */
	case 62:
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
	/* read */
	case 63:
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
	case 64:
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
	/* readv */
	case 65:
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
	case 66:
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
	/* linux_pread */
	case 67:
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
	case 68:
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
	/* linux_preadv */
	case 69:
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
	case 70:
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
	/* linux_sendfile */
	case 71:
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
	/* linux_pselect6 */
	case 72:
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
	case 73:
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
	/* linux_signalfd4 */
	case 74:
		break;
	/* linux_vmsplice */
	case 75:
		break;
	/* linux_splice */
	case 76:
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
	case 77:
		break;
	/* linux_readlinkat */
	case 78:
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
	/* linux_newfstatat */
	case 79:
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
	/* linux_newfstat */
	case 80:
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
	/* fsync */
	case 82:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* linux_fdatasync */
	case 83:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_sync_file_range */
	case 84:
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
	/* linux_timerfd_create */
	case 85:
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
	/* linux_timerfd_settime */
	case 86:
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
	case 87:
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
	/* linux_utimensat */
	case 88:
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
	/* acct */
	case 89:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* linux_capget */
	case 90:
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
	case 91:
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
	/* linux_personality */
	case 92:
		switch (ndx) {
		case 0:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_exit */
	case 93:
		switch (ndx) {
		case 0:
			p = "u_int";
			break;
		default:
			break;
		};
		break;
	/* linux_exit_group */
	case 94:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_waitid */
	case 95:
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
	/* linux_set_tid_address */
	case 96:
		switch (ndx) {
		case 0:
			p = "userland l_int *";
			break;
		default:
			break;
		};
		break;
	/* linux_unshare */
	case 97:
		break;
	/* linux_sys_futex */
	case 98:
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
	/* linux_set_robust_list */
	case 99:
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
	case 100:
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
	/* linux_nanosleep */
	case 101:
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
	case 102:
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
	/* linux_setitimer */
	case 103:
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
	/* linux_kexec_load */
	case 104:
		break;
	/* linux_init_module */
	case 105:
		break;
	/* linux_delete_module */
	case 106:
		break;
	/* linux_timer_create */
	case 107:
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
	/* linux_timer_gettime */
	case 108:
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
	case 109:
		switch (ndx) {
		case 0:
			p = "l_timer_t";
			break;
		default:
			break;
		};
		break;
	/* linux_timer_settime */
	case 110:
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
	/* linux_timer_delete */
	case 111:
		switch (ndx) {
		case 0:
			p = "l_timer_t";
			break;
		default:
			break;
		};
		break;
	/* linux_clock_settime */
	case 112:
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
	case 113:
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
	case 114:
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
	case 115:
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
	/* linux_syslog */
	case 116:
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
	/* linux_ptrace */
	case 117:
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
	/* linux_sched_setparam */
	case 118:
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
	case 119:
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
	case 120:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_getparam */
	case 121:
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
	/* linux_sched_setaffinity */
	case 122:
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
	case 123:
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
	/* sched_yield */
	case 124:
		break;
	/* linux_sched_get_priority_max */
	case 125:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_get_priority_min */
	case 126:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_sched_rr_get_interval */
	case 127:
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
	/* linux_kill */
	case 129:
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
	/* linux_tkill */
	case 130:
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
	/* linux_tgkill */
	case 131:
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
	/* linux_sigaltstack */
	case 132:
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
	/* linux_rt_sigsuspend */
	case 133:
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
	/* linux_rt_sigaction */
	case 134:
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
	case 135:
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
	/* linux_rt_sigpending */
	case 136:
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
	case 137:
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
	case 138:
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
	/* linux_rt_sigreturn */
	case 139:
		break;
	/* setpriority */
	case 140:
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
	/* linux_getpriority */
	case 141:
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
	/* linux_reboot */
	case 142:
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
	/* setregid */
	case 143:
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
	/* setgid */
	case 144:
		switch (ndx) {
		case 0:
			p = "gid_t";
			break;
		default:
			break;
		};
		break;
	/* setreuid */
	case 145:
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
	/* setuid */
	case 146:
		switch (ndx) {
		case 0:
			p = "uid_t";
			break;
		default:
			break;
		};
		break;
	/* setresuid */
	case 147:
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
	case 148:
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
	case 149:
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
	case 150:
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
	/* linux_setfsuid */
	case 151:
		switch (ndx) {
		case 0:
			p = "l_uid_t";
			break;
		default:
			break;
		};
		break;
	/* linux_setfsgid */
	case 152:
		switch (ndx) {
		case 0:
			p = "l_gid_t";
			break;
		default:
			break;
		};
		break;
	/* linux_times */
	case 153:
		switch (ndx) {
		case 0:
			p = "userland struct l_times_argv *";
			break;
		default:
			break;
		};
		break;
	/* setpgid */
	case 154:
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
	/* getpgid */
	case 155:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* linux_getsid */
	case 156:
		switch (ndx) {
		case 0:
			p = "l_pid_t";
			break;
		default:
			break;
		};
		break;
	/* setsid */
	case 157:
		break;
	/* linux_getgroups */
	case 158:
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
	case 159:
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
	/* linux_newuname */
	case 160:
		switch (ndx) {
		case 0:
			p = "userland struct l_new_utsname *";
			break;
		default:
			break;
		};
		break;
	/* linux_sethostname */
	case 161:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		case 1:
			p = "l_uint";
			break;
		default:
			break;
		};
		break;
	/* linux_setdomainname */
	case 162:
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
	/* linux_getrlimit */
	case 163:
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
	/* linux_setrlimit */
	case 164:
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
	case 165:
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
	/* umask */
	case 166:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* linux_prctl */
	case 167:
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
	/* linux_getcpu */
	case 168:
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
	/* gettimeofday */
	case 169:
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
	/* settimeofday */
	case 170:
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
	/* linux_adjtimex */
	case 171:
		break;
	/* linux_getpid */
	case 172:
		break;
	/* linux_getppid */
	case 173:
		break;
	/* linux_getuid */
	case 174:
		break;
	/* geteuid */
	case 175:
		break;
	/* linux_getgid */
	case 176:
		break;
	/* getegid */
	case 177:
		break;
	/* linux_gettid */
	case 178:
		break;
	/* linux_sysinfo */
	case 179:
		switch (ndx) {
		case 0:
			p = "userland struct l_sysinfo *";
			break;
		default:
			break;
		};
		break;
	/* linux_mq_open */
	case 180:
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
	case 181:
		switch (ndx) {
		case 0:
			p = "userland const char *";
			break;
		default:
			break;
		};
		break;
	/* linux_mq_timedsend */
	case 182:
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
	case 183:
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
	case 184:
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
	case 185:
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
	/* linux_msgget */
	case 186:
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
	/* linux_msgctl */
	case 187:
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
	/* linux_msgrcv */
	case 188:
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
	/* linux_msgsnd */
	case 189:
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
	/* linux_semget */
	case 190:
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
	/* linux_semctl */
	case 191:
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
	/* linux_semtimedop */
	case 192:
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
	/* semop */
	case 193:
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
	/* linux_shmget */
	case 194:
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
	/* linux_shmctl */
	case 195:
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
	/* linux_shmat */
	case 196:
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
	/* linux_shmdt */
	case 197:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* linux_socket */
	case 198:
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
	/* linux_socketpair */
	case 199:
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
	/* linux_bind */
	case 200:
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
	case 201:
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
	/* linux_accept */
	case 202:
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
	/* linux_connect */
	case 203:
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
	/* linux_getsockname */
	case 204:
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
	case 205:
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
	case 206:
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
	case 207:
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
	/* linux_setsockopt */
	case 208:
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
	case 209:
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
	/* linux_shutdown */
	case 210:
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
	/* linux_sendmsg */
	case 211:
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
	case 212:
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
	/* linux_brk */
	case 214:
		switch (ndx) {
		case 0:
			p = "l_ulong";
			break;
		default:
			break;
		};
		break;
	/* munmap */
	case 215:
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
	/* linux_mremap */
	case 216:
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
	/* linux_add_key */
	case 217:
		break;
	/* linux_request_key */
	case 218:
		break;
	/* linux_keyctl */
	case 219:
		break;
	/* linux_clone */
	case 220:
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
			p = "l_ulong";
			break;
		case 4:
			p = "userland l_int *";
			break;
		default:
			break;
		};
		break;
	/* linux_execve */
	case 221:
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
	/* linux_mmap2 */
	case 222:
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
	/* linux_fadvise64 */
	case 223:
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
	/* swapon */
	case 224:
		switch (ndx) {
		case 0:
			p = "userland char *";
			break;
		default:
			break;
		};
		break;
	/* linux_swapoff */
	case 225:
		break;
	/* linux_mprotect */
	case 226:
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
	/* linux_msync */
	case 227:
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
	/* mlock */
	case 228:
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
	case 229:
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
	case 230:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
	/* munlockall */
	case 231:
		break;
	/* linux_mincore */
	case 232:
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
	case 233:
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
	/* linux_remap_file_pages */
	case 234:
		break;
	/* linux_mbind */
	case 235:
		break;
	/* linux_get_mempolicy */
	case 236:
		break;
	/* linux_set_mempolicy */
	case 237:
		break;
	/* linux_migrate_pages */
	case 238:
		break;
	/* linux_move_pages */
	case 239:
		break;
	/* linux_rt_tgsigqueueinfo */
	case 240:
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
	case 241:
		break;
	/* linux_accept4 */
	case 242:
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
	/* linux_recvmmsg */
	case 243:
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
	/* linux_wait4 */
	case 260:
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
	/* linux_prlimit64 */
	case 261:
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
	/* linux_fanotify_init */
	case 262:
		break;
	/* linux_fanotify_mark */
	case 263:
		break;
	/* linux_name_to_handle_at */
	case 264:
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
	case 265:
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
	case 266:
		break;
	/* linux_syncfs */
	case 267:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_setns */
	case 268:
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
	/* linux_sendmmsg */
	case 269:
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
	/* linux_process_vm_readv */
	case 270:
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
	case 271:
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
	case 272:
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
	case 273:
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
	case 274:
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
	case 275:
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
	case 276:
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
	case 277:
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
	case 278:
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
	case 279:
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
	/* linux_bpf */
	case 280:
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
	case 281:
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
	case 282:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_membarrier */
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
	/* linux_mlock2 */
	case 284:
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
	case 285:
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
	case 286:
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
	case 287:
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
	case 288:
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
	case 289:
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
	case 290:
		switch (ndx) {
		case 0:
			p = "l_int";
			break;
		default:
			break;
		};
		break;
	/* linux_statx */
	case 291:
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
	case 292:
		break;
	/* linux_rseq */
	case 293:
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
	/* linux_kexec_file_load */
	case 294:
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
	/* linux_setxattr */
	case 5:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_lsetxattr */
	case 6:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fsetxattr */
	case 7:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getxattr */
	case 8:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_lgetxattr */
	case 9:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fgetxattr */
	case 10:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_listxattr */
	case 11:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_llistxattr */
	case 12:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_flistxattr */
	case 13:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_removexattr */
	case 14:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_lremovexattr */
	case 15:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fremovexattr */
	case 16:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getcwd */
	case 17:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_lookup_dcookie */
	case 18:
	/* linux_eventfd2 */
	case 19:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_epoll_create1 */
	case 20:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_epoll_ctl */
	case 21:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_epoll_pwait */
	case 22:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* dup */
	case 23:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_dup3 */
	case 24:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fcntl */
	case 25:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_inotify_init1 */
	case 26:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_inotify_add_watch */
	case 27:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_inotify_rm_watch */
	case 28:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ioctl */
	case 29:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ioprio_set */
	case 30:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ioprio_get */
	case 31:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* flock */
	case 32:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mknodat */
	case 33:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mkdirat */
	case 34:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_unlinkat */
	case 35:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_symlinkat */
	case 36:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_linkat */
	case 37:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_renameat */
	case 38:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mount */
	case 40:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pivot_root */
	case 41:
	/* linux_statfs */
	case 43:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fstatfs */
	case 44:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_truncate */
	case 45:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ftruncate */
	case 46:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fallocate */
	case 47:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_faccessat */
	case 48:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_chdir */
	case 49:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchdir */
	case 50:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* chroot */
	case 51:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchmod */
	case 52:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fchmodat */
	case 53:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fchownat */
	case 54:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fchown */
	case 55:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_openat */
	case 56:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* close */
	case 57:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_vhangup */
	case 58:
	/* linux_pipe2 */
	case 59:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getdents64 */
	case 61:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_lseek */
	case 62:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* read */
	case 63:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_write */
	case 64:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* readv */
	case 65:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_writev */
	case 66:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pread */
	case 67:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pwrite */
	case 68:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_preadv */
	case 69:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pwritev */
	case 70:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sendfile */
	case 71:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pselect6 */
	case 72:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ppoll */
	case 73:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_signalfd4 */
	case 74:
	/* linux_vmsplice */
	case 75:
	/* linux_splice */
	case 76:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_tee */
	case 77:
	/* linux_readlinkat */
	case 78:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_newfstatat */
	case 79:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_newfstat */
	case 80:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* fsync */
	case 82:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fdatasync */
	case 83:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sync_file_range */
	case 84:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timerfd_create */
	case 85:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timerfd_settime */
	case 86:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timerfd_gettime */
	case 87:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_utimensat */
	case 88:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* acct */
	case 89:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_capget */
	case 90:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_capset */
	case 91:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_personality */
	case 92:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_exit */
	case 93:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_exit_group */
	case 94:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_waitid */
	case 95:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_set_tid_address */
	case 96:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_unshare */
	case 97:
	/* linux_sys_futex */
	case 98:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_set_robust_list */
	case 99:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_get_robust_list */
	case 100:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_nanosleep */
	case 101:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getitimer */
	case 102:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setitimer */
	case 103:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_kexec_load */
	case 104:
	/* linux_init_module */
	case 105:
	/* linux_delete_module */
	case 106:
	/* linux_timer_create */
	case 107:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timer_gettime */
	case 108:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timer_getoverrun */
	case 109:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timer_settime */
	case 110:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_timer_delete */
	case 111:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_clock_settime */
	case 112:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_clock_gettime */
	case 113:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_clock_getres */
	case 114:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_clock_nanosleep */
	case 115:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_syslog */
	case 116:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_ptrace */
	case 117:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_setparam */
	case 118:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_setscheduler */
	case 119:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_getscheduler */
	case 120:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_getparam */
	case 121:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_setaffinity */
	case 122:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_getaffinity */
	case 123:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* sched_yield */
	case 124:
	/* linux_sched_get_priority_max */
	case 125:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_get_priority_min */
	case 126:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_rr_get_interval */
	case 127:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_kill */
	case 129:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_tkill */
	case 130:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_tgkill */
	case 131:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sigaltstack */
	case 132:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigsuspend */
	case 133:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigaction */
	case 134:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigprocmask */
	case 135:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigpending */
	case 136:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigtimedwait */
	case 137:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigqueueinfo */
	case 138:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_rt_sigreturn */
	case 139:
	/* setpriority */
	case 140:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getpriority */
	case 141:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_reboot */
	case 142:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setregid */
	case 143:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setgid */
	case 144:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setreuid */
	case 145:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setuid */
	case 146:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setresuid */
	case 147:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getresuid */
	case 148:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setresgid */
	case 149:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getresgid */
	case 150:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setfsuid */
	case 151:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setfsgid */
	case 152:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_times */
	case 153:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setpgid */
	case 154:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getpgid */
	case 155:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getsid */
	case 156:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* setsid */
	case 157:
	/* linux_getgroups */
	case 158:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setgroups */
	case 159:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_newuname */
	case 160:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sethostname */
	case 161:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setdomainname */
	case 162:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getrlimit */
	case 163:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setrlimit */
	case 164:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* getrusage */
	case 165:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* umask */
	case 166:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_prctl */
	case 167:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getcpu */
	case 168:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* gettimeofday */
	case 169:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* settimeofday */
	case 170:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_adjtimex */
	case 171:
	/* linux_getpid */
	case 172:
	/* linux_getppid */
	case 173:
	/* linux_getuid */
	case 174:
	/* geteuid */
	case 175:
	/* linux_getgid */
	case 176:
	/* getegid */
	case 177:
	/* linux_gettid */
	case 178:
	/* linux_sysinfo */
	case 179:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mq_open */
	case 180:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mq_unlink */
	case 181:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mq_timedsend */
	case 182:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mq_timedreceive */
	case 183:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mq_notify */
	case 184:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mq_getsetattr */
	case 185:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_msgget */
	case 186:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_msgctl */
	case 187:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_msgrcv */
	case 188:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_msgsnd */
	case 189:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_semget */
	case 190:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_semctl */
	case 191:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_semtimedop */
	case 192:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* semop */
	case 193:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_shmget */
	case 194:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_shmctl */
	case 195:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_shmat */
	case 196:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_shmdt */
	case 197:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_socket */
	case 198:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_socketpair */
	case 199:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_bind */
	case 200:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_listen */
	case 201:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_accept */
	case 202:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_connect */
	case 203:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getsockname */
	case 204:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getpeername */
	case 205:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sendto */
	case 206:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_recvfrom */
	case 207:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setsockopt */
	case 208:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getsockopt */
	case 209:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_shutdown */
	case 210:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sendmsg */
	case 211:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_recvmsg */
	case 212:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_brk */
	case 214:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* munmap */
	case 215:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mremap */
	case 216:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_add_key */
	case 217:
	/* linux_request_key */
	case 218:
	/* linux_keyctl */
	case 219:
	/* linux_clone */
	case 220:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_execve */
	case 221:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mmap2 */
	case 222:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fadvise64 */
	case 223:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* swapon */
	case 224:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_swapoff */
	case 225:
	/* linux_mprotect */
	case 226:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_msync */
	case 227:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mlock */
	case 228:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* munlock */
	case 229:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* mlockall */
	case 230:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* munlockall */
	case 231:
	/* linux_mincore */
	case 232:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_madvise */
	case 233:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_remap_file_pages */
	case 234:
	/* linux_mbind */
	case 235:
	/* linux_get_mempolicy */
	case 236:
	/* linux_set_mempolicy */
	case 237:
	/* linux_migrate_pages */
	case 238:
	/* linux_move_pages */
	case 239:
	/* linux_rt_tgsigqueueinfo */
	case 240:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_perf_event_open */
	case 241:
	/* linux_accept4 */
	case 242:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_recvmmsg */
	case 243:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_wait4 */
	case 260:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_prlimit64 */
	case 261:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_fanotify_init */
	case 262:
	/* linux_fanotify_mark */
	case 263:
	/* linux_name_to_handle_at */
	case 264:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_open_by_handle_at */
	case 265:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_clock_adjtime */
	case 266:
	/* linux_syncfs */
	case 267:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_setns */
	case 268:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sendmmsg */
	case 269:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_process_vm_readv */
	case 270:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_process_vm_writev */
	case 271:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_kcmp */
	case 272:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_finit_module */
	case 273:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_setattr */
	case 274:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_sched_getattr */
	case 275:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_renameat2 */
	case 276:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_seccomp */
	case 277:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_getrandom */
	case 278:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_memfd_create */
	case 279:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_bpf */
	case 280:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_execveat */
	case 281:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_userfaultfd */
	case 282:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_membarrier */
	case 283:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_mlock2 */
	case 284:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_copy_file_range */
	case 285:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_preadv2 */
	case 286:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pwritev2 */
	case 287:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pkey_mprotect */
	case 288:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pkey_alloc */
	case 289:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_pkey_free */
	case 290:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_statx */
	case 291:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_io_pgetevents */
	case 292:
	/* linux_rseq */
	case 293:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
	/* linux_kexec_file_load */
	case 294:
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
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
