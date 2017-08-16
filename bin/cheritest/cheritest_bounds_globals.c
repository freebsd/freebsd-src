/*-
 * Copyright (c) 2017 Robert N. M. Watson
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
 */

#include <sys/cdefs.h>

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"

/*
 * Global variables of various types, with pointers to them.  Some are static,
 * which may allow the compiler to make code-generation optimisations based on
 * analysing the whole compilation unit.  Others are non-static so that (at
 * least we hope) the compiler is more likely to do what it says on the tin.
 * Regardless of the way the globals are actually generated, we'd like to see
 * correct bounds.  We make statically initialised pointers non-static in the
 * hopes that they will be set up by the linker.
 *
 * Further down the file, similar tests are run on globals allocated in
 * another compilation unit, but declared here with different C-language
 * types.  Regardless of what the types say about the sizes, pointers to the
 * globals should have the correct dynamic sizes in their underlying
 * capabilities.
 *
 * XXXRW: For now, no expectations about non-CheriABI code.
 */

/*
 * Template for a test function, which assumes there is a global (test) and
 * corresponding statically initialised pointer (testp).  Check that both
 * taking a pointer to the global, and using the existing pointer, return
 * offsets and sizes as desired.
 */
#define	TEST_BOUNDS(test)						\
	void								\
	test_bounds_##test(const struct cheri_test *ctp __unused)	\
	{								\
		__capability void *allocation =				\
		    (__capability void *)&test;				\
		size_t allocation_offset = cheri_getoffset(allocation);	\
		size_t allocation_len = cheri_getlen(allocation);	\
		size_t pointer_offset = cheri_getoffset(test##p);	\
		size_t pointer_len = cheri_getlen(test##p);		\
									\
		/* Global offset. */					\
		if (allocation_offset != 0)				\
			cheritest_failure_errx(				\
			    "global: non-zero offset (%ju)",		\
			    allocation_offset);				\
									\
		/* Global length. */					\
		if (allocation_len != sizeof(test))			\
			cheritest_failure_errx(				\
			    "global: incorrect length (expected %ju, "	\
			    "got %ju)", sizeof(test),			\
			    allocation_len);				\
									\
		/* Pointer offset. */					\
		if (pointer_offset != 0)				\
			cheritest_failure_errx(				\
			    "pointer: non-zero offset (%ju)",		\
			    pointer_offset);				\
									\
		/* Pointer length. */					\
		if (pointer_len != sizeof(test))			\
			cheritest_failure_errx(				\
			    "pointer: incorrect length (expected %ju, "	\
			    "got %ju)", sizeof(test), pointer_len);	\
		cheritest_success();					\
	}

/*
 * Basic integer types.
 */
static uint8_t			 global_static_uint8;
extern __capability void	*global_static_uint8p;
__capability void		*global_static_uint8p =
		    (__capability void *)&global_static_uint8;

extern uint8_t			 global_uint8;
uint8_t				 global_uint8;
extern __capability void	*global_uint8p;
__capability void		*global_uint8p =
		    (__capability void *)&global_uint8;

static uint16_t			 global_static_uint16;
extern __capability void	*global_static_uint16p;
__capability void		*global_static_uint16p =
		    (__capability void *)&global_static_uint16;

extern uint16_t			 global_uint16;
uint16_t			 global_uint16;
extern __capability void	*global_uint16p;
__capability void		*global_uint16p =
		    (__capability void *)&global_uint16;

static uint32_t			 global_static_uint32;
extern __capability void	*global_static_uint32p;
__capability void		*global_static_uint32p =
		    (__capability void *)&global_static_uint32;

extern uint32_t			 global_uint32;
uint32_t			 global_uint32;
extern __capability void	*global_uint32p;
__capability void		*global_uint32p =
		    (__capability void *)&global_uint32;

static uint64_t	 global_static_uint64;
extern __capability void	*global_static_uint64p;
__capability void		*global_static_uint64p =
		    (__capability void *)&global_static_uint64;

extern uint64_t			 global_uint64;
uint64_t			 global_uint64;
extern __capability void	*global_uint64p;
__capability void		*global_uint64p =
		    (__capability void *)&global_uint64;

TEST_BOUNDS(global_static_uint8);
TEST_BOUNDS(global_uint8);
TEST_BOUNDS(global_static_uint16);
TEST_BOUNDS(global_uint16);
TEST_BOUNDS(global_static_uint32);
TEST_BOUNDS(global_uint32);
TEST_BOUNDS(global_static_uint64);
TEST_BOUNDS(global_uint64);

/*
 * Arrays of bytes with annoying (often prime) sizes.
 */
static uint8_t			 global_static_uint8_array1[1];
extern __capability void	*global_static_uint8_array1p;
__capability void		*global_static_uint8_array1p =
		    (__capability void *)&global_static_uint8_array1;

extern uint8_t			 global_uint8_array1[1];
uint8_t				 global_uint8_array1[1];
extern __capability void	*global_uint8_array1p;
__capability void		*global_uint8_array1p =
		    (__capability void *)&global_uint8_array1;

static uint8_t			 global_static_uint8_array3[3];
extern __capability void	*global_static_uint8_array3p;
__capability void		*global_static_uint8_array3p =
		    (__capability void *)&global_static_uint8_array3;

extern uint8_t			 global_uint8_array3[3];
uint8_t				 global_uint8_array3[3];
extern __capability void	*global_uint8_array3p;
__capability void		*global_uint8_array3p =
		    (__capability void *)&global_uint8_array3;

static uint8_t			 global_static_uint8_array17[17];
extern __capability void	*global_static_uint8_array17p;
__capability void		*global_static_uint8_array17p =
		    (__capability void *)&global_static_uint8_array17;

extern uint8_t			 global_uint8_array17[17];
uint8_t				 global_uint8_array17[17];
extern __capability void	*global_uint8_array17p;
__capability void		*global_uint8_array17p =
		    (__capability void *)&global_uint8_array17;

static uint8_t			 global_static_uint8_array65537[65537];
extern __capability void	*global_static_uint8_array65537p;
__capability void		*global_static_uint8_array65537p =
		    (__capability void *)&global_static_uint8_array65537;

extern uint8_t			 global_uint8_array65537[65537];
uint8_t				 global_uint8_array65537[65537];
extern __capability void	*global_uint8_array65537p;
__capability void		*global_uint8_array65537p =
		    (__capability void *)&global_uint8_array65537;

TEST_BOUNDS(global_static_uint8_array1);
TEST_BOUNDS(global_uint8_array1);
TEST_BOUNDS(global_static_uint8_array3)
TEST_BOUNDS(global_uint8_array3);
TEST_BOUNDS(global_static_uint8_array17);
TEST_BOUNDS(global_uint8_array17);
TEST_BOUNDS(global_static_uint8_array65537);
TEST_BOUNDS(global_uint8_array65537);

/*
 * Arrays of bytes with power-of-two sizes starting with size 32.
 */
static uint8_t			 global_static_uint8_array32[32];
extern __capability void	*global_static_uint8_array32p;
__capability void		*global_static_uint8_array32p =
		    (__capability void *)&global_static_uint8_array32;

extern uint8_t			 global_uint8_array32[32];
uint8_t		 		 global_uint8_array32[32];
extern __capability void	*global_uint8_array32p;
__capability void		*global_uint8_array32p =
		    (__capability void *)&global_uint8_array32;

static uint8_t	 global_static_uint8_array64[64];
extern __capability void	*global_static_uint8_array64p;
__capability void		*global_static_uint8_array64p =
		    (__capability void *)&global_static_uint8_array64;

extern uint8_t			 global_uint8_array64[64];
uint8_t				 global_uint8_array64[64];
extern __capability void	*global_uint8_array64p;
__capability void		*global_uint8_array64p =
		    (__capability void *)&global_uint8_array64;

static uint8_t			 global_static_uint8_array128[128];
extern __capability void	*global_static_uint8_array128p;
__capability void		*global_static_uint8_array128p =
		    (__capability void *)&global_static_uint8_array128;

extern uint8_t			 global_uint8_array128[128];
uint8_t				 global_uint8_array128[128];
extern __capability void	*global_uint8_array128p;
__capability void		*global_uint8_array128p =
		    (__capability void *)&global_uint8_array128;

static uint8_t			 global_static_uint8_array256[256];
extern __capability void	*global_static_uint8_array256p;
__capability void		*global_static_uint8_array256p =
		    (__capability void *)&global_static_uint8_array256;

extern uint8_t			 global_uint8_array256[256];
uint8_t				 global_uint8_array256[256];
extern __capability void	*global_uint8_array256p;
__capability void		*global_uint8_array256p =
		    (__capability void *)&global_uint8_array256;

static uint8_t			 global_static_uint8_array512[512];
extern __capability void	*global_static_uint8_array512p;
__capability void		*global_static_uint8_array512p =
		    (__capability void *)&global_static_uint8_array512;

extern uint8_t			 global_uint8_array512[512];
uint8_t				 global_uint8_array512[512];
extern __capability void	*global_uint8_array512p;
__capability void		*global_uint8_array512p =
		    (__capability void *)&global_uint8_array512;

static uint8_t			 global_static_uint8_array1024[1024];
extern __capability void	*global_static_uint8_array1024p;
__capability void		*global_static_uint8_array1024p =
		    (__capability void *)&global_static_uint8_array1024;

extern uint8_t			 global_uint8_array1024[1024];
uint8_t				 global_uint8_array1024[1024];
extern __capability void	*global_uint8_array1024p;
__capability void		*global_uint8_array1024p =
		    (__capability void *)&global_uint8_array1024;

static uint8_t			 global_static_uint8_array2048[2048];
extern __capability void	*global_static_uint8_array2048p;
__capability void		*global_static_uint8_array2048p =
		    (__capability void *)&global_static_uint8_array2048;

extern uint8_t			 global_uint8_array2048[2048];
uint8_t				 global_uint8_array2048[2048];
extern __capability void	*global_uint8_array2048p;
__capability void		*global_uint8_array2048p =
		    (__capability void *)&global_uint8_array2048;

static uint8_t			 global_static_uint8_array4096[4096];
extern __capability void	*global_static_uint8_array4096p;
__capability void		*global_static_uint8_array4096p =
		    (__capability void *)&global_static_uint8_array4096;

extern uint8_t			 global_uint8_array4096[4096];
uint8_t				 global_uint8_array4096[4096];
extern __capability void	*global_uint8_array4096p;
__capability void		*global_uint8_array4096p =
		    (__capability void *)&global_uint8_array4096;

static uint8_t			 global_static_uint8_array8192[8192];
extern __capability void	*global_static_uint8_array8192p;
__capability void		*global_static_uint8_array8192p =
		    (__capability void *)&global_static_uint8_array8192;

extern uint8_t			 global_uint8_array8192[8192];
uint8_t				 global_uint8_array8192[8192];
extern __capability void	*global_uint8_array8192p;
__capability void		*global_uint8_array8192p =
		    (__capability void *)&global_uint8_array8192;

static uint8_t			 global_static_uint8_array16384[16384];
extern __capability void	*global_static_uint8_array16384p;
__capability void		*global_static_uint8_array16384p =
		    (__capability void *)&global_static_uint8_array16384;

extern uint8_t			 global_uint8_array16384[16384];
uint8_t				 global_uint8_array16384[16384];
extern __capability void	*global_uint8_array16384p;
__capability void		*global_uint8_array16384p =
		    (__capability void *)&global_uint8_array16384;

static uint8_t			 global_static_uint8_array32768[32768];
extern __capability void	*global_static_uint8_array32768p;
__capability void		*global_static_uint8_array32768p =
		    (__capability void *)&global_static_uint8_array32768;

extern uint8_t			 global_uint8_array32768[32768];
uint8_t				 global_uint8_array32768[32768];
extern __capability void	*global_uint8_array32768p;
__capability void		*global_uint8_array32768p =
		    (__capability void *)&global_uint8_array32768;

static uint8_t			 global_static_uint8_array65536[65536];
extern __capability void	*global_static_uint8_array65536p;
__capability void		*global_static_uint8_array65536p =
		    (__capability void *)&global_static_uint8_array65536;

extern uint8_t			 global_uint8_array65536[65536];
uint8_t				 global_uint8_array65536[65536];
extern __capability void	*global_uint8_array65536p;
__capability void		*global_uint8_array65536p =
		    (__capability void *)&global_uint8_array65536;

TEST_BOUNDS(global_static_uint8_array32);
TEST_BOUNDS(global_uint8_array32);
TEST_BOUNDS(global_static_uint8_array64);
TEST_BOUNDS(global_uint8_array64);
TEST_BOUNDS(global_static_uint8_array128);
TEST_BOUNDS(global_uint8_array128);
TEST_BOUNDS(global_static_uint8_array256);
TEST_BOUNDS(global_uint8_array256);
TEST_BOUNDS(global_static_uint8_array512);
TEST_BOUNDS(global_uint8_array512);
TEST_BOUNDS(global_static_uint8_array1024);
TEST_BOUNDS(global_uint8_array1024);
TEST_BOUNDS(global_static_uint8_array2048);
TEST_BOUNDS(global_uint8_array2048);
TEST_BOUNDS(global_static_uint8_array4096);
TEST_BOUNDS(global_uint8_array4096);
TEST_BOUNDS(global_static_uint8_array8192);
TEST_BOUNDS(global_uint8_array8192);
TEST_BOUNDS(global_static_uint8_array16384);
TEST_BOUNDS(global_uint8_array16384);
TEST_BOUNDS(global_static_uint8_array32768);
TEST_BOUNDS(global_uint8_array32768);
TEST_BOUNDS(global_static_uint8_array65536);
TEST_BOUNDS(global_uint8_array65536);

/*
 * Tests on globals allocated in another compilation unit.  Sometimes with
 * correct local type information, and sometimes with incorrect local type
 * information.
 */

/* 1-byte global with correct type information. */
extern uint8_t			 extern_global_uint8;
extern __capability void	*extern_global_uint8p;
__capability void		*extern_global_uint8p =
		    (__capability void *)&extern_global_uint8;

/* 2-byte global with incorrect type information. */
extern uint8_t			 extern_global_uint16;
extern __capability void	*extern_global_uint16p;
__capability void		*extern_global_uint16p =
		    (__capability void *)&extern_global_uint16;

/* 4-byte global with correct type information. */
extern uint32_t			 extern_global_uint32;
extern __capability void	*extern_global_uint32p;
__capability void		*extern_global_uint32p =
		    (__capability void *)&extern_global_uint32;

/* 8-byte global with incorrect type information. */
extern uint32_t			 extern_global_uint64;
extern __capability void	*extern_global_uint64p;
__capability void		*extern_global_uint64p =
		    (__capability void *)&extern_global_uint64;

/* 1-byte global with incorrect type information. */
extern uint8_t			 extern_global_array1[2];
extern __capability void	*extern_global_array1p;
__capability void		*extern_global_array1p =
		    (__capability void *)&extern_global_array1;

/* 7-byte global with correct type information. */
extern uint8_t			 extern_global_array7[7];
extern __capability void	*extern_global_array7p;
__capability void		*extern_global_array7p =
		    (__capability void *)&extern_global_array7;

/* 65,537-byte global with incorrect type information. */
extern uint8_t			 extern_global_array65537[127];
extern __capability void	*extern_global_array65537p;
__capability void		*extern_global_array65537p =
		    (__capability void *)&extern_global_array65537;

/* 16-byte global with correct type information. */
extern uint8_t			 extern_global_array16[16];
extern __capability void	*extern_global_array16p;
__capability void		*extern_global_array16p =
		    (__capability void *)&extern_global_array16;

/* 256-byte global with incorrect type information. */
extern uint8_t			 extern_global_array256[128];
extern __capability void	*extern_global_array256p;
__capability void		*extern_global_array256p =
		    (__capability void *)&extern_global_array256;

/* 65,536-byte global with correct type information. */
extern uint8_t			 extern_global_array65536[65536];
extern __capability void	*extern_global_array65536p;
__capability void		*extern_global_array65536p =
		    (__capability void *)&extern_global_array65536;

/*
 * Checks against C-based types.
 */
TEST_BOUNDS(extern_global_uint8);
TEST_BOUNDS(extern_global_uint32);
TEST_BOUNDS(extern_global_array7);
TEST_BOUNDS(extern_global_array16);
TEST_BOUNDS(extern_global_array65536);

/*
 * Template for a test function, which assumes there is a global (test) and
 * corresponding statically initialised pointer (testp).  Check that both
 * taking a pointer to the global, and using the existing pointer, return
 * offsets and sizes as desired.  Unlike above, take an explicit type argument
 * from which to generate a size, whereas above we assume the C type of the
 * variable is a correct source of size information.
 */
#define	TEST_DYNAMIC_BOUNDS(test, size)					\
	void								\
	test_bounds_##test(const struct cheri_test *ctp __unused)	\
	{								\
		__capability void *allocation =				\
		    (__capability void *)&test;				\
		size_t allocation_offset = cheri_getoffset(allocation);	\
		size_t allocation_len = cheri_getlen(allocation);	\
		size_t pointer_offset = cheri_getoffset(test##p);	\
		size_t pointer_len = cheri_getlen(test##p);		\
									\
		/* Global offset. */					\
		if (allocation_offset != 0)				\
			cheritest_failure_errx(				\
			    "global: non-zero offset (%ju)",		\
			    allocation_offset);				\
									\
		/* Global length. */					\
		if (allocation_len != size)				\
			cheritest_failure_errx(				\
			    "global: incorrect length (expected %ju, "	\
			    "got %ju)", size, allocation_len);		\
									\
		/* Pointer offset. */					\
		if (pointer_offset != 0)				\
			cheritest_failure_errx(				\
			    "pointer: non-zero offset (%ju)",		\
			    pointer_offset);				\
									\
		/* Pointer length. */					\
		if (pointer_len != size)				\
			cheritest_failure_errx(				\
			    "pointer: incorrect length (expected %ju, "	\
			    "got %ju)", size, pointer_len);		\
		cheritest_success();					\
	}

/*
 * Checks to ensure we are using linker-provided size information, and not C
 * types.  Use a priori knowledge of the types to check lengths.
 */
TEST_DYNAMIC_BOUNDS(extern_global_uint16, sizeof(uint16_t));
TEST_DYNAMIC_BOUNDS(extern_global_uint64, sizeof(uint64_t));
TEST_DYNAMIC_BOUNDS(extern_global_array1, sizeof(uint8_t[1]));
TEST_DYNAMIC_BOUNDS(extern_global_array65537, sizeof(uint8_t[65537]));
TEST_DYNAMIC_BOUNDS(extern_global_array256, sizeof(uint8_t[256]));
