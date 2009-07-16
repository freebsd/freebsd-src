/*-
 * Copyright (c) 2009 Jeffrey Roberson <jeff@freebsd.org>
 * Copyright (c) 2009 Robert N. M. Watson
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
 * $FreeBSD$
 */

/*
 * This is the virtual network stack memory allocator, which provides support
 * for virtualized global variables via a special linker set, set_vnet.  When
 * "options VIMAGE" isn't defined, virtualized global variables are compiled
 * as normal globals.
 */

#ifndef _NET_VNET_H_
#define	_NET_VNET_H_

#ifdef _KERNEL
#ifdef VIMAGE

#if defined(__arm__)
__asm__(".section set_vnet, \"aw\", %progbits");
#else
__asm__(".section set_vnet, \"aw\", @progbits");
#endif
__asm__(".previous");

#define	VNET_NAME(n)		vnet_entry_##n
#define	VNET_DECLARE(t, n)	extern t VNET_NAME(n)
#define	VNET_DEFINE(t, n)	t VNET_NAME(n) __section("set_vnet") __used
#define	_VNET_PTR(b, n)		(__typeof(VNET_NAME(n))*)		\
				    ((b) + (uintptr_t)&VNET_NAME(n))

#define	_VNET(b, n)		(*_VNET_PTR(b, n))

/*
 * Virtualized global variable accessor macros.
 */
#define	VNET_VNET_PTR(vnet, n)		_VNET_PTR((vnet)->vnet_data_base, n)
#define	VNET_VNET(vnet, n)		(*VNET_VNET_PTR((vnet), n))

#define	VNET_PTR(n)		VNET_VNET_PTR(curvnet, n)
#define	VNET(n)			VNET_VNET(curvnet, n)

/*
 * Sysctl variants for vnet-virtualized global variables.  Include
 * <sys/sysctl.h> to expose these definitions.
 *
 * Note: SYSCTL_PROC() handler functions will need to resolve pointer
 * arguments themselves, if required.
 */
#ifdef SYSCTL_OID
int	vnet_sysctl_handle_int(SYSCTL_HANDLER_ARGS);
int	vnet_sysctl_handle_opaque(SYSCTL_HANDLER_ARGS);
int	vnet_sysctl_handle_string(SYSCTL_HANDLER_ARGS);
int	vnet_sysctl_handle_uint(SYSCTL_HANDLER_ARGS);

#define	SYSCTL_VNET_INT(parent, nbr, name, access, ptr, val, descr)	\
	SYSCTL_OID(parent, nbr, name, CTLTYPE_INT|CTLFLAG_MPSAFE|(access), \
	    ptr, val, vnet_sysctl_handle_int, "I", descr)
#define	SYSCTL_VNET_PROC(parent, nbr, name, access, ptr, arg, handler,	\
	    fmt, descr)							\
	SYSCTL_OID(parent, nbr, name, access, ptr, arg, handler, fmt,	\
	    descr)
#define	SYSCTL_VNET_STRING(parent, nbr, name, access, arg, len, descr)	\
	SYSCTL_OID(parent, nbr, name, CTLTYPE_STRING|(access), arg,	\
	    len, vnet_sysctl_handle_string, "A", descr)
#define	SYSCTL_VNET_STRUCT(parent, nbr, name, access, ptr, type, descr)	\
	SYSCTL_OID(parent, nbr, name, CTLTYPE_OPAQUE|(access), ptr,	\
	    sizeof(struct type), vnet_sysctl_handle_opaque, "S," #type,	\
	    descr)
#define	SYSCTL_VNET_UINT(parent, nbr, name, access, ptr, val, descr)	\
	SYSCTL_OID(parent, nbr, name, CTLTYPE_UINT|CTLFLAG_MPSAFE|(access), \
	    ptr, val, vnet_sysctl_handle_uint, "IU", descr)
#endif /* SYSCTL_OID */

/*
 * Interfaces from the kernel linker.
 */
void	*vnet_data_alloc(int size);
void	 vnet_data_copy(void *start, int size);
void	 vnet_data_free(void *start_arg, int size);

/*
 * Interfaces for vnet setup/teardown.
 */
struct vnet;
void	 vnet_data_init(struct vnet *vnet);
void	 vnet_data_destroy(struct vnet *vnet);

#else /* !VIMAGE */

/*
 * Versions of the VNET macros that compile to normal global variables and
 * standard sysctl definitions.
 */
#define	VNET_NAME(n)		n
#define	VNET_DECLARE(t, n)	extern t n
#define	VNET_DEFINE(t, n)	t n
#define	_VNET_PTR(b, n)		&VNET_NAME(n)

#ifdef SYSCTL_OID
#define	SYSCTL_VNET_INT(parent, nbr, name, access, ptr, val, descr)	\
	SYSCTL_INT(parent, nbr, name, access, ptr, val, descr)
#define	SYSCTL_VNET_PROC(parent, nbr, name, access, ptr, arg, handler,	\
	    fmt, descr)							\
	SYSCTL_PROC(parent, nbr, name, access, ptr, arg, handler, fmt,	\
	    descr)
#define	SYSCTL_VNET_STRING(parent, nbr, name, access, arg, len, descr)	\
	SYSCTL_STRING(parent, nbr, name, access, arg, len, descr)
#define	SYSCTL_VNET_STRUCT(parent, nbr, name, access, ptr, type, descr)	\
	SYSCTL_STRUCT(parent, nbr, name, access, ptr, type, descr)
#define	SYSCTL_VNET_UINT(parent, nbr, name, access, ptr, val, descr)	\
	SYSCTL_UINT(parent, nbr, name, access, ptr, val, descr)
#endif /* SYSCTL_OID */

/*
 * Virtualized global variable accessor macros.
 */
#define	VNET_VNET_PTR(vnet, n)		(&(n))
#define	VNET_VNET(vnet, n)		(n)

#define	VNET_PTR(n)		(&(n))
#define	VNET(n)			(n)

#endif /* VIMAGE */

#endif /* _KERNEL */

#endif /* !_NET_VNET_H_ */
