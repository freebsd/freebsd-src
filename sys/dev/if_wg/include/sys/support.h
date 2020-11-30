/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019-2020 Rubicon Communications, LLC (Netgate)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
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

#ifndef SYS_SUPPORT_H_
#define SYS_SUPPORT_H_
#ifdef __LOCORE
#include <machine/asm.h>
#define SYM_FUNC_START ENTRY
#define SYM_FUNC_END END

#else
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/endian.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <vm/uma.h>

#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
#include <machine/fpu.h>
#endif
#include <crypto/siphash/siphash.h>


#define COMPAT_ZINC_IS_A_MODULE
MALLOC_DECLARE(M_WG);

#define	BUILD_BUG_ON(x)			CTASSERT(!(x))

#define BIT(nr)                 (1UL << (nr))
#define BIT_ULL(nr)             (1ULL << (nr))
#ifdef __LP64__
#define BITS_PER_LONG           64
#else
#define BITS_PER_LONG           32
#endif

#define rw_enter_write rw_wlock
#define rw_exit_write rw_wunlock
#define rw_enter_read rw_rlock
#define rw_exit_read rw_runlock
#define rw_exit rw_unlock

#define ASSERT(x) MPASS(x)

#define ___PASTE(a,b) a##b
#define __PASTE(a,b) ___PASTE(a,b)
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __COUNTER__)

#define typeof(x) __typeof__(x)


#define min_t(t, a, b) ({ t __a = (a); t __b = (b); __a > __b ? __b : __a; })

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint32_t __le32;
typedef uint64_t  u64;
typedef uint64_t  __le64;

#define __must_check		__attribute__((__warn_unused_result__))
#define asmlinkage
#define __ro_after_init	__read_mostly

#define get_unaligned_le32(x) le32dec(x)
#define get_unaligned_le64(x) le64dec(x)

#define cpu_to_le64(x) htole64(x)
#define cpu_to_le32(x) htole32(x)
#define letoh64(x) le64toh(x)

#define	need_resched() \
	((curthread->td_flags & (TDF_NEEDRESCHED|TDF_ASTPENDING)) || \
	 curthread->td_owepreempt)

 
#define CONTAINER_OF(a, b, c) __containerof((a), b, c)

typedef struct {
	uint64_t	k0;
	uint64_t	k1;
} SIPHASH_KEY;

static inline uint64_t
siphash24(const SIPHASH_KEY *key, const void *src, size_t len)
{
	SIPHASH_CTX ctx;

	return (SipHashX(&ctx, 2, 4, (const uint8_t *)key, src, len));
}

static inline void
put_unaligned_le32(u32 val, void *p)
{
	*((__le32 *)p) = cpu_to_le32(val);
}


#define rol32(i32, n) ((i32) << (n) | (i32) >> (32 - (n)))

#define memzero_explicit(p, s) explicit_bzero(p, s)

#define EXPORT_SYMBOL(x)

#define U32_MAX		((u32)~0U)
#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
#define	kfpu_begin(ctx) {							\
		if (ctx->sc_fpu_ctx == NULL)	 {			 \
			ctx->sc_fpu_ctx = fpu_kern_alloc_ctx(0); \
		}														 \
		critical_enter();										 \
	fpu_kern_enter(curthread, ctx->sc_fpu_ctx, FPU_KERN_NORMAL); \
}

#define	kfpu_end(ctx)	 {						 \
		MPASS(ctx->sc_fpu_ctx != NULL);			 \
		fpu_kern_leave(curthread, ctx->sc_fpu_ctx);	\
		critical_exit();			     \
}
#else
#define	kfpu_begin(ctx)
#define	kfpu_end(ctx)
#define	fpu_kern_free_ctx(p)
#endif

typedef enum {
	HAVE_NO_SIMD = 1 << 0,
	HAVE_FULL_SIMD = 1 << 1,
	HAVE_SIMD_IN_USE = 1 << 31
} simd_context_state_t;

typedef struct {
	simd_context_state_t sc_state;
	struct fpu_kern_ctx *sc_fpu_ctx;
} simd_context_t;


#define DONT_USE_SIMD NULL

static __must_check inline bool
may_use_simd(void)
{
#if defined(__amd64__)
	return true;
#else
	return false;
#endif
}

static inline void
simd_get(simd_context_t *ctx)
{
	ctx->sc_state = may_use_simd() ? HAVE_FULL_SIMD : HAVE_NO_SIMD;
}

