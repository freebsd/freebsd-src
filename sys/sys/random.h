/*-
 * Copyright (c) 2000-2013 Mark R. V. Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_SYS_RANDOM_H_
#define	_SYS_RANDOM_H_

#ifdef _KERNEL

int read_random(void *, int);

/*
 * Note: if you add or remove members of random_entropy_source, remember to also update the
 * KASSERT regarding what valid members are in random_harvest_internal(), and remember the
 * strings in the static array random_source_descr[] in random_harvestq.c.
 *
 * NOTE: complain loudly to markm@ or on the lists if this enum gets more than 32
 * distinct values (0-31)! ENTROPYSOURCE may be == 32, but not > 32.
 */
enum random_entropy_source {
	RANDOM_START = 0,
	RANDOM_CACHED = 0,
	/* Environmental sources */
	RANDOM_ATTACH,
	RANDOM_KEYBOARD,
	RANDOM_MOUSE,
	RANDOM_NET_TUN,
	RANDOM_NET_ETHER,
	RANDOM_NET_NG,
	RANDOM_INTERRUPT,
	RANDOM_SWI,
	RANDOM_UMA_ALLOC,
	RANDOM_ENVIRONMENTAL_END, /* This one is wasted */
	/* High-quality HW RNGs from here on. */
	RANDOM_PURE_OCTEON,
	RANDOM_PURE_SAFE,
	RANDOM_PURE_GLXSB,
	RANDOM_PURE_UBSEC,
	RANDOM_PURE_HIFN,
	RANDOM_PURE_RDRAND,
	RANDOM_PURE_NEHEMIAH,
	RANDOM_PURE_RNDTEST,
	RANDOM_PURE_VIRTIO,
	ENTROPYSOURCE
};
void random_harvest(const void *, u_int, u_int, enum random_entropy_source);

#endif /* _KERNEL */

#endif /* _SYS_RANDOM_H_ */
