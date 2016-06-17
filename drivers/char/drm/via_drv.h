/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef _VIA_DRV_H_
#define _VIA_DRV_H_

typedef struct drm_via_private {
	drm_via_sarea_t *sarea_priv;
	drm_map_t *sarea;
	drm_map_t *fb;
	drm_map_t *mmio;
	unsigned long agpAddr;
} drm_via_private_t;

extern int via_do_init_map(drm_device_t *dev, drm_via_init_t *init);
extern int via_do_cleanup_map(drm_device_t *dev);
extern int via_map_init(struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg);

/*=* [DBG] For RedHat7.3 insert kernel module has unresolved symbol
   cmpxchg() *=*/

/* Include this here so that driver can be used with older kernels. */
#ifndef __HAVE_ARCH_CMPXCHG 

#ifdef CONFIG_SMP
#define LOCK_PREFIX "lock ; "
#else
#define LOCK_PREFIX ""
#endif		

#if defined(__alpha__)
static __inline__ unsigned long
__cmpxchg_u32(volatile int *m, int old, int new)
{
	unsigned long prev, cmp;

	__asm__ __volatile__(
	"1:	ldl_l %0,%2\n"
	"	cmpeq %0,%3,%1\n"
	"	beq %1,2f\n"
	"	mov %4,%1\n"
	"	stl_c %1,%2\n"
	"	beq %1,3f\n"
	"2:	mb\n"
	".subsection 2\n"
	"3:	br 1b\n"
	".previous"
	: "=&r"(prev), "=&r"(cmp), "=m"(*m)
	: "r"((long) old), "r"(new), "m"(*m));

	return prev;
}

static __inline__ unsigned long
__cmpxchg_u64(volatile long *m, unsigned long old, unsigned long new)
{
	unsigned long prev, cmp;

	__asm__ __volatile__(
	"1:	ldq_l %0,%2\n"
	"	cmpeq %0,%3,%1\n"
	"	beq %1,2f\n"
	"	mov %4,%1\n"
	"	stq_c %1,%2\n"
	"	beq %1,3f\n"
	"2:	mb\n"
	".subsection 2\n"
	"3:	br 1b\n"
	".previous"
	: "=&r"(prev), "=&r"(cmp), "=m"(*m)
	: "r"((long) old), "r"(new), "m"(*m));

	return prev;
}

static __inline__ unsigned long
__cmpxchg(volatile void *ptr, unsigned long old, unsigned long new, int size)
{
	switch (size) {
		case 4:
			return __cmpxchg_u32(ptr, old, new);
		case 8:
			return __cmpxchg_u64(ptr, old, new);
	}
	return old;
}
#define cmpxchg(ptr,o,n)						 \
  ({									 \
     __typeof__(*(ptr)) _o_ = (o);					 \
     __typeof__(*(ptr)) _n_ = (n);					 \
     (__typeof__(*(ptr))) __cmpxchg((ptr), (unsigned long)_o_,		 \
				    (unsigned long)_n_, sizeof(*(ptr))); \
  })

#elif __i386__
static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
				      unsigned long new, int size)
{
	unsigned long prev;
	switch (size) {
	case 1:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgb %b1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	case 2:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgw %w1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	case 4:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgl %1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	}
	return old;
}

#define cmpxchg(ptr,o,n)						\
  ((__typeof__(*(ptr)))__cmpxchg((ptr),(unsigned long)(o),		\
				 (unsigned long)(n),sizeof(*(ptr))))
#endif /* i386 & alpha */
#endif
#endif
