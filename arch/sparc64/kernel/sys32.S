/* $Id: sys32.S,v 1.12 2000/03/24 04:17:37 davem Exp $
 * sys32.S: I-cache tricks for 32-bit compatability layer simple
 *          conversions.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1998 Jakub Jelinek   (jj@ultra.linux.cz)
 */

#include <asm/errno.h>

/* NOTE: call as jump breaks return stack, we have to avoid that */

	.text

	.align		32
	.globl		sys32_mmap
sys32_mmap:
	srl		%o4, 0, %o4
	sethi		%hi(sys_mmap), %g1
	jmpl		%g1 + %lo(sys_mmap), %g0
	 srl		%o5, 0, %o5

	.align		32
	.globl		sys32_lseek
	.globl		sys32_chmod, sys32_mknod
sys32_lseek:
	sra		%o1, 0, %o1
	sethi		%hi(sys_lseek), %g1
	jmpl		%g1 + %lo(sys_lseek), %g0
	 nop
sys32_chmod:
	sethi		%hi(0xffff), %g2
	sethi		%hi(sys_chmod), %g1
	orcc		%g2, %lo(0xffff), %g2
	jmpl		%g1 + %lo(sys_chmod), %g0
	 and		%o1, %g2, %o1
sys32_mknod:
	sethi		%hi(0xffff), %g2
	sethi		%hi(sys_mknod), %g1
	orcc		%g2, %lo(0xffff), %g2
	jmpl		%g1 + %lo(sys_mknod), %g0
	 and		%o2, %g2, %o2

	.align		32
	.globl		sys32_sendto, sys32_recvfrom
sys32_sendto:
	sethi		%hi(sys_sendto), %g1
	jmpl		%g1 + %lo(sys_sendto), %g0
	 srl		%o4, 0, %o4
sys32_recvfrom:
	srl		%o4, 0, %o4
	sethi		%hi(sys_recvfrom), %g1
	jmpl		%g1 + %lo(sys_recvfrom), %g0
	 srl		%o5, 0, %o5

	.globl		sys32_bdflush
sys32_bdflush:
	sethi		%hi(sys_bdflush), %g1
	jmpl		%g1 + %lo(sys_bdflush), %g0
	 sra		%o1, 0, %o1

	.align		32
	.globl		sys32_mmap2
sys32_mmap2:
	srl		%o4, 0, %o4
	sethi		%hi(sys_mmap), %g1
	srl		%o5, 0, %o5
	jmpl		%g1 + %lo(sys_mmap), %g0
	 sllx		%o5, 12, %o5

	.align		32
	.globl		sys32_socketcall
sys32_socketcall:	/* %o0=call, %o1=args */
	cmp		%o0, 1
	bl,pn		%xcc, do_einval
	 cmp		%o0, 17
	bg,pn		%xcc, do_einval
	 sub		%o0, 1, %o0
	sllx		%o0, 5, %o0
	sethi		%hi(__socketcall_table_begin), %g2
	or		%g2, %lo(__socketcall_table_begin), %g2
	jmpl		%g2 + %o0, %g0
	 nop

	/* Each entry is exactly 32 bytes. */
	.align		32
