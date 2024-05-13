/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2024 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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

#ifndef	_LINUXKPI_LINUX_BITFIELD_H
#define	_LINUXKPI_LINUX_BITFIELD_H

#include <linux/types.h>
#include <asm/byteorder.h>

/* Use largest possible type. */
static inline uint64_t ___lsb(uint64_t f) { return (f & -f); }
static inline uint64_t ___bitmask(uint64_t f) { return (f / ___lsb(f)); }

#define	_uX_get_bits(_n)						\
	static __inline uint ## _n ## _t				\
	u ## _n ## _get_bits(uint ## _n ## _t v, uint ## _n ## _t f)	\
	{								\
		return ((v & f) / ___lsb(f));				\
	}

_uX_get_bits(64)
_uX_get_bits(32)
_uX_get_bits(16)
_uX_get_bits(8)

#define	_leX_get_bits(_n)						\
	static __inline uint ## _n ## _t				\
	le ## _n ## _get_bits(__le ## _n v, uint ## _n ## _t f)		\
	{								\
		return ((le ## _n ## _to_cpu(v) & f) / ___lsb(f));	\
	}

_leX_get_bits(64)
_leX_get_bits(32)
_leX_get_bits(16)

#define	_uX_encode_bits(_n)						\
	static __inline uint ## _n ## _t				\
	u ## _n ## _encode_bits(uint ## _n ## _t v, uint ## _n ## _t f)	\
	{								\
		return ((v & ___bitmask(f)) * ___lsb(f));		\
	}

_uX_encode_bits(64)
_uX_encode_bits(32)
_uX_encode_bits(16)
_uX_encode_bits(8)

#define	_leX_encode_bits(_n)						\
	static __inline uint ## _n ## _t				\
	le ## _n ## _encode_bits(__le ## _n v, uint ## _n ## _t f)	\
	{								\
		return (cpu_to_le ## _n((v & ___bitmask(f)) * ___lsb(f))); \
	}

_leX_encode_bits(64)
_leX_encode_bits(32)
_leX_encode_bits(16)

#define	_leXp_replace_bits(_n)						\
	static __inline void						\
	le ## _n ## p_replace_bits(uint ## _n ## _t *p,			\
	    uint ## _n ## _t v, uint ## _n ## _t f)			\
	{								\
		*p = (*p & ~(cpu_to_le ## _n(f))) |			\
		     le ## _n ## _encode_bits(v, f);			\
	}

_leXp_replace_bits(64)
_leXp_replace_bits(32)
_leXp_replace_bits(16)

#define	_uXp_replace_bits(_n)						\
	static __inline void						\
	u ## _n ## p_replace_bits(uint ## _n ## _t *p,			\
	    uint ## _n ## _t v, uint ## _n ## _t f)			\
	{								\
		*p = (*p & ~f) | u ## _n ## _encode_bits(v, f);		\
	}

_uXp_replace_bits(64)
_uXp_replace_bits(32)
_uXp_replace_bits(16)
_uXp_replace_bits(8)

#define	_uX_replace_bits(_n)						\
	static __inline uint ## _n ## _t				\
	u ## _n ## _replace_bits(uint ## _n ## _t p,			\
	    uint ## _n ## _t v, uint ## _n ## _t f)			\
	{								\
		return ((p & ~f) | u ## _n ## _encode_bits(v, f));	\
	}

_uX_replace_bits(64)
_uX_replace_bits(32)
_uX_replace_bits(16)
_uX_replace_bits(8)

#define	__bf_shf(x)	(__builtin_ffsll(x) - 1)

#define	FIELD_FIT(_mask, _value)					\
	(!(((typeof(_mask))(_value) << __bf_shf(_mask)) & ~(_mask)))

#define	FIELD_PREP(_mask, _value)					\
	(((typeof(_mask))(_value) << __bf_shf(_mask)) & (_mask))

/* Likely would need extra sanity checks compared to FIELD_PREP()? */
#define	FIELD_PREP_CONST(_mask, _value)					\
	(((typeof(_mask))(_value) << __bf_shf(_mask)) & (_mask))

#define	FIELD_GET(_mask, _value)					\
	((typeof(_mask))(((_value) & (_mask)) >> __bf_shf(_mask)))

#endif	/* _LINUXKPI_LINUX_BITFIELD_H */
