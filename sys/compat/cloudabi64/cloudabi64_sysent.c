/*
 * System call switch table.
 *
 * DO NOT EDIT-- this file is automatically @generated.
 * $FreeBSD$
 */

#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <contrib/cloudabi/cloudabi64_types.h>
#include <compat/cloudabi64/cloudabi64_proto.h>

#define AS(name) (sizeof(struct name) / sizeof(register_t))

/* The casts are bogus but will do for now. */
struct sysent cloudabi64_sysent[] = {
	{ .sy_narg = AS(cloudabi_sys_clock_res_get_args), .sy_call = (sy_call_t *)cloudabi_sys_clock_res_get, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 0 = cloudabi_sys_clock_res_get */
	{ .sy_narg = AS(cloudabi_sys_clock_time_get_args), .sy_call = (sy_call_t *)cloudabi_sys_clock_time_get, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 1 = cloudabi_sys_clock_time_get */
	{ .sy_narg = AS(cloudabi_sys_condvar_signal_args), .sy_call = (sy_call_t *)cloudabi_sys_condvar_signal, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 2 = cloudabi_sys_condvar_signal */
	{ .sy_narg = AS(cloudabi_sys_fd_close_args), .sy_call = (sy_call_t *)cloudabi_sys_fd_close, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 3 = cloudabi_sys_fd_close */
	{ .sy_narg = AS(cloudabi_sys_fd_create1_args), .sy_call = (sy_call_t *)cloudabi_sys_fd_create1, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 4 = cloudabi_sys_fd_create1 */
	{ .sy_narg = AS(cloudabi_sys_fd_create2_args), .sy_call = (sy_call_t *)cloudabi_sys_fd_create2, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 5 = cloudabi_sys_fd_create2 */
	{ .sy_narg = AS(cloudabi_sys_fd_datasync_args), .sy_call = (sy_call_t *)cloudabi_sys_fd_datasync, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 6 = cloudabi_sys_fd_datasync */
	{ .sy_narg = AS(cloudabi_sys_fd_dup_args), .sy_call = (sy_call_t *)cloudabi_sys_fd_dup, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 7 = cloudabi_sys_fd_dup */
	{ .sy_narg = AS(cloudabi64_sys_fd_pread_args), .sy_call = (sy_call_t *)cloudabi64_sys_fd_pread, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 8 = cloudabi64_sys_fd_pread */
	{ .sy_narg = AS(cloudabi64_sys_fd_pwrite_args), .sy_call = (sy_call_t *)cloudabi64_sys_fd_pwrite, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 9 = cloudabi64_sys_fd_pwrite */
	{ .sy_narg = AS(cloudabi64_sys_fd_read_args), .sy_call = (sy_call_t *)cloudabi64_sys_fd_read, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 10 = cloudabi64_sys_fd_read */
	{ .sy_narg = AS(cloudabi_sys_fd_replace_args), .sy_call = (sy_call_t *)cloudabi_sys_fd_replace, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 11 = cloudabi_sys_fd_replace */
	{ .sy_narg = AS(cloudabi_sys_fd_seek_args), .sy_call = (sy_call_t *)cloudabi_sys_fd_seek, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 12 = cloudabi_sys_fd_seek */
	{ .sy_narg = AS(cloudabi_sys_fd_stat_get_args), .sy_call = (sy_call_t *)cloudabi_sys_fd_stat_get, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 13 = cloudabi_sys_fd_stat_get */
	{ .sy_narg = AS(cloudabi_sys_fd_stat_put_args), .sy_call = (sy_call_t *)cloudabi_sys_fd_stat_put, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 14 = cloudabi_sys_fd_stat_put */
	{ .sy_narg = AS(cloudabi_sys_fd_sync_args), .sy_call = (sy_call_t *)cloudabi_sys_fd_sync, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 15 = cloudabi_sys_fd_sync */
	{ .sy_narg = AS(cloudabi64_sys_fd_write_args), .sy_call = (sy_call_t *)cloudabi64_sys_fd_write, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 16 = cloudabi64_sys_fd_write */
	{ .sy_narg = AS(cloudabi_sys_file_advise_args), .sy_call = (sy_call_t *)cloudabi_sys_file_advise, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 17 = cloudabi_sys_file_advise */
	{ .sy_narg = AS(cloudabi_sys_file_allocate_args), .sy_call = (sy_call_t *)cloudabi_sys_file_allocate, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 18 = cloudabi_sys_file_allocate */
	{ .sy_narg = AS(cloudabi_sys_file_create_args), .sy_call = (sy_call_t *)cloudabi_sys_file_create, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 19 = cloudabi_sys_file_create */
	{ .sy_narg = AS(cloudabi_sys_file_link_args), .sy_call = (sy_call_t *)cloudabi_sys_file_link, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 20 = cloudabi_sys_file_link */
	{ .sy_narg = AS(cloudabi_sys_file_open_args), .sy_call = (sy_call_t *)cloudabi_sys_file_open, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 21 = cloudabi_sys_file_open */
	{ .sy_narg = AS(cloudabi_sys_file_readdir_args), .sy_call = (sy_call_t *)cloudabi_sys_file_readdir, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 22 = cloudabi_sys_file_readdir */
	{ .sy_narg = AS(cloudabi_sys_file_readlink_args), .sy_call = (sy_call_t *)cloudabi_sys_file_readlink, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 23 = cloudabi_sys_file_readlink */
	{ .sy_narg = AS(cloudabi_sys_file_rename_args), .sy_call = (sy_call_t *)cloudabi_sys_file_rename, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 24 = cloudabi_sys_file_rename */
	{ .sy_narg = AS(cloudabi_sys_file_stat_fget_args), .sy_call = (sy_call_t *)cloudabi_sys_file_stat_fget, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 25 = cloudabi_sys_file_stat_fget */
	{ .sy_narg = AS(cloudabi_sys_file_stat_fput_args), .sy_call = (sy_call_t *)cloudabi_sys_file_stat_fput, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 26 = cloudabi_sys_file_stat_fput */
	{ .sy_narg = AS(cloudabi_sys_file_stat_get_args), .sy_call = (sy_call_t *)cloudabi_sys_file_stat_get, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 27 = cloudabi_sys_file_stat_get */
	{ .sy_narg = AS(cloudabi_sys_file_stat_put_args), .sy_call = (sy_call_t *)cloudabi_sys_file_stat_put, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 28 = cloudabi_sys_file_stat_put */
	{ .sy_narg = AS(cloudabi_sys_file_symlink_args), .sy_call = (sy_call_t *)cloudabi_sys_file_symlink, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 29 = cloudabi_sys_file_symlink */
	{ .sy_narg = AS(cloudabi_sys_file_unlink_args), .sy_call = (sy_call_t *)cloudabi_sys_file_unlink, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 30 = cloudabi_sys_file_unlink */
	{ .sy_narg = AS(cloudabi_sys_lock_unlock_args), .sy_call = (sy_call_t *)cloudabi_sys_lock_unlock, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 31 = cloudabi_sys_lock_unlock */
	{ .sy_narg = AS(cloudabi_sys_mem_advise_args), .sy_call = (sy_call_t *)cloudabi_sys_mem_advise, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 32 = cloudabi_sys_mem_advise */
	{ .sy_narg = AS(cloudabi_sys_mem_map_args), .sy_call = (sy_call_t *)cloudabi_sys_mem_map, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 33 = cloudabi_sys_mem_map */
	{ .sy_narg = AS(cloudabi_sys_mem_protect_args), .sy_call = (sy_call_t *)cloudabi_sys_mem_protect, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 34 = cloudabi_sys_mem_protect */
	{ .sy_narg = AS(cloudabi_sys_mem_sync_args), .sy_call = (sy_call_t *)cloudabi_sys_mem_sync, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 35 = cloudabi_sys_mem_sync */
	{ .sy_narg = AS(cloudabi_sys_mem_unmap_args), .sy_call = (sy_call_t *)cloudabi_sys_mem_unmap, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 36 = cloudabi_sys_mem_unmap */
	{ .sy_narg = AS(cloudabi64_sys_poll_args), .sy_call = (sy_call_t *)cloudabi64_sys_poll, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 37 = cloudabi64_sys_poll */
	{ .sy_narg = AS(cloudabi_sys_proc_exec_args), .sy_call = (sy_call_t *)cloudabi_sys_proc_exec, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 38 = cloudabi_sys_proc_exec */
	{ .sy_narg = AS(cloudabi_sys_proc_exit_args), .sy_call = (sy_call_t *)cloudabi_sys_proc_exit, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 39 = cloudabi_sys_proc_exit */
	{ .sy_narg = 0, .sy_call = (sy_call_t *)cloudabi_sys_proc_fork, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 40 = cloudabi_sys_proc_fork */
	{ .sy_narg = AS(cloudabi_sys_proc_raise_args), .sy_call = (sy_call_t *)cloudabi_sys_proc_raise, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 41 = cloudabi_sys_proc_raise */
	{ .sy_narg = AS(cloudabi_sys_random_get_args), .sy_call = (sy_call_t *)cloudabi_sys_random_get, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 42 = cloudabi_sys_random_get */
	{ .sy_narg = AS(cloudabi64_sys_sock_recv_args), .sy_call = (sy_call_t *)cloudabi64_sys_sock_recv, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 43 = cloudabi64_sys_sock_recv */
	{ .sy_narg = AS(cloudabi64_sys_sock_send_args), .sy_call = (sy_call_t *)cloudabi64_sys_sock_send, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 44 = cloudabi64_sys_sock_send */
	{ .sy_narg = AS(cloudabi_sys_sock_shutdown_args), .sy_call = (sy_call_t *)cloudabi_sys_sock_shutdown, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 45 = cloudabi_sys_sock_shutdown */
	{ .sy_narg = AS(cloudabi64_sys_thread_create_args), .sy_call = (sy_call_t *)cloudabi64_sys_thread_create, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 46 = cloudabi64_sys_thread_create */
	{ .sy_narg = AS(cloudabi_sys_thread_exit_args), .sy_call = (sy_call_t *)cloudabi_sys_thread_exit, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 47 = cloudabi_sys_thread_exit */
	{ .sy_narg = 0, .sy_call = (sy_call_t *)cloudabi_sys_thread_yield, .sy_auevent = AUE_NULL, .sy_flags = SYF_CAPENABLED, .sy_thrcnt = SY_THR_STATIC },	/* 48 = cloudabi_sys_thread_yield */
};
