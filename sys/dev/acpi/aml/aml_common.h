/*-
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: aml_common.h,v 1.4 2000/08/08 14:12:05 iwasaki Exp $
 *	$FreeBSD$
 */

#ifndef _AML_COMMON_H_
#define _AML_COMMON_H_

/*
 * General Stuff
 */
#ifdef _KERNEL
#define AML_SYSABORT() do {						\
	printf("aml: fatal errer at %s:%d\n", __FILE__, __LINE__);	\
	panic("panic in AML interpreter!");				\
} while(0)
#define AML_SYSASSERT(x) do {						\
	if (!(x)) {							\
		AML_SYSABORT();						\
	}								\
} while(0)
#define AML_SYSERRX(eval, fmt, args...) do {				\
	printf(fmt, args);						\
} while(0)
#define AML_DEBUGGER(x, y)	/* no debugger in kernel */
#else /* !_KERNEL */
#define AML_SYSASSERT(x)	assert(x)
#define AML_SYSABORT()  	abort()
#define AML_SYSERRX(eval, fmt, args...)	errx(eval, fmt, args)
#define AML_DEBUGGER(x, y)	aml_dbgr(x, y)
#endif /* _KERNEL */

union	aml_object;
struct	aml_name;

extern int	aml_debug;

#define AML_DEBUGPRINT(args...) do {					\
	if (aml_debug) {						\
		printf(args);						\
	}								\
} while(0)

void		 aml_showobject(union aml_object *);
void		 aml_showtree(struct aml_name *, int);
int		 aml_print_curname(struct aml_name *);
void		 aml_print_namestring(u_int8_t *);
void		 aml_print_indent(int);

/*
 * Reigion I/O Stuff for both kernel/userland.
 */

/*
 * Field Flags
 */
/* bit 0 -3:	AccessType */
#define AML_FIELDFLAGS_ACCESS_ANYACC		0x00
#define AML_FIELDFLAGS_ACCESS_BYTEACC		0x01
#define AML_FIELDFLAGS_ACCESS_WORDACC		0x02
#define AML_FIELDFLAGS_ACCESS_DWORDACC		0x03
#define AML_FIELDFLAGS_ACCESS_BLOCKACC		0x04
#define AML_FIELDFLAGS_ACCESS_SMBSENDRECVACC	0x05
#define AML_FIELDFLAGS_ACCESS_SMBQUICKACC	0x06
#define AML_FIELDFLAGS_ACCESSTYPE(flags)	(flags & 0x0f)
/* bit 4:	LockRule */
#define AML_FIELDFLAGS_LOCK_NOLOCK		0x00
#define AML_FIELDFLAGS_LOCK_LOCK		0x10
#define AML_FIELDFLAGS_LOCKRULE(flags)		(flags & 0x10)
/* bit 5 - 6:	UpdateRule */
#define AML_FIELDFLAGS_UPDATE_PRESERVE		0x00
#define AML_FIELDFLAGS_UPDATE_WRITEASONES	0x20
#define AML_FIELDFLAGS_UPDATE_WRITEASZEROS	0x40
#define AML_FIELDFLAGS_UPDATERULE(flags)	(flags & 0x60)
/* bit 7:	reserved (must be 0) */

#define AML_REGION_INPUT	0
#define AML_REGION_OUTPUT	1

#define AML_REGION_SYSMEM	0
#define AML_REGION_SYSIO	1
#define AML_REGION_PCICFG	2
#define AML_REGION_EMBCTL	3
#define AML_REGION_SMBUS	4

struct aml_region_handle {
	/* These are copies of values used on initialization */ 
	struct		aml_environ *env;
	int		regtype;
	u_int32_t	flags;
	u_int32_t	baseaddr;
	u_int32_t	bitoffset;
	u_int32_t	bitlen;

	/* following is determined on initialization */ 
	vm_offset_t	addr, bytelen;
	u_int32_t	unit;		/* access unit in bytes */

	/* region type dependant */
	vm_offset_t	vaddr;			/* SystemMemory */
	u_int32_t	pci_bus, pci_devfunc;	/* PCI_Config */
};

u_int32_t	 aml_adjust_readvalue(u_int32_t, u_int32_t, u_int32_t,
				      u_int32_t);
u_int32_t	 aml_adjust_updatevalue(u_int32_t, u_int32_t, u_int32_t,
					u_int32_t, u_int32_t);

u_int32_t	 aml_bufferfield_read(u_int8_t *, u_int32_t, u_int32_t);
int		 aml_bufferfield_write(u_int32_t, u_int8_t *,
				       u_int32_t, u_int32_t);

int		 aml_region_handle_alloc(struct aml_environ *, int, u_int32_t,
					 u_int32_t, u_int32_t, u_int32_t,
					 struct aml_region_handle *);
void		 aml_region_handle_free(struct aml_region_handle *);

int		 aml_region_io(struct aml_environ *, int, int,
			       u_int32_t, u_int32_t *, u_int32_t,
			       u_int32_t, u_int32_t);
extern int	 aml_region_read_simple(struct aml_region_handle *, vm_offset_t,
					u_int32_t *);
extern int	 aml_region_write_simple(struct aml_region_handle *, vm_offset_t,
					 u_int32_t);
extern u_int32_t aml_region_prompt_read(struct aml_region_handle *,
					u_int32_t);
extern u_int32_t aml_region_prompt_write(struct aml_region_handle *,
					 u_int32_t);
extern int	 aml_region_prompt_update_value(u_int32_t, u_int32_t,
					        struct aml_region_handle *);
#endif /* !_AML_COMMON_H_ */
