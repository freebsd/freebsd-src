/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*-
 * Copyright (c) 2011-2012 Semihalf.
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

#ifndef _BMAN_H
#define _BMAN_H

#include <sys/vmem.h>
#include <machine/vmparam.h>

/*
 * BMAN Configuration
 */

/*
 * Portal definitions
 */

struct bman_softc {
	device_t	sc_dev;			/* device handle */
	int		sc_rrid;		/* register rid */
	struct resource	*sc_rres;		/* register resource */
	int		sc_irid;		/* interrupt rid */
	struct resource	*sc_ires;		/* interrupt resource */
	void		*sc_icookie;
	vmem_t		*sc_vmem;		/* resource pool */
	int		 sc_major;
	int		 sc_minor;
};

struct bman_buffer {
	uint16_t bpid;
	uint16_t buf_hi;
	uint32_t buf_lo;
} __aligned(8);

struct bman_pool;
struct bman_buffer;

typedef void (*bm_depletion_handler)(void *, bool);

/*
 * External API
 */

struct bman_pool *bman_new_pool(void);
struct bman_pool *bman_pool_create(uint8_t *bpid, uint16_t buffer_size,
    uint16_t max_buffers, uint32_t dep_sw_entry, uint32_t dep_sw_exit, uint32_t
    dep_hw_entry, uint32_t dep_hw_exit, bm_depletion_handler dep_cb, void *arg);

/*
 * @brief Destroy pool.
 *
 * The bman_pool_destroy() function destroys the BMAN pool.
 * The buffer pool must be empty.
 *
 * @param pool		The BMAN pool handle.
 * @return		0 on success, EBUSY if the pool is not empty.
 */
int bman_pool_destroy(struct bman_pool *pool);

/*
 * @brief Count free buffers in given pool.
 *
 * @param pool		The BMAN pool handle.
 *
 * @returns		Number of free buffers in pool.
 */
uint32_t bman_count(struct bman_pool *pool);

int bman_put_buffers(struct bman_pool *, struct bman_buffer *, int);
static inline int
bman_put_buffer(struct bman_pool *p, vm_paddr_t buf, int bpid)
{
	struct bman_buffer b = {
		.bpid = bpid,
		.buf_hi = ((uintptr_t)buf) >> 32,
		.buf_lo = ((uintptr_t)buf) & 0xffffffff
	};
	return (bman_put_buffers(p, &b, 1));
}

int bman_acquire(struct bman_pool *, struct bman_buffer *, uint8_t);

int bman_create_affine_portal(device_t, vm_offset_t, vm_offset_t, int);
void bman_destroy_affine_portal(int);
uint32_t bman_get_bpid(struct bman_pool *);

/*
 * Bus i/f
 */
int bman_attach(device_t dev);
int bman_detach(device_t dev);
int bman_suspend(device_t dev);
int bman_resume(device_t dev);
int bman_shutdown(device_t dev);

#endif /* BMAN_H */