static inline void
simd_put(simd_context_t *ctx)
{
#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
	if (is_fpu_kern_thread(0))
		return;
#endif
	if (ctx->sc_state & HAVE_SIMD_IN_USE)
		kfpu_end(ctx);
	ctx->sc_state = HAVE_NO_SIMD;
}

static __must_check inline bool
simd_use(simd_context_t *ctx)
{
#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
	if (is_fpu_kern_thread(0))
		return true;
#else
	return false;
#endif
	if (ctx == NULL)
		return false;
	if (!(ctx->sc_state & HAVE_FULL_SIMD))
		return false;
	if (ctx->sc_state & HAVE_SIMD_IN_USE)
		return true;
	kfpu_begin(ctx);
	ctx->sc_state |= HAVE_SIMD_IN_USE;
	return true;
}

static inline bool
simd_relax(simd_context_t *ctx)
{
	if ((ctx->sc_state & HAVE_SIMD_IN_USE) && need_resched()) {
		simd_put(ctx);
		simd_get(ctx);
		return simd_use(ctx);
	}
	return false;
}

#define unlikely(x) __predict_false(x)
#define likely(x) __predict_true(x)
/* Generic path for arbitrary size */


static inline unsigned long
__crypto_memneq_generic(const void *a, const void *b, size_t size)
{
	unsigned long neq = 0;

	while (size >= sizeof(unsigned long)) {
		neq |= *(const unsigned long *)a ^ *(const unsigned long *)b;
		__compiler_membar();
		a  = ((const char *)a + sizeof(unsigned long));
		b = ((const char *)b + sizeof(unsigned long));
		size -= sizeof(unsigned long);
	}
	while (size > 0) {
		neq |= *(const unsigned char *)a ^ *(const unsigned char *)b;
		__compiler_membar();
		a  = (const char *)a + 1;
		b = (const char *)b + 1;
		size -= 1;
	}
	return neq;
}

#define crypto_memneq(a, b, c) __crypto_memneq_generic((a), (b), (c))

static inline void
__cpu_to_le32s(uint32_t *buf)
{
	*buf = htole32(*buf);
}

static inline void cpu_to_le32_array(u32 *buf, unsigned int words)
{
	while (words--) {
		__cpu_to_le32s(buf);
		buf++;
	}
}

#define CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS 1
void __crypto_xor(u8 *dst, const u8 *src1, const u8 *src2, unsigned int len);

static inline void crypto_xor_cpy(u8 *dst, const u8 *src1, const u8 *src2,
				  unsigned int size)
{
	if (CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS &&
	    __builtin_constant_p(size) &&
	    (size % sizeof(unsigned long)) == 0) {
		unsigned long *d = (unsigned long *)dst;
		const unsigned long *s1 = (const unsigned long *)src1;
		const unsigned long *s2 = (const unsigned long *)src2;

		while (size > 0) {
			*d++ = *s1++ ^ *s2++;
			size -= sizeof(unsigned long);
		}
	} else {
		__crypto_xor(dst, src1, src2, size);
	}
}
#include <sys/kernel.h>
#define	module_init(fn)							\
static void \
wrap_ ## fn(void *dummy __unused) \
{								 \
	fn();						 \
}																		\
SYSINIT(if_wg_ ## fn, SI_SUB_LAST, SI_ORDER_FIRST, wrap_ ## fn, NULL)


#define	module_exit(fn) 							\
static void \
wrap_ ## fn(void *dummy __unused) \
{								 \
	fn();						 \
}																		\
SYSUNINIT(if_wg_ ## fn, SI_SUB_LAST, SI_ORDER_FIRST, wrap_ ## fn, NULL)

#define module_param(a, b, c)
#define MODULE_LICENSE(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_AUTHOR(x)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define __initconst
#define __initdata
#define __init
#define __exit
#define BUG() panic("%s:%d bug hit!\n", __FILE__, __LINE__)

#define	WARN_ON(cond) ({					\
      bool __ret = (cond);					\
      if (__ret) {						\
		printf("WARNING %s failed at %s:%d\n",		\
		    __stringify(cond), __FILE__, __LINE__);	\
      }								\
      unlikely(__ret);						\
})

#define pr_err printf
#define pr_info printf
#define IS_ENABLED(x) 0
#define	___stringify(...)		#__VA_ARGS__
#define	__stringify(...)		___stringify(__VA_ARGS__)
#define kmalloc(size, flag) malloc((size), M_WG, M_WAITOK)
#define kfree(p) free(p, M_WG)
#define vzalloc(size) malloc((size), M_WG, M_WAITOK|M_ZERO)
#define vfree(p) free(p, M_WG)
#endif
#endif
