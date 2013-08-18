/*-
 * Copyright (c) 2013 Davide Italiano <davide@FreeBSD.org>
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
 */

#ifndef _SYS_SCHEDCTL_H_
#define _SYS_SCHEDCTL_H_

#include <sys/lock.h>
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>

/*
 * Thread possible states.
 */
#define	STATE_FREE	0x01
#define	STATE_ONCPU	0x02
#define	STATE_OFFCPU	0x04

/* XXX: what is the proper 'pad' size? */
struct state_shared {
	volatile int 	sh_state;
	char		pad[CACHE_LINE_SIZE - sizeof(int)];
} __aligned(CACHE_LINE_SIZE);

struct page_shared {
	long				bitmap;
	int				available;
	vm_object_t			shared_page_obj;
	vm_offset_t			pageaddr;
	vm_offset_t			usraddr;
	SLIST_ENTRY(page_shared)	pg_next;
};

typedef struct state_shared shstate_t;
typedef struct page_shared shpage_t;

/*
 * Function prototypes.
 */
void	schedctl_init(void);
void	schedctl_thread_cleanup(struct thread *);
void	schedctl_proc_cleanup(void);

#endif /* _SYS_SCHEDCTL_H_ */
