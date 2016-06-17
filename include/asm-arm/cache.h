/*
 *  linux/include/asm-arm/cache.h
 */
#ifndef __ASMARM_CACHE_H
#define __ASMARM_CACHE_H

#define        L1_CACHE_BYTES  32
#define        L1_CACHE_ALIGN(x)       (((x)+(L1_CACHE_BYTES-1))&~(L1_CACHE_BYTES-1))
#define        SMP_CACHE_BYTES L1_CACHE_BYTES

#ifdef MODULE
#define __cacheline_aligned __attribute__((__aligned__(L1_CACHE_BYTES)))
#else
#define __cacheline_aligned					\
  __attribute__((__aligned__(L1_CACHE_BYTES),			\
		 __section__(".data.cacheline_aligned")))
#endif

#endif
