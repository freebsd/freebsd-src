/*-
 * Copyright (c) 2012 Robert N. M. Watson
 * Copyright (c) 2012 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#ifndef _DEV_ISF_H_
#define	_DEV_ISF_H_

struct isf_range {
	off_t	ir_off;		/* Offset of range to delete (set to 0xFF) */
	size_t	ir_size;	/* Size of range */
};

#define	ISF_ERASE	_IOW('I', 1, struct isf_range)

/*
 * Ordinary read and write operations are limited to 512 bytes.
 * We support erasing 128K blocks and ignore the fact that portions of the
 * flash are in fact divided into 32K blocks.
 */
#define	ISF_SECTORSIZE	(512)
#define ISF_ERASE_BLOCK	(128 * 1024)

#ifdef _KERNEL
MALLOC_DECLARE(M_ISF);

enum bstate {
	BS_STEADY = 0,
	BS_ERASING
};

struct isf_softc {
	device_t		 isf_dev;
	int			 isf_unit;
	struct resource		*isf_res;
	int			 isf_rid;
	struct mtx		 isf_lock;
	struct disk		*isf_disk;
	struct proc		*isf_proc;
	int			 isf_doomed;

	/*
	 * Fields relating to in-progress and pending I/O, if any.
	 */
	struct bio_queue_head	 isf_bioq;
	uint16_t		 isf_rbuf[ISF_SECTORSIZE / 2];
	int			 isf_erasing;
	enum bstate		*isf_bstate;
};

#define	ISF_LOCK(sc)		mtx_lock(&(sc)->isf_lock)
#define	ISF_LOCK_ASSERT(sc)	mtx_assert(&(sc)->isf_lock, MA_OWNED)
#define	ISF_LOCK_DESTROY(sc)	mtx_destroy(&(sc)->isf_lock)
#define	ISF_LOCK_INIT(sc)	mtx_init(&(sc)->isf_lock, "isf", NULL,	\
				    MTX_DEF)
#define ISF_SLEEP(sc, wait, timo)	mtx_sleep((wait), 		\
					    &(sc)->isf_lock, PRIBIO, 	\
					    "isf", (timo))
#define	ISF_UNLOCK(sc)			mtx_unlock(&(sc)->isf_lock)
#define	ISF_WAKEUP(sc)			wakeup((sc))

int	isf_attach(struct isf_softc *sc);
void	isf_detach(struct isf_softc *sc);

extern devclass_t	isf_devclass;
#endif /* _KERNEL */

#endif	/* _DEV_ISF_H_ */