__socketcall_table_begin:
do_sys_socket: /* sys_socket(int, int, int) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys_socket), %g1
	ldswa		[%o1 + 0x8] %asi, %o2
	jmpl		%g1 + %lo(sys_socket), %g0
	 ldswa		[%o1 + 0x4] %asi, %o1
	nop
	nop
	nop
do_sys_bind: /* sys_bind(int fd, struct sockaddr *, int) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys_bind), %g1
	ldswa		[%o1 + 0x8] %asi, %o2
	jmpl		%g1 + %lo(sys_bind), %g0
	 lduwa		[%o1 + 0x4] %asi, %o1
	nop
	nop
	nop
do_sys_connect: /* sys_connect(int, struct sockaddr *, int) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys_connect), %g1
	ldswa		[%o1 + 0x8] %asi, %o2
	jmpl		%g1 + %lo(sys_connect), %g0
	 lduwa		[%o1 + 0x4] %asi, %o1
	nop
	nop
	nop
do_sys_listen: /* sys_listen(int, int) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys_listen), %g1
	jmpl		%g1 + %lo(sys_listen), %g0
	 ldswa		[%o1 + 0x4] %asi, %o1
	nop
	nop
	nop
	nop
do_sys_accept: /* sys_accept(int, struct sockaddr *, int *) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys_accept), %g1
	lduwa		[%o1 + 0x8] %asi, %o2
	jmpl		%g1 + %lo(sys_accept), %g0
	 lduwa		[%o1 + 0x4] %asi, %o1
	nop
	nop
	nop
do_sys_getsockname: /* sys_getsockname(int, struct sockaddr *, int *) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys_getsockname), %g1
	lduwa		[%o1 + 0x8] %asi, %o2
	jmpl		%g1 + %lo(sys_getsockname), %g0
	 lduwa		[%o1 + 0x4] %asi, %o1
	nop
	nop
	nop
do_sys_getpeername: /* sys_getpeername(int, struct sockaddr *, int *) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys_getpeername), %g1
	lduwa		[%o1 + 0x8] %asi, %o2
	jmpl		%g1 + %lo(sys_getpeername), %g0
	 lduwa		[%o1 + 0x4] %asi, %o1
	nop
	nop
	nop
do_sys_socketpair: /* sys_socketpair(int, int, int, int *) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys_socketpair), %g1
	ldswa		[%o1 + 0x8] %asi, %o2
	lduwa		[%o1 + 0xc] %asi, %o3
	jmpl		%g1 + %lo(sys_socketpair), %g0
	 ldswa		[%o1 + 0x4] %asi, %o1
	nop
	nop
do_sys_send: /* sys_send(int, void *, size_t, unsigned int) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys_send), %g1
	lduwa		[%o1 + 0x8] %asi, %o2
	lduwa		[%o1 + 0xc] %asi, %o3
	jmpl		%g1 + %lo(sys_send), %g0
	 lduwa		[%o1 + 0x4] %asi, %o1
	nop
	nop
do_sys_recv: /* sys_recv(int, void *, size_t, unsigned int) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys_recv), %g1
	lduwa		[%o1 + 0x8] %asi, %o2
	lduwa		[%o1 + 0xc] %asi, %o3
	jmpl		%g1 + %lo(sys_recv), %g0
	 lduwa		[%o1 + 0x4] %asi, %o1
	nop
	nop
do_sys_sendto: /* sys32_sendto(int, u32, __kernel_size_t32, unsigned int, u32, int) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys32_sendto), %g1
	lduwa		[%o1 + 0x8] %asi, %o2
	lduwa		[%o1 + 0xc] %asi, %o3
	lduwa		[%o1 + 0x10] %asi, %o4
	ldswa		[%o1 + 0x14] %asi, %o5
	jmpl		%g1 + %lo(sys32_sendto), %g0
	 lduwa		[%o1 + 0x4] %asi, %o1
do_sys_recvfrom: /* sys32_recvfrom(int, u32, __kernel_size_t32, unsigned int, u32, u32) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys32_recvfrom), %g1
	lduwa		[%o1 + 0x8] %asi, %o2
	lduwa		[%o1 + 0xc] %asi, %o3
	lduwa		[%o1 + 0x10] %asi, %o4
	lduwa		[%o1 + 0x14] %asi, %o5
	jmpl		%g1 + %lo(sys32_recvfrom), %g0
	 lduwa		[%o1 + 0x4] %asi, %o1
do_sys_shutdown: /* sys_shutdown(int, int) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys_shutdown), %g1
	jmpl		%g1 + %lo(sys_shutdown), %g0
	 ldswa		[%o1 + 0x4] %asi, %o1
	nop
	nop
	nop
	nop
do_sys_setsockopt: /* sys32_setsockopt(int, int, int, char *, int) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys32_setsockopt), %g1
	ldswa		[%o1 + 0x8] %asi, %o2
	lduwa		[%o1 + 0xc] %asi, %o3
	ldswa		[%o1 + 0x10] %asi, %o4
	jmpl		%g1 + %lo(sys32_setsockopt), %g0
	 ldswa		[%o1 + 0x4] %asi, %o1
	nop
do_sys_getsockopt: /* sys32_getsockopt(int, int, int, u32, u32) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys32_getsockopt), %g1
	ldswa		[%o1 + 0x8] %asi, %o2
	lduwa		[%o1 + 0xc] %asi, %o3
	lduwa		[%o1 + 0x10] %asi, %o4
	jmpl		%g1 + %lo(sys32_getsockopt), %g0
	 ldswa		[%o1 + 0x4] %asi, %o1
	nop
do_sys_sendmsg: /* sys32_sendmsg(int, struct msghdr32 *, unsigned int) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys32_sendmsg), %g1
	lduwa		[%o1 + 0x8] %asi, %o2
	jmpl		%g1 + %lo(sys32_sendmsg), %g0
	 lduwa		[%o1 + 0x4] %asi, %o1
	nop
	nop
	nop
do_sys_recvmsg: /* sys32_recvmsg(int, struct msghdr32 *, unsigned int) */
	ldswa		[%o1 + 0x0] %asi, %o0
	sethi		%hi(sys32_recvmsg), %g1
	lduwa		[%o1 + 0x8] %asi, %o2
	jmpl		%g1 + %lo(sys32_recvmsg), %g0
	 lduwa		[%o1 + 0x4] %asi, %o1
	nop
	nop
	nop
__socketcall_table_end:

do_einval:
	retl
	 mov		-EINVAL, %o0
do_efault:
	retl
	 mov		-EFAULT, %o0

	.section	__ex_table
	.align		4
	.word		__socketcall_table_begin, 0, __socketcall_table_end, do_efault
	.previous
