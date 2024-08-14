#ifndef custom_bsd_exit /* Allow override */
void bsd_exit(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	do_bsd_exit(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_exit */

#ifndef custom_bsd_fork /* Allow override */
int bsd_fork(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fork();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fork */

#ifndef custom_bsd_read /* Allow override */
ssize_t bsd_read(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_read(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_read */

#ifndef custom_bsd_write /* Allow override */
ssize_t bsd_write(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_write(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_write */

#ifndef custom_bsd_open /* Allow override */
int bsd_open(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_open(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_open */

#ifndef custom_bsd_close /* Allow override */
int bsd_close(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_close(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_close */

#ifndef custom_bsd_wait4 /* Allow override */
int bsd_wait4(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_wait4(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_wait4 */

#ifndef custom_bsd_link /* Allow override */
int bsd_link(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_link(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_link */

#ifndef custom_bsd_unlink /* Allow override */
int bsd_unlink(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_unlink(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_unlink */

#ifndef custom_bsd_chdir /* Allow override */
int bsd_chdir(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_chdir(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_chdir */

#ifndef custom_bsd_fchdir /* Allow override */
int bsd_fchdir(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fchdir(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fchdir */

#ifndef custom_bsd_chmod /* Allow override */
int bsd_chmod(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_chmod(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_chmod */

#ifndef custom_bsd_chown /* Allow override */
int bsd_chown(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_chown(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_chown */

#ifndef custom_bsd_break /* Allow override */
void * bsd_break(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_break(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_break */

#ifndef custom_bsd_getpid /* Allow override */
pid_t bsd_getpid(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getpid();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getpid */

#ifndef custom_bsd_mount /* Allow override */
int bsd_mount(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mount(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mount */

#ifndef custom_bsd_unmount /* Allow override */
int bsd_unmount(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_unmount(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_unmount */

#ifndef custom_bsd_setuid /* Allow override */
int bsd_setuid(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setuid(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setuid */

#ifndef custom_bsd_getuid /* Allow override */
uid_t bsd_getuid(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getuid();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getuid */

#ifndef custom_bsd_geteuid /* Allow override */
uid_t bsd_geteuid(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_geteuid();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_geteuid */

#ifndef custom_bsd_ptrace /* Allow override */
int bsd_ptrace(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ptrace(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ptrace */

#ifndef custom_bsd_recvmsg /* Allow override */
ssize_t bsd_recvmsg(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_recvmsg(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_recvmsg */

#ifndef custom_bsd_sendmsg /* Allow override */
ssize_t bsd_sendmsg(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sendmsg(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sendmsg */

#ifndef custom_bsd_recvfrom /* Allow override */
ssize_t bsd_recvfrom(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_recvfrom(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_recvfrom */

#ifndef custom_bsd_accept /* Allow override */
int bsd_accept(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_accept(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_accept */

#ifndef custom_bsd_getpeername /* Allow override */
int bsd_getpeername(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getpeername(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getpeername */

#ifndef custom_bsd_getsockname /* Allow override */
int bsd_getsockname(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getsockname(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getsockname */

#ifndef custom_bsd_access /* Allow override */
int bsd_access(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_access(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_access */

#ifndef custom_bsd_chflags /* Allow override */
int bsd_chflags(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_chflags(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_chflags */

#ifndef custom_bsd_fchflags /* Allow override */
int bsd_fchflags(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fchflags(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fchflags */

#ifndef custom_bsd_sync /* Allow override */
int bsd_sync(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sync();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sync */

#ifndef custom_bsd_kill /* Allow override */
int bsd_kill(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kill(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kill */

#ifndef custom_bsd_getppid /* Allow override */
pid_t bsd_getppid(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getppid();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getppid */

#ifndef custom_bsd_dup /* Allow override */
int bsd_dup(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_dup(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_dup */

#ifndef custom_bsd_getegid /* Allow override */
gid_t bsd_getegid(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getegid();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getegid */

#ifndef custom_bsd_profil /* Allow override */
int bsd_profil(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_profil(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_profil */

#ifndef custom_bsd_ktrace /* Allow override */
int bsd_ktrace(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ktrace(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ktrace */

#ifndef custom_bsd_getgid /* Allow override */
gid_t bsd_getgid(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getgid();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getgid */

#ifndef custom_bsd_getlogin /* Allow override */
int bsd_getlogin(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getlogin(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getlogin */

#ifndef custom_bsd_setlogin /* Allow override */
int bsd_setlogin(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setlogin(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setlogin */

#ifndef custom_bsd_acct /* Allow override */
int bsd_acct(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_acct(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_acct */

#ifndef custom_bsd_sigaltstack /* Allow override */
int bsd_sigaltstack(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sigaltstack(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sigaltstack */

#ifndef custom_bsd_ioctl /* Allow override */
int bsd_ioctl(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ioctl(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ioctl */

#ifndef custom_bsd_reboot /* Allow override */
int bsd_reboot(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_reboot(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_reboot */

#ifndef custom_bsd_revoke /* Allow override */
int bsd_revoke(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_revoke(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_revoke */

#ifndef custom_bsd_symlink /* Allow override */
int bsd_symlink(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_symlink(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_symlink */

#ifndef custom_bsd_readlink /* Allow override */
ssize_t bsd_readlink(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_readlink(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_readlink */

#ifndef custom_bsd_execve /* Allow override */
int bsd_execve(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_execve(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_execve */

#ifndef custom_bsd_umask /* Allow override */
mode_t bsd_umask(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_umask(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_umask */

#ifndef custom_bsd_chroot /* Allow override */
int bsd_chroot(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_chroot(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_chroot */

#ifndef custom_bsd_msync /* Allow override */
int bsd_msync(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_msync(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_msync */

#ifndef custom_bsd_vfork /* Allow override */
int bsd_vfork(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_vfork();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_vfork */

#ifndef custom_bsd_sbrk /* Allow override */
int bsd_sbrk(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sbrk(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sbrk */

#ifndef custom_bsd_sstk /* Allow override */
int bsd_sstk(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sstk(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sstk */

#ifndef custom_bsd_munmap /* Allow override */
int bsd_munmap(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_munmap(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_munmap */

#ifndef custom_bsd_mprotect /* Allow override */
int bsd_mprotect(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mprotect(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mprotect */

#ifndef custom_bsd_madvise /* Allow override */
int bsd_madvise(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_madvise(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_madvise */

#ifndef custom_bsd_mincore /* Allow override */
int bsd_mincore(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mincore(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mincore */

#ifndef custom_bsd_getgroups /* Allow override */
int bsd_getgroups(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getgroups(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getgroups */

#ifndef custom_bsd_setgroups /* Allow override */
int bsd_setgroups(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setgroups(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setgroups */

#ifndef custom_bsd_getpgrp /* Allow override */
int bsd_getpgrp(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getpgrp();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getpgrp */

#ifndef custom_bsd_setpgid /* Allow override */
int bsd_setpgid(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setpgid(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setpgid */

#ifndef custom_bsd_setitimer /* Allow override */
int bsd_setitimer(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setitimer(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setitimer */

#ifndef custom_bsd_swapon /* Allow override */
int bsd_swapon(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_swapon(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_swapon */

#ifndef custom_bsd_getitimer /* Allow override */
int bsd_getitimer(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getitimer(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getitimer */

#ifndef custom_bsd_getdtablesize /* Allow override */
int bsd_getdtablesize(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getdtablesize();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getdtablesize */

#ifndef custom_bsd_dup2 /* Allow override */
int bsd_dup2(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_dup2(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_dup2 */

#ifndef custom_bsd_fcntl /* Allow override */
int bsd_fcntl(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fcntl(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fcntl */

#ifndef custom_bsd_select /* Allow override */
int bsd_select(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_select(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_select */

#ifndef custom_bsd_fsync /* Allow override */
int bsd_fsync(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fsync(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fsync */

#ifndef custom_bsd_setpriority /* Allow override */
int bsd_setpriority(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setpriority(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setpriority */

#ifndef custom_bsd_socket /* Allow override */
int bsd_socket(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_socket(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_socket */

#ifndef custom_bsd_connect /* Allow override */
int bsd_connect(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_connect(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_connect */

#ifndef custom_bsd_getpriority /* Allow override */
int bsd_getpriority(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getpriority(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getpriority */

#ifndef custom_bsd_bind /* Allow override */
int bsd_bind(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_bind(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_bind */

#ifndef custom_bsd_setsockopt /* Allow override */
int bsd_setsockopt(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setsockopt(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setsockopt */

#ifndef custom_bsd_listen /* Allow override */
int bsd_listen(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_listen(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_listen */

#ifndef custom_bsd_gettimeofday /* Allow override */
int bsd_gettimeofday(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_gettimeofday(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_gettimeofday */

#ifndef custom_bsd_getrusage /* Allow override */
int bsd_getrusage(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getrusage(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getrusage */

#ifndef custom_bsd_getsockopt /* Allow override */
int bsd_getsockopt(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getsockopt(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getsockopt */

#ifndef custom_bsd_readv /* Allow override */
int bsd_readv(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_readv(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_readv */

#ifndef custom_bsd_writev /* Allow override */
int bsd_writev(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_writev(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_writev */

#ifndef custom_bsd_settimeofday /* Allow override */
int bsd_settimeofday(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_settimeofday(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_settimeofday */

#ifndef custom_bsd_fchown /* Allow override */
int bsd_fchown(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fchown(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fchown */

#ifndef custom_bsd_fchmod /* Allow override */
int bsd_fchmod(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fchmod(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fchmod */

#ifndef custom_bsd_setreuid /* Allow override */
int bsd_setreuid(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setreuid(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setreuid */

#ifndef custom_bsd_setregid /* Allow override */
int bsd_setregid(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setregid(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setregid */

#ifndef custom_bsd_rename /* Allow override */
int bsd_rename(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_rename(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_rename */

#ifndef custom_bsd_flock /* Allow override */
int bsd_flock(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_flock(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_flock */

#ifndef custom_bsd_mkfifo /* Allow override */
int bsd_mkfifo(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mkfifo(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mkfifo */

#ifndef custom_bsd_sendto /* Allow override */
ssize_t bsd_sendto(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sendto(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sendto */

#ifndef custom_bsd_shutdown /* Allow override */
int bsd_shutdown(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_shutdown(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_shutdown */

#ifndef custom_bsd_socketpair /* Allow override */
int bsd_socketpair(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_socketpair(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_socketpair */

#ifndef custom_bsd_mkdir /* Allow override */
int bsd_mkdir(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mkdir(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mkdir */

#ifndef custom_bsd_rmdir /* Allow override */
int bsd_rmdir(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_rmdir(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_rmdir */

#ifndef custom_bsd_utimes /* Allow override */
int bsd_utimes(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_utimes(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_utimes */

#ifndef custom_bsd_adjtime /* Allow override */
int bsd_adjtime(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_adjtime(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_adjtime */

#ifndef custom_bsd_setsid /* Allow override */
int bsd_setsid(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setsid();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setsid */

#ifndef custom_bsd_quotactl /* Allow override */
int bsd_quotactl(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_quotactl(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_quotactl */

#ifndef custom_bsd_nlm_syscall /* Allow override */
int bsd_nlm_syscall(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_nlm_syscall(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_nlm_syscall */

#ifndef custom_bsd_nfssvc /* Allow override */
int bsd_nfssvc(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_nfssvc(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_nfssvc */

#ifndef custom_bsd_lgetfh /* Allow override */
int bsd_lgetfh(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_lgetfh(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_lgetfh */

#ifndef custom_bsd_getfh /* Allow override */
int bsd_getfh(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getfh(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getfh */

#ifndef custom_bsd_sysarch /* Allow override */
int bsd_sysarch(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sysarch(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sysarch */

#ifndef custom_bsd_rtprio /* Allow override */
int bsd_rtprio(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_rtprio(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_rtprio */

#ifndef custom_bsd_semsys /* Allow override */
int bsd_semsys(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_semsys(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_semsys */

#ifndef custom_bsd_msgsys /* Allow override */
int bsd_msgsys(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_msgsys(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_msgsys */

#ifndef custom_bsd_shmsys /* Allow override */
int bsd_shmsys(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_shmsys(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_shmsys */

#ifndef custom_bsd_setfib /* Allow override */
int bsd_setfib(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setfib(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setfib */

#ifndef custom_bsd_ntp_adjtime /* Allow override */
int bsd_ntp_adjtime(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ntp_adjtime(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ntp_adjtime */

#ifndef custom_bsd_setgid /* Allow override */
int bsd_setgid(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setgid(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setgid */

#ifndef custom_bsd_setegid /* Allow override */
int bsd_setegid(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setegid(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setegid */

#ifndef custom_bsd_seteuid /* Allow override */
int bsd_seteuid(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_seteuid(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_seteuid */

#ifndef custom_bsd_pathconf /* Allow override */
int bsd_pathconf(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_pathconf(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_pathconf */

#ifndef custom_bsd_fpathconf /* Allow override */
int bsd_fpathconf(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fpathconf(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fpathconf */

#ifndef custom_bsd_getrlimit /* Allow override */
int bsd_getrlimit(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getrlimit(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getrlimit */

#ifndef custom_bsd_setrlimit /* Allow override */
int bsd_setrlimit(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setrlimit(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setrlimit */

#ifndef custom_bsd___sysctl /* Allow override */
int bsd___sysctl(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___sysctl(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___sysctl */

#ifndef custom_bsd_mlock /* Allow override */
int bsd_mlock(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mlock(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mlock */

#ifndef custom_bsd_munlock /* Allow override */
int bsd_munlock(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_munlock(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_munlock */

#ifndef custom_bsd_undelete /* Allow override */
int bsd_undelete(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_undelete(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_undelete */

#ifndef custom_bsd_futimes /* Allow override */
int bsd_futimes(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_futimes(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_futimes */

#ifndef custom_bsd_getpgid /* Allow override */
int bsd_getpgid(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getpgid(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getpgid */

#ifndef custom_bsd_poll /* Allow override */
int bsd_poll(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_poll(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_poll */

#ifndef custom_bsd_semget /* Allow override */
int bsd_semget(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_semget(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_semget */

#ifndef custom_bsd_semop /* Allow override */
int bsd_semop(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_semop(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_semop */

#ifndef custom_bsd_msgget /* Allow override */
int bsd_msgget(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_msgget(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_msgget */

#ifndef custom_bsd_msgsnd /* Allow override */
int bsd_msgsnd(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_msgsnd(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_msgsnd */

#ifndef custom_bsd_msgrcv /* Allow override */
ssize_t bsd_msgrcv(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_msgrcv(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_msgrcv */

#ifndef custom_bsd_shmat /* Allow override */
void * bsd_shmat(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_shmat(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_shmat */

#ifndef custom_bsd_shmdt /* Allow override */
int bsd_shmdt(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_shmdt(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_shmdt */

#ifndef custom_bsd_shmget /* Allow override */
int bsd_shmget(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_shmget(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_shmget */

#ifndef custom_bsd_clock_gettime /* Allow override */
int bsd_clock_gettime(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_clock_gettime(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_clock_gettime */

#ifndef custom_bsd_clock_settime /* Allow override */
int bsd_clock_settime(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_clock_settime(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_clock_settime */

#ifndef custom_bsd_clock_getres /* Allow override */
int bsd_clock_getres(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_clock_getres(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_clock_getres */

#ifndef custom_bsd_ktimer_create /* Allow override */
int bsd_ktimer_create(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ktimer_create(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ktimer_create */

#ifndef custom_bsd_ktimer_delete /* Allow override */
int bsd_ktimer_delete(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ktimer_delete(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ktimer_delete */

#ifndef custom_bsd_ktimer_settime /* Allow override */
int bsd_ktimer_settime(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ktimer_settime(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ktimer_settime */

#ifndef custom_bsd_ktimer_gettime /* Allow override */
int bsd_ktimer_gettime(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ktimer_gettime(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ktimer_gettime */

#ifndef custom_bsd_ktimer_getoverrun /* Allow override */
int bsd_ktimer_getoverrun(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ktimer_getoverrun(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ktimer_getoverrun */

#ifndef custom_bsd_nanosleep /* Allow override */
int bsd_nanosleep(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_nanosleep(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_nanosleep */

#ifndef custom_bsd_ffclock_getcounter /* Allow override */
int bsd_ffclock_getcounter(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ffclock_getcounter(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ffclock_getcounter */

#ifndef custom_bsd_ffclock_setestimate /* Allow override */
int bsd_ffclock_setestimate(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ffclock_setestimate(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ffclock_setestimate */

#ifndef custom_bsd_ffclock_getestimate /* Allow override */
int bsd_ffclock_getestimate(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ffclock_getestimate(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ffclock_getestimate */

#ifndef custom_bsd_clock_nanosleep /* Allow override */
int bsd_clock_nanosleep(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_clock_nanosleep(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_clock_nanosleep */

#ifndef custom_bsd_clock_getcpuclockid2 /* Allow override */
int bsd_clock_getcpuclockid2(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_clock_getcpuclockid2(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_clock_getcpuclockid2 */

#ifndef custom_bsd_ntp_gettime /* Allow override */
int bsd_ntp_gettime(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ntp_gettime(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ntp_gettime */

#ifndef custom_bsd_minherit /* Allow override */
int bsd_minherit(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_minherit(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_minherit */

#ifndef custom_bsd_rfork /* Allow override */
int bsd_rfork(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_rfork(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_rfork */

#ifndef custom_bsd_issetugid /* Allow override */
int bsd_issetugid(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_issetugid();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_issetugid */

#ifndef custom_bsd_lchown /* Allow override */
int bsd_lchown(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_lchown(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_lchown */

#ifndef custom_bsd_aio_read /* Allow override */
int bsd_aio_read(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_aio_read(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_aio_read */

#ifndef custom_bsd_aio_write /* Allow override */
int bsd_aio_write(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_aio_write(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_aio_write */

#ifndef custom_bsd_lio_listio /* Allow override */
int bsd_lio_listio(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_lio_listio(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_lio_listio */

#ifndef custom_bsd_lchmod /* Allow override */
int bsd_lchmod(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_lchmod(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_lchmod */

#ifndef custom_bsd_lutimes /* Allow override */
int bsd_lutimes(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_lutimes(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_lutimes */

#ifndef custom_bsd_preadv /* Allow override */
ssize_t bsd_preadv(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_preadv(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_preadv */

#ifndef custom_bsd_pwritev /* Allow override */
ssize_t bsd_pwritev(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_pwritev(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_pwritev */

#ifndef custom_bsd_fhopen /* Allow override */
int bsd_fhopen(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fhopen(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fhopen */

#ifndef custom_bsd_modnext /* Allow override */
int bsd_modnext(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_modnext(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_modnext */

#ifndef custom_bsd_modstat /* Allow override */
int bsd_modstat(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_modstat(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_modstat */

#ifndef custom_bsd_modfnext /* Allow override */
int bsd_modfnext(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_modfnext(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_modfnext */

#ifndef custom_bsd_modfind /* Allow override */
int bsd_modfind(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_modfind(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_modfind */

#ifndef custom_bsd_kldload /* Allow override */
int bsd_kldload(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kldload(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kldload */

#ifndef custom_bsd_kldunload /* Allow override */
int bsd_kldunload(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kldunload(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kldunload */

#ifndef custom_bsd_kldfind /* Allow override */
int bsd_kldfind(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kldfind(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kldfind */

#ifndef custom_bsd_kldnext /* Allow override */
int bsd_kldnext(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kldnext(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kldnext */

#ifndef custom_bsd_kldstat /* Allow override */
int bsd_kldstat(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kldstat(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kldstat */

#ifndef custom_bsd_kldfirstmod /* Allow override */
int bsd_kldfirstmod(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kldfirstmod(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kldfirstmod */

#ifndef custom_bsd_getsid /* Allow override */
int bsd_getsid(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getsid(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getsid */

#ifndef custom_bsd_setresuid /* Allow override */
int bsd_setresuid(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setresuid(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setresuid */

#ifndef custom_bsd_setresgid /* Allow override */
int bsd_setresgid(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setresgid(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setresgid */

#ifndef custom_bsd_aio_return /* Allow override */
ssize_t bsd_aio_return(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_aio_return(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_aio_return */

#ifndef custom_bsd_aio_suspend /* Allow override */
int bsd_aio_suspend(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_aio_suspend(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_aio_suspend */

#ifndef custom_bsd_aio_cancel /* Allow override */
int bsd_aio_cancel(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_aio_cancel(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_aio_cancel */

#ifndef custom_bsd_aio_error /* Allow override */
int bsd_aio_error(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_aio_error(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_aio_error */

#ifndef custom_bsd_yield /* Allow override */
int bsd_yield(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_yield();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_yield */

#ifndef custom_bsd_mlockall /* Allow override */
int bsd_mlockall(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mlockall(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mlockall */

#ifndef custom_bsd_munlockall /* Allow override */
int bsd_munlockall(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_munlockall();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_munlockall */

#ifndef custom_bsd___getcwd /* Allow override */
int bsd___getcwd(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___getcwd(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___getcwd */

#ifndef custom_bsd_sched_setparam /* Allow override */
int bsd_sched_setparam(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sched_setparam(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sched_setparam */

#ifndef custom_bsd_sched_getparam /* Allow override */
int bsd_sched_getparam(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sched_getparam(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sched_getparam */

#ifndef custom_bsd_sched_setscheduler /* Allow override */
int bsd_sched_setscheduler(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sched_setscheduler(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sched_setscheduler */

#ifndef custom_bsd_sched_getscheduler /* Allow override */
int bsd_sched_getscheduler(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sched_getscheduler(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sched_getscheduler */

#ifndef custom_bsd_sched_yield /* Allow override */
int bsd_sched_yield(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sched_yield();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sched_yield */

#ifndef custom_bsd_sched_get_priority_max /* Allow override */
int bsd_sched_get_priority_max(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sched_get_priority_max(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sched_get_priority_max */

#ifndef custom_bsd_sched_get_priority_min /* Allow override */
int bsd_sched_get_priority_min(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sched_get_priority_min(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sched_get_priority_min */

#ifndef custom_bsd_sched_rr_get_interval /* Allow override */
int bsd_sched_rr_get_interval(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sched_rr_get_interval(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sched_rr_get_interval */

#ifndef custom_bsd_utrace /* Allow override */
int bsd_utrace(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_utrace(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_utrace */

#ifndef custom_bsd_kldsym /* Allow override */
int bsd_kldsym(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kldsym(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kldsym */

#ifndef custom_bsd_jail /* Allow override */
int bsd_jail(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_jail(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_jail */

#ifndef custom_bsd_nnpfs_syscall /* Allow override */
int bsd_nnpfs_syscall(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_nnpfs_syscall(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_nnpfs_syscall */

#ifndef custom_bsd_sigprocmask /* Allow override */
int bsd_sigprocmask(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sigprocmask(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sigprocmask */

#ifndef custom_bsd_sigsuspend /* Allow override */
int bsd_sigsuspend(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sigsuspend(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sigsuspend */

#ifndef custom_bsd_sigpending /* Allow override */
int bsd_sigpending(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sigpending(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sigpending */

#ifndef custom_bsd_sigtimedwait /* Allow override */
int bsd_sigtimedwait(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sigtimedwait(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sigtimedwait */

#ifndef custom_bsd_sigwaitinfo /* Allow override */
int bsd_sigwaitinfo(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sigwaitinfo(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sigwaitinfo */

#ifndef custom_bsd___acl_get_file /* Allow override */
int bsd___acl_get_file(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_get_file(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_get_file */

#ifndef custom_bsd___acl_set_file /* Allow override */
int bsd___acl_set_file(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_set_file(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_set_file */

#ifndef custom_bsd___acl_get_fd /* Allow override */
int bsd___acl_get_fd(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_get_fd(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_get_fd */

#ifndef custom_bsd___acl_set_fd /* Allow override */
int bsd___acl_set_fd(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_set_fd(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_set_fd */

#ifndef custom_bsd___acl_delete_file /* Allow override */
int bsd___acl_delete_file(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_delete_file(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_delete_file */

#ifndef custom_bsd___acl_delete_fd /* Allow override */
int bsd___acl_delete_fd(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_delete_fd(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_delete_fd */

#ifndef custom_bsd___acl_aclcheck_file /* Allow override */
int bsd___acl_aclcheck_file(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_aclcheck_file(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_aclcheck_file */

#ifndef custom_bsd___acl_aclcheck_fd /* Allow override */
int bsd___acl_aclcheck_fd(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_aclcheck_fd(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_aclcheck_fd */

#ifndef custom_bsd_extattrctl /* Allow override */
int bsd_extattrctl(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattrctl(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattrctl */

#ifndef custom_bsd_extattr_set_file /* Allow override */
ssize_t bsd_extattr_set_file(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_set_file(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_set_file */

#ifndef custom_bsd_extattr_get_file /* Allow override */
ssize_t bsd_extattr_get_file(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_get_file(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_get_file */

#ifndef custom_bsd_extattr_delete_file /* Allow override */
int bsd_extattr_delete_file(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_delete_file(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_delete_file */

#ifndef custom_bsd_aio_waitcomplete /* Allow override */
ssize_t bsd_aio_waitcomplete(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_aio_waitcomplete(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_aio_waitcomplete */

#ifndef custom_bsd_getresuid /* Allow override */
int bsd_getresuid(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getresuid(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getresuid */

#ifndef custom_bsd_getresgid /* Allow override */
int bsd_getresgid(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getresgid(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getresgid */

#ifndef custom_bsd_kqueue /* Allow override */
int bsd_kqueue(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kqueue();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kqueue */

#ifndef custom_bsd_extattr_set_fd /* Allow override */
ssize_t bsd_extattr_set_fd(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_set_fd(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_set_fd */

#ifndef custom_bsd_extattr_get_fd /* Allow override */
ssize_t bsd_extattr_get_fd(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_get_fd(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_get_fd */

#ifndef custom_bsd_extattr_delete_fd /* Allow override */
int bsd_extattr_delete_fd(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_delete_fd(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_delete_fd */

#ifndef custom_bsd___setugid /* Allow override */
int bsd___setugid(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___setugid(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___setugid */

#ifndef custom_bsd_eaccess /* Allow override */
int bsd_eaccess(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_eaccess(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_eaccess */

#ifndef custom_bsd_afs3_syscall /* Allow override */
int bsd_afs3_syscall(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6, abi_long arg7)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_afs3_syscall(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_afs3_syscall */

#ifndef custom_bsd_nmount /* Allow override */
int bsd_nmount(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_nmount(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_nmount */

#ifndef custom_bsd___mac_get_proc /* Allow override */
int bsd___mac_get_proc(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___mac_get_proc(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___mac_get_proc */

#ifndef custom_bsd___mac_set_proc /* Allow override */
int bsd___mac_set_proc(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___mac_set_proc(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___mac_set_proc */

#ifndef custom_bsd___mac_get_fd /* Allow override */
int bsd___mac_get_fd(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___mac_get_fd(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___mac_get_fd */

#ifndef custom_bsd___mac_get_file /* Allow override */
int bsd___mac_get_file(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___mac_get_file(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___mac_get_file */

#ifndef custom_bsd___mac_set_fd /* Allow override */
int bsd___mac_set_fd(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___mac_set_fd(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___mac_set_fd */

#ifndef custom_bsd___mac_set_file /* Allow override */
int bsd___mac_set_file(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___mac_set_file(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___mac_set_file */

#ifndef custom_bsd_kenv /* Allow override */
int bsd_kenv(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kenv(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kenv */

#ifndef custom_bsd_lchflags /* Allow override */
int bsd_lchflags(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_lchflags(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_lchflags */

#ifndef custom_bsd_uuidgen /* Allow override */
int bsd_uuidgen(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_uuidgen(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_uuidgen */

#ifndef custom_bsd_sendfile /* Allow override */
int bsd_sendfile(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6, abi_long arg7)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sendfile(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sendfile */

#ifndef custom_bsd_mac_syscall /* Allow override */
int bsd_mac_syscall(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mac_syscall(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mac_syscall */

#ifndef custom_bsd_ksem_close /* Allow override */
int bsd_ksem_close(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ksem_close(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ksem_close */

#ifndef custom_bsd_ksem_post /* Allow override */
int bsd_ksem_post(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ksem_post(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ksem_post */

#ifndef custom_bsd_ksem_wait /* Allow override */
int bsd_ksem_wait(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ksem_wait(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ksem_wait */

#ifndef custom_bsd_ksem_trywait /* Allow override */
int bsd_ksem_trywait(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ksem_trywait(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ksem_trywait */

#ifndef custom_bsd_ksem_init /* Allow override */
int bsd_ksem_init(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ksem_init(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ksem_init */

#ifndef custom_bsd_ksem_open /* Allow override */
int bsd_ksem_open(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ksem_open(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ksem_open */

#ifndef custom_bsd_ksem_unlink /* Allow override */
int bsd_ksem_unlink(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ksem_unlink(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ksem_unlink */

#ifndef custom_bsd_ksem_getvalue /* Allow override */
int bsd_ksem_getvalue(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ksem_getvalue(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ksem_getvalue */

#ifndef custom_bsd_ksem_destroy /* Allow override */
int bsd_ksem_destroy(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ksem_destroy(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ksem_destroy */

#ifndef custom_bsd___mac_get_pid /* Allow override */
int bsd___mac_get_pid(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___mac_get_pid(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___mac_get_pid */

#ifndef custom_bsd___mac_get_link /* Allow override */
int bsd___mac_get_link(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___mac_get_link(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___mac_get_link */

#ifndef custom_bsd___mac_set_link /* Allow override */
int bsd___mac_set_link(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___mac_set_link(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___mac_set_link */

#ifndef custom_bsd_extattr_set_link /* Allow override */
ssize_t bsd_extattr_set_link(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_set_link(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_set_link */

#ifndef custom_bsd_extattr_get_link /* Allow override */
ssize_t bsd_extattr_get_link(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_get_link(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_get_link */

#ifndef custom_bsd_extattr_delete_link /* Allow override */
int bsd_extattr_delete_link(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_delete_link(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_delete_link */

#ifndef custom_bsd___mac_execve /* Allow override */
int bsd___mac_execve(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___mac_execve(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___mac_execve */

#ifndef custom_bsd_sigaction /* Allow override */
int bsd_sigaction(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sigaction(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sigaction */

#ifndef custom_bsd_sigreturn /* Allow override */
int bsd_sigreturn(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sigreturn(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sigreturn */

#ifndef custom_bsd_getcontext /* Allow override */
int bsd_getcontext(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getcontext(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getcontext */

#ifndef custom_bsd_setcontext /* Allow override */
int bsd_setcontext(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setcontext(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setcontext */

#ifndef custom_bsd_swapcontext /* Allow override */
int bsd_swapcontext(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_swapcontext(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_swapcontext */

#ifndef custom_bsd___acl_get_link /* Allow override */
int bsd___acl_get_link(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_get_link(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_get_link */

#ifndef custom_bsd___acl_set_link /* Allow override */
int bsd___acl_set_link(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_set_link(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_set_link */

#ifndef custom_bsd___acl_delete_link /* Allow override */
int bsd___acl_delete_link(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_delete_link(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_delete_link */

#ifndef custom_bsd___acl_aclcheck_link /* Allow override */
int bsd___acl_aclcheck_link(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___acl_aclcheck_link(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___acl_aclcheck_link */

#ifndef custom_bsd_sigwait /* Allow override */
int bsd_sigwait(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sigwait(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sigwait */

#ifndef custom_bsd_thr_create /* Allow override */
int bsd_thr_create(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_thr_create(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_thr_create */

#ifndef custom_bsd_thr_exit /* Allow override */
void bsd_thr_exit(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	do_bsd_thr_exit(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_thr_exit */

#ifndef custom_bsd_thr_self /* Allow override */
int bsd_thr_self(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_thr_self(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_thr_self */

#ifndef custom_bsd_thr_kill /* Allow override */
int bsd_thr_kill(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_thr_kill(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_thr_kill */

#ifndef custom_bsd_jail_attach /* Allow override */
int bsd_jail_attach(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_jail_attach(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_jail_attach */

#ifndef custom_bsd_extattr_list_fd /* Allow override */
ssize_t bsd_extattr_list_fd(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_list_fd(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_list_fd */

#ifndef custom_bsd_extattr_list_file /* Allow override */
ssize_t bsd_extattr_list_file(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_list_file(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_list_file */

#ifndef custom_bsd_extattr_list_link /* Allow override */
ssize_t bsd_extattr_list_link(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_extattr_list_link(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_extattr_list_link */

#ifndef custom_bsd_ksem_timedwait /* Allow override */
int bsd_ksem_timedwait(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ksem_timedwait(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ksem_timedwait */

#ifndef custom_bsd_thr_suspend /* Allow override */
int bsd_thr_suspend(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_thr_suspend(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_thr_suspend */

#ifndef custom_bsd_thr_wake /* Allow override */
int bsd_thr_wake(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_thr_wake(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_thr_wake */

#ifndef custom_bsd_kldunloadf /* Allow override */
int bsd_kldunloadf(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kldunloadf(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kldunloadf */

#ifndef custom_bsd_audit /* Allow override */
int bsd_audit(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_audit(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_audit */

#ifndef custom_bsd_auditon /* Allow override */
int bsd_auditon(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_auditon(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_auditon */

#ifndef custom_bsd_getauid /* Allow override */
int bsd_getauid(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getauid(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getauid */

#ifndef custom_bsd_setauid /* Allow override */
int bsd_setauid(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setauid(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setauid */

#ifndef custom_bsd_getaudit /* Allow override */
int bsd_getaudit(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getaudit(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getaudit */

#ifndef custom_bsd_setaudit /* Allow override */
int bsd_setaudit(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setaudit(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setaudit */

#ifndef custom_bsd_getaudit_addr /* Allow override */
int bsd_getaudit_addr(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getaudit_addr(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getaudit_addr */

#ifndef custom_bsd_setaudit_addr /* Allow override */
int bsd_setaudit_addr(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setaudit_addr(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setaudit_addr */

#ifndef custom_bsd_auditctl /* Allow override */
int bsd_auditctl(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_auditctl(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_auditctl */

#ifndef custom_bsd__umtx_op /* Allow override */
int bsd__umtx_op(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd__umtx_op(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd__umtx_op */

#ifndef custom_bsd_thr_new /* Allow override */
int bsd_thr_new(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_thr_new(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_thr_new */

#ifndef custom_bsd_sigqueue /* Allow override */
int bsd_sigqueue(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sigqueue(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sigqueue */

#ifndef custom_bsd_kmq_open /* Allow override */
int bsd_kmq_open(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kmq_open(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kmq_open */

#ifndef custom_bsd_kmq_setattr /* Allow override */
int bsd_kmq_setattr(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kmq_setattr(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kmq_setattr */

#ifndef custom_bsd_kmq_timedreceive /* Allow override */
int bsd_kmq_timedreceive(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kmq_timedreceive(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kmq_timedreceive */

#ifndef custom_bsd_kmq_timedsend /* Allow override */
int bsd_kmq_timedsend(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kmq_timedsend(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kmq_timedsend */

#ifndef custom_bsd_kmq_notify /* Allow override */
int bsd_kmq_notify(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kmq_notify(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kmq_notify */

#ifndef custom_bsd_kmq_unlink /* Allow override */
int bsd_kmq_unlink(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kmq_unlink(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kmq_unlink */

#ifndef custom_bsd_abort2 /* Allow override */
void bsd_abort2(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	do_bsd_abort2(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_abort2 */

#ifndef custom_bsd_thr_set_name /* Allow override */
int bsd_thr_set_name(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_thr_set_name(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_thr_set_name */

#ifndef custom_bsd_aio_fsync /* Allow override */
int bsd_aio_fsync(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_aio_fsync(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_aio_fsync */

#ifndef custom_bsd_rtprio_thread /* Allow override */
int bsd_rtprio_thread(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_rtprio_thread(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_rtprio_thread */

#ifndef custom_bsd_sctp_peeloff /* Allow override */
int bsd_sctp_peeloff(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sctp_peeloff(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sctp_peeloff */

#ifndef custom_bsd_sctp_generic_sendmsg /* Allow override */
int bsd_sctp_generic_sendmsg(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6, abi_long arg7)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sctp_generic_sendmsg(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sctp_generic_sendmsg */

#ifndef custom_bsd_sctp_generic_sendmsg_iov /* Allow override */
int bsd_sctp_generic_sendmsg_iov(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6, abi_long arg7)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sctp_generic_sendmsg_iov(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sctp_generic_sendmsg_iov */

#ifndef custom_bsd_sctp_generic_recvmsg /* Allow override */
int bsd_sctp_generic_recvmsg(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6, abi_long arg7)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sctp_generic_recvmsg(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sctp_generic_recvmsg */

#ifndef custom_bsd_pread /* Allow override */
ssize_t bsd_pread(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_pread(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_pread */

#ifndef custom_bsd_pwrite /* Allow override */
ssize_t bsd_pwrite(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_pwrite(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_pwrite */

#ifndef custom_bsd_mmap /* Allow override */
void * bsd_mmap(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mmap(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mmap */

#ifndef custom_bsd_lseek /* Allow override */
off_t bsd_lseek(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_lseek(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_lseek */

#ifndef custom_bsd_truncate /* Allow override */
int bsd_truncate(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_truncate(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_truncate */

#ifndef custom_bsd_ftruncate /* Allow override */
int bsd_ftruncate(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ftruncate(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ftruncate */

#ifndef custom_bsd_thr_kill2 /* Allow override */
int bsd_thr_kill2(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_thr_kill2(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_thr_kill2 */

#ifndef custom_bsd_shm_unlink /* Allow override */
int bsd_shm_unlink(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_shm_unlink(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_shm_unlink */

#ifndef custom_bsd_cpuset /* Allow override */
int bsd_cpuset(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cpuset(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cpuset */

#ifndef custom_bsd_cpuset_setid /* Allow override */
int bsd_cpuset_setid(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cpuset_setid(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cpuset_setid */

#ifndef custom_bsd_cpuset_getid /* Allow override */
int bsd_cpuset_getid(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cpuset_getid(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cpuset_getid */

#ifndef custom_bsd_cpuset_getaffinity /* Allow override */
int bsd_cpuset_getaffinity(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cpuset_getaffinity(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cpuset_getaffinity */

#ifndef custom_bsd_cpuset_setaffinity /* Allow override */
int bsd_cpuset_setaffinity(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cpuset_setaffinity(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cpuset_setaffinity */

#ifndef custom_bsd_faccessat /* Allow override */
int bsd_faccessat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_faccessat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_faccessat */

#ifndef custom_bsd_fchmodat /* Allow override */
int bsd_fchmodat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fchmodat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fchmodat */

#ifndef custom_bsd_fchownat /* Allow override */
int bsd_fchownat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fchownat(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fchownat */

#ifndef custom_bsd_fexecve /* Allow override */
int bsd_fexecve(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fexecve(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fexecve */

#ifndef custom_bsd_futimesat /* Allow override */
int bsd_futimesat(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_futimesat(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_futimesat */

#ifndef custom_bsd_linkat /* Allow override */
int bsd_linkat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_linkat(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_linkat */

#ifndef custom_bsd_mkdirat /* Allow override */
int bsd_mkdirat(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mkdirat(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mkdirat */

#ifndef custom_bsd_mkfifoat /* Allow override */
int bsd_mkfifoat(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mkfifoat(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mkfifoat */

#ifndef custom_bsd_openat /* Allow override */
int bsd_openat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_openat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_openat */

#ifndef custom_bsd_readlinkat /* Allow override */
ssize_t bsd_readlinkat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_readlinkat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_readlinkat */

#ifndef custom_bsd_renameat /* Allow override */
int bsd_renameat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_renameat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_renameat */

#ifndef custom_bsd_symlinkat /* Allow override */
int bsd_symlinkat(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_symlinkat(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_symlinkat */

#ifndef custom_bsd_unlinkat /* Allow override */
int bsd_unlinkat(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_unlinkat(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_unlinkat */

#ifndef custom_bsd_posix_openpt /* Allow override */
int bsd_posix_openpt(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_posix_openpt(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_posix_openpt */

#ifndef custom_bsd_gssd_syscall /* Allow override */
int bsd_gssd_syscall(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_gssd_syscall(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_gssd_syscall */

#ifndef custom_bsd_jail_get /* Allow override */
int bsd_jail_get(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_jail_get(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_jail_get */

#ifndef custom_bsd_jail_set /* Allow override */
int bsd_jail_set(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_jail_set(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_jail_set */

#ifndef custom_bsd_jail_remove /* Allow override */
int bsd_jail_remove(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_jail_remove(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_jail_remove */

#ifndef custom_bsd___semctl /* Allow override */
int bsd___semctl(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___semctl(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___semctl */

#ifndef custom_bsd_msgctl /* Allow override */
int bsd_msgctl(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_msgctl(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_msgctl */

#ifndef custom_bsd_shmctl /* Allow override */
int bsd_shmctl(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_shmctl(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_shmctl */

#ifndef custom_bsd_lpathconf /* Allow override */
int bsd_lpathconf(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_lpathconf(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_lpathconf */

#ifndef custom_bsd___cap_rights_get /* Allow override */
int bsd___cap_rights_get(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___cap_rights_get(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___cap_rights_get */

#ifndef custom_bsd_cap_enter /* Allow override */
int bsd_cap_enter(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cap_enter();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cap_enter */

#ifndef custom_bsd_cap_getmode /* Allow override */
int bsd_cap_getmode(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cap_getmode(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cap_getmode */

#ifndef custom_bsd_pdfork /* Allow override */
int bsd_pdfork(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_pdfork(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_pdfork */

#ifndef custom_bsd_pdkill /* Allow override */
int bsd_pdkill(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_pdkill(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_pdkill */

#ifndef custom_bsd_pdgetpid /* Allow override */
int bsd_pdgetpid(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_pdgetpid(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_pdgetpid */

#ifndef custom_bsd_pselect /* Allow override */
int bsd_pselect(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_pselect(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_pselect */

#ifndef custom_bsd_getloginclass /* Allow override */
int bsd_getloginclass(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getloginclass(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getloginclass */

#ifndef custom_bsd_setloginclass /* Allow override */
int bsd_setloginclass(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_setloginclass(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_setloginclass */

#ifndef custom_bsd_rctl_get_racct /* Allow override */
int bsd_rctl_get_racct(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_rctl_get_racct(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_rctl_get_racct */

#ifndef custom_bsd_rctl_get_rules /* Allow override */
int bsd_rctl_get_rules(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_rctl_get_rules(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_rctl_get_rules */

#ifndef custom_bsd_rctl_get_limits /* Allow override */
int bsd_rctl_get_limits(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_rctl_get_limits(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_rctl_get_limits */

#ifndef custom_bsd_rctl_add_rule /* Allow override */
int bsd_rctl_add_rule(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_rctl_add_rule(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_rctl_add_rule */

#ifndef custom_bsd_rctl_remove_rule /* Allow override */
int bsd_rctl_remove_rule(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_rctl_remove_rule(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_rctl_remove_rule */

#ifndef custom_bsd_posix_fallocate /* Allow override */
int bsd_posix_fallocate(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_posix_fallocate(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_posix_fallocate */

#ifndef custom_bsd_posix_fadvise /* Allow override */
int bsd_posix_fadvise(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_posix_fadvise(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_posix_fadvise */

#ifndef custom_bsd_wait6 /* Allow override */
int bsd_wait6(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_wait6(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_wait6 */

#ifndef custom_bsd_cap_rights_limit /* Allow override */
int bsd_cap_rights_limit(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cap_rights_limit(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cap_rights_limit */

#ifndef custom_bsd_cap_ioctls_limit /* Allow override */
int bsd_cap_ioctls_limit(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cap_ioctls_limit(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cap_ioctls_limit */

#ifndef custom_bsd_cap_ioctls_get /* Allow override */
ssize_t bsd_cap_ioctls_get(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cap_ioctls_get(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cap_ioctls_get */

#ifndef custom_bsd_cap_fcntls_limit /* Allow override */
int bsd_cap_fcntls_limit(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cap_fcntls_limit(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cap_fcntls_limit */

#ifndef custom_bsd_cap_fcntls_get /* Allow override */
int bsd_cap_fcntls_get(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cap_fcntls_get(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cap_fcntls_get */

#ifndef custom_bsd_bindat /* Allow override */
int bsd_bindat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_bindat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_bindat */

#ifndef custom_bsd_connectat /* Allow override */
int bsd_connectat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_connectat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_connectat */

#ifndef custom_bsd_chflagsat /* Allow override */
int bsd_chflagsat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_chflagsat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_chflagsat */

#ifndef custom_bsd_accept4 /* Allow override */
int bsd_accept4(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_accept4(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_accept4 */

#ifndef custom_bsd_pipe2 /* Allow override */
int bsd_pipe2(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_pipe2(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_pipe2 */

#ifndef custom_bsd_aio_mlock /* Allow override */
int bsd_aio_mlock(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_aio_mlock(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_aio_mlock */

#ifndef custom_bsd_procctl /* Allow override */
int bsd_procctl(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_procctl(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_procctl */

#ifndef custom_bsd_ppoll /* Allow override */
int bsd_ppoll(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_ppoll(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_ppoll */

#ifndef custom_bsd_futimens /* Allow override */
int bsd_futimens(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_futimens(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_futimens */

#ifndef custom_bsd_utimensat /* Allow override */
int bsd_utimensat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_utimensat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_utimensat */

#ifndef custom_bsd_fdatasync /* Allow override */
int bsd_fdatasync(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fdatasync(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fdatasync */

#ifndef custom_bsd_fstat /* Allow override */
int bsd_fstat(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fstat(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fstat */

#ifndef custom_bsd_fstatat /* Allow override */
int bsd_fstatat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fstatat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fstatat */

#ifndef custom_bsd_fhstat /* Allow override */
int bsd_fhstat(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fhstat(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fhstat */

#ifndef custom_bsd_getdirentries /* Allow override */
ssize_t bsd_getdirentries(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getdirentries(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getdirentries */

#ifndef custom_bsd_statfs /* Allow override */
int bsd_statfs(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_statfs(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_statfs */

#ifndef custom_bsd_fstatfs /* Allow override */
int bsd_fstatfs(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fstatfs(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fstatfs */

#ifndef custom_bsd_getfsstat /* Allow override */
int bsd_getfsstat(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getfsstat(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getfsstat */

#ifndef custom_bsd_fhstatfs /* Allow override */
int bsd_fhstatfs(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fhstatfs(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fhstatfs */

#ifndef custom_bsd_mknodat /* Allow override */
int bsd_mknodat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_mknodat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_mknodat */

#ifndef custom_bsd_kevent /* Allow override */
int bsd_kevent(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kevent(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kevent */

#ifndef custom_bsd_cpuset_getdomain /* Allow override */
int bsd_cpuset_getdomain(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cpuset_getdomain(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cpuset_getdomain */

#ifndef custom_bsd_cpuset_setdomain /* Allow override */
int bsd_cpuset_setdomain(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_cpuset_setdomain(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_cpuset_setdomain */

#ifndef custom_bsd_getrandom /* Allow override */
int bsd_getrandom(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getrandom(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getrandom */

#ifndef custom_bsd_getfhat /* Allow override */
int bsd_getfhat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_getfhat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_getfhat */

#ifndef custom_bsd_fhlink /* Allow override */
int bsd_fhlink(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fhlink(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fhlink */

#ifndef custom_bsd_fhlinkat /* Allow override */
int bsd_fhlinkat(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fhlinkat(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fhlinkat */

#ifndef custom_bsd_fhreadlink /* Allow override */
int bsd_fhreadlink(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fhreadlink(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fhreadlink */

#ifndef custom_bsd_funlinkat /* Allow override */
int bsd_funlinkat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_funlinkat(arg1, arg2, arg3, arg4);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_funlinkat */

#ifndef custom_bsd_copy_file_range /* Allow override */
ssize_t bsd_copy_file_range(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_copy_file_range(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_copy_file_range */

#ifndef custom_bsd___sysctlbyname /* Allow override */
int bsd___sysctlbyname(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5, abi_long arg6)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___sysctlbyname(arg1, arg2, arg3, arg4, arg5, arg6);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___sysctlbyname */

#ifndef custom_bsd_shm_open2 /* Allow override */
int bsd_shm_open2(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_shm_open2(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_shm_open2 */

#ifndef custom_bsd_shm_rename /* Allow override */
int bsd_shm_rename(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_shm_rename(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_shm_rename */

#ifndef custom_bsd_sigfastblock /* Allow override */
int bsd_sigfastblock(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sigfastblock(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sigfastblock */

#ifndef custom_bsd___realpathat /* Allow override */
int bsd___realpathat(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___realpathat(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___realpathat */

#ifndef custom_bsd_close_range /* Allow override */
int bsd_close_range(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_close_range(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_close_range */

#ifndef custom_bsd_rpctls_syscall /* Allow override */
int bsd_rpctls_syscall(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_rpctls_syscall(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_rpctls_syscall */

#ifndef custom_bsd___specialfd /* Allow override */
int bsd___specialfd(abi_long arg1, abi_long arg2, abi_long arg3)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd___specialfd(arg1, arg2, arg3);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd___specialfd */

#ifndef custom_bsd_aio_writev /* Allow override */
int bsd_aio_writev(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_aio_writev(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_aio_writev */

#ifndef custom_bsd_aio_readv /* Allow override */
int bsd_aio_readv(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_aio_readv(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_aio_readv */

#ifndef custom_bsd_fspacectl /* Allow override */
int bsd_fspacectl(abi_long arg1, abi_long arg2, abi_long arg3, abi_long arg4, abi_long arg5)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_fspacectl(arg1, arg2, arg3, arg4, arg5);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_fspacectl */

#ifndef custom_bsd_sched_getcpu /* Allow override */
int bsd_sched_getcpu(void)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_sched_getcpu();
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_sched_getcpu */

#ifndef custom_bsd_swapoff /* Allow override */
int bsd_swapoff(abi_long arg1, abi_long arg2)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_swapoff(arg1, arg2);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_swapoff */

#ifndef custom_bsd_kqueue1 /* Allow override */
int bsd_kqueue1(abi_long arg1)
{
	/* Convert IN parameters from target to host */
	/*     scalors via g2h_xxx functions to map errno etc if needed */
	/*     structures using t2h_xxx (which does user lock + g2h + user unlock */
	/*     lock user any strings */
	/*     do any pathname mapping */
	/* Stage any OUT parameters */
	/*     host storage for system call */
	/*     lock_user output buffers */
	/*     for any 'optional' out parameter, do foop dance */
	return do_bsd_kqueue1(arg1);
	/* Unlock strings */
	/* Unlock buffers */
	/* Convert any OUT parameters from host to target */
	/*     scalors via h2g_xxx functions */
	/*     structures using h2t_xxx functions (lock h2g unlock) */
	/*     careful to optionally do optional parameters */
}
#endif /* custom_bsd_kqueue1 */

