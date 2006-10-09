/*	$NetBSD: openfirm.h,v 1.1 1998/05/15 10:16:00 tsubai Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (C) 2000 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _OPENFIRM_H_
#define _OPENFIRM_H_

/*
 * Prototypes for Open Firmware Interface Routines
 */

typedef unsigned long cell_t;

typedef	unsigned int	ihandle_t;
typedef unsigned int	phandle_t;

#ifdef _KERNEL
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/malloc.h>

#define p1275_ptr2cell(p)       ((cell_t)((uintptr_t)((void *)(p))))
#define p1275_int2cell(i)       ((cell_t)((int)(i)))
#define p1275_uint2cell(u)      ((cell_t)((unsigned int)(u)))
#define p1275_size2cell(u)      ((cell_t)((size_t)(u)))
#define p1275_phandle2cell(ph)  ((cell_t)((unsigned int)((phandle_t)(ph))))
#define p1275_dnode2cell(d)     ((cell_t)((unsigned int)((pnode_t)(d))))
#define p1275_ihandle2cell(ih)  ((cell_t)((unsigned int)((ihandle_t)(ih))))
#define p1275_ull2cell_high(ll) (0LL)
#define p1275_ull2cell_low(ll)  ((cell_t)(ll))
#define p1275_uintptr2cell(i)   ((cell_t)((uintptr_t)(i)))

#define p1275_cell2ptr(p)       ((void *)((cell_t)(p)))
#define p1275_cell2int(i)       ((int)((cell_t)(i)))
#define p1275_cell2uint(u)      ((unsigned int)((cell_t)(u)))
#define p1275_cell2size(u)      ((size_t)((cell_t)(u)))
#define p1275_cell2phandle(ph)  ((phandle_t)((cell_t)(ph)))
#define p1275_cell2dnode(d)     ((pnode_t)((cell_t)(d)))
#define p1275_cell2ihandle(ih)  ((ihandle_t)((cell_t)(ih)))
#define p1275_cells2ull(h, l)   ((unsigned long long)(cell_t)(l))
#define p1275_cell2uintptr(i)   ((uintptr_t)((cell_t)(i)))

MALLOC_DECLARE(M_OFWPROP);

/*
 * Stuff that is used by the Open Firmware code.
 */
void	set_openfirm_callback(int (*)(void *));
int	openfirmware(void *);

/*
 * This isn't actually an Open Firmware function, but it seemed like the right
 * place for it to go.
 */
void		OF_init(int (*openfirm)(void *));

/* Generic functions */
int		OF_test(char *);
void		OF_helloworld(void);
void		OF_printf(const char *, ...);

/* Device tree functions */
phandle_t	OF_peer(phandle_t);
phandle_t	OF_child(phandle_t);
phandle_t	OF_parent(phandle_t);
phandle_t	OF_instance_to_package(ihandle_t);
int		OF_getproplen(phandle_t, char *);
int		OF_getprop(phandle_t, char *, void *, int);
int		OF_getprop_alloc(phandle_t package, char *propname, int elsz,
    void **buf);
int		OF_nextprop(phandle_t, char *, char *);
int		OF_setprop(phandle_t, char *, void *, int);
int		OF_canon(const char *, char *, int);
phandle_t	OF_finddevice(const char *);
int		OF_instance_to_path(ihandle_t, char *, int);
int		OF_package_to_path(phandle_t, char *, int);
int		OF_call_method(char *, ihandle_t, int, int, ...);

/* Device I/O functions */
ihandle_t	OF_open(char *);
void		OF_close(ihandle_t);
int		OF_read(ihandle_t, void *, int);
int		OF_write(ihandle_t, void *, int);
int		OF_seek(ihandle_t, u_quad_t);

/* Memory functions */
void 		*OF_claim(void *, u_int, u_int);
void		OF_release(void *, u_int);

/* Control transfer functions */
void		OF_boot(char *);
void		OF_enter(void);
void		OF_exit(void) __attribute__((noreturn));
void		OF_chain(void *, u_int,
    void (*)(void *, u_int, void *, void *, u_int), void *, u_int);

/* User interface functions */
int		OF_interpret(char *, int, ...);
#if 0
void 		*OF_set_callback(void *);
void		OF_set_symbol_lookup(void *, void *);
#endif

/* Time function */
int		OF_milliseconds(void);

/* sun4v additions */
void            OF_set_mmfsa_traptable(void *tba_addr, uint64_t mmfsa_ra);
int             OF_translate_virt(vm_offset_t va, int *valid, vm_paddr_t *physaddr, int *mode);
vm_paddr_t      OF_vtophys(vm_offset_t va);

#endif /* _KERNEL */
#endif /* _OPENFIRM_H_ */
