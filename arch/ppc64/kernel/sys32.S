/* 
 * sys32.S: I-cache tricks for 32-bit compatability layer simple
 *          conversions.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1998 Jakub Jelinek   (jj@ultra.linux.cz)
 * Copyright (C) 2000 Ken Aaker (kdaaker@rchland.vnet.ibm.com)
 * For PPC ABI convention is parms in Regs 3-10.
 * The router in entry.S clears the high 32 bits in the first
 * 4 arguments (R3-R6).
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include "ppc_asm.h"
#include <asm/errno.h>
#include <asm/processor.h>

	.text

_GLOBAL(sys32_mmap)
	clrldi		r7, r7, 32	/* int fd parm */
	clrldi		r8, r8, 32	/* off_t offset parm */
	b		.sys_mmap

_GLOBAL(sys32_lseek)
	extsw		r4,r4		/* sign extend off_t offset parm */
	b		.sys_lseek

_GLOBAL(sys32_sendto)
	clrldi		r7, r7, 32	/* struct sockaddr *addr parm */
	clrldi		r8, r8, 32	/* int addr_len parm */
	b		.sys_sendto

_GLOBAL(sys32_recvfrom)
	clrldi		r7, r7, 32	/* struct sockaddr *addr parm */
	clrldi		r8, r8, 32	/* int *addr_len parm */
	b		.sys_recvfrom

_GLOBAL(sys32_getsockopt)
	clrldi		r7, r7, 32	/* int *optlen parm */
	b		.sys_getsockopt

_GLOBAL(sys32_bdflush)
	extsw		r4,r4		/* sign extend long data parm */
	b		.sys_bdflush

_GLOBAL(ppc32_mmap2)
	clrldi		r7, r7, 32	/* unsigned long fd parm */
	clrldi		r8, r8, 32	/* unsigned long pgoff */
	b		.sys32_mmap2

_GLOBAL(sys32_socketcall)	/* r3=call, r4=args */
	cmpwi		r3, 1
	blt-		.do_einval
	cmpwi		r3, 17
	bgt-		.do_einval
	subi		r3, r3, 1	/* index into socketcall_table vectors and jmp */
	sldi		r3, r3, 3	/* each entry is 8 bytes */
	LOADADDR(r10,.socketcall_table_begin)
	ldx		r10, r10, r3
	mtctr		r10
	bctr

/* Socket function vectored fix ups for 32 bit */
_STATIC(do_sys_socket) /* sys_socket(int, int, int) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwa		r4,4(r10)
	lwa		r5,8(r10)
	b		.sys_socket

_STATIC(do_sys_bind) /* sys_bind(int fd, struct sockaddr *, int) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwz		r4,4(r10)
	lwa		r5,8(r10)
	b		.sys_bind

_STATIC(do_sys_connect) /* sys_connect(int, struct sockaddr *, int) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwz		r4,4(r10)
	lwa		r5,8(r10)
	b		.sys_connect

_STATIC(do_sys_listen) /* sys_listen(int, int) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwa		r4,4(r10)
	b		.sys_listen

_STATIC(do_sys_accept) /* sys_accept(int, struct sockaddr *, int *) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwz		r4,4(r10)
	lwz		r5,8(r10)
	b		.sys_accept

_STATIC(do_sys_getsockname) /* sys_getsockname(int, struct sockaddr *, int *) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwz		r4,4(r10)
	lwz		r5,8(r10)
	b		.sys_getsockname

_STATIC(do_sys_getpeername) /* sys_getpeername(int, struct sockaddr *, int *) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwz		r4,4(r10)
	lwz		r5,8(r10)
	b		.sys_getpeername

_STATIC(do_sys_socketpair) /* sys_socketpair(int, int, int, int *) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwa		r4,4(r10)
	lwa		r5,8(r10)
	lwz		r6,12(r10)
	b		.sys_socketpair

_STATIC(do_sys_send) /* sys_send(int, void *, size_t, unsigned int) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwz		r4,4(r10)
	lwz		r5,8(r10)
	lwz		r6,12(r10)
	b		.sys_send

_STATIC(do_sys_recv) /* sys_recv(int, void *, size_t, unsigned int) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwz		r4,4(r10)
	lwz		r5,8(r10)
	lwz		r6,12(r10)
	b		.sys_recv

_STATIC(do_sys_sendto) /* sys32_sendto(int, u32, __kernel_size_t32, unsigned int, u32, int) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwz		r4,4(r10)
	lwz		r5,8(r10)
	lwz		r6,12(r10)
	lwz		r7,16(r10)
	lwa		r8,20(r10)
	b		.sys32_sendto

_STATIC(do_sys_recvfrom) /* sys32_recvfrom(int, u32, __kernel_size_t32, unsigned int, u32, u32) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwz		r4,4(r10)
	lwz		r5,8(r10)
	lwz		r6,12(r10)
	lwz		r7,16(r10)
	lwz		r8,20(r10)
	b		.sys32_recvfrom

_STATIC(do_sys_shutdown) /* sys_shutdown(int, int) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwa		r4,4(r10)
	b		.sys_shutdown

_STATIC(do_sys_setsockopt) /* sys32_setsockopt(int, int, int, char *, int) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwa		r4,4(r10)
	lwa		r5,8(r10)
	lwz		r6,12(r10)
	lwa		r7,16(r10)
	b		.sys32_setsockopt

_STATIC(do_sys_getsockopt) /* sys32_getsockopt(int, int, int, u32, u32) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwa		r4,4(r10)
	lwa		r5,8(r10)
	lwz		r6,12(r10)
	lwz		r7,16(r10)
	b		.sys32_getsockopt

_STATIC(do_sys_sendmsg) /* sys32_sendmsg(int, struct msghdr32 *, unsigned int) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwz		r4,4(r10)
	lwa		r5,8(r10)
	b		.sys32_sendmsg

_STATIC(do_sys_recvmsg) /* sys32_recvmsg(int, struct msghdr32 *, unsigned int) */
	mr		r10,r4
	lwa		r3,0(r10)
	lwz		r4,4(r10)
	lwa		r5,8(r10)
	b		.sys32_recvmsg

_STATIC(do_einval)
	li	r3,-EINVAL
	b	.ret_from_syscall_1
_STATIC(do_efault)
	li	r3,-EFAULT
	b	.ret_from_syscall_1

	.data
	.align	8
_GLOBAL(socketcall_table_begin)
	.llong		.do_sys_socket
	.llong		.do_sys_bind
	.llong		.do_sys_connect
	.llong		.do_sys_listen
	.llong		.do_sys_accept
	.llong		.do_sys_getsockname
	.llong		.do_sys_getpeername
	.llong		.do_sys_socketpair
	.llong		.do_sys_send
	.llong		.do_sys_recv
	.llong		.do_sys_sendto
	.llong		.do_sys_recvfrom
	.llong		.do_sys_shutdown
	.llong		.do_sys_setsockopt
	.llong		.do_sys_getsockopt
	.llong		.do_sys_sendmsg
	.llong		.do_sys_recvmsg
_GLOBAL(socketcall_table_end)
	.section	__ex_table,"a"
	.align		3
	.llong		.socketcall_table_begin
	.llong		0
	.llong		.socketcall_table_end
	.llong		.do_efault
	.previous
