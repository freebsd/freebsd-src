// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32.c
/// \brief      CRC32 calculation
//
//  Authors:    Lasse Collin
//              Ilya Kurdyukov
//              Hans Jansen
//
///////////////////////////////////////////////////////////////////////////////

#include "check.h"
#include "crc_common.h"

#if defined(CRC_X86_CLMUL)
#	define BUILDING_CRC32_CLMUL
#	include "crc_x86_clmul.h"
#elif defined(CRC32_ARM64)
#	include "crc32_arm64.h"
#endif


#ifdef CRC32_GENERIC

///////////////////
// Generic CRC32 //
///////////////////

static uint32_t
crc32_generic(const uint8_t *buf, size_t size, uint32_t crc)
{
	crc = ~crc;

#ifdef WORDS_BIGENDIAN
	crc = bswap32(crc);
#endif

	if (size > 8) {
		// Fix the alignment, if needed. The if statement above
		// ensures that this won't read past the end of buf[].
		while ((uintptr_t)(buf) & 7) {
			crc = lzma_crc32_table[0][*buf++ ^ A(crc)] ^ S8(crc);
			--size;
		}

		// Calculate the position where to stop.
		const uint8_t *const limit = buf + (size & ~(size_t)(7));

		// Calculate how many bytes must be calculated separately
		// before returning the result.
		size &= (size_t)(7);

		// Calculate the CRC32 using the slice-by-eight algorithm.
		while (buf < limit) {
			crc ^= aligned_read32ne(buf);
			buf += 4;

			crc = lzma_crc32_table[7][A(crc)]
			    ^ lzma_crc32_table[6][B(crc)]
			    ^ lzma_crc32_table[5][C(crc)]
			    ^ lzma_crc32_table[4][D(crc)];

			const uint32_t tmp = aligned_read32ne(buf);
			buf += 4;

			// At least with some compilers, it is critical for
			// performance, that the crc variable is XORed
			// between the two table-lookup pairs.
			crc = lzma_crc32_table[3][A(tmp)]
			    ^ lzma_crc32_table[2][B(tmp)]
			    ^ crc
			    ^ lzma_crc32_table[1][C(tmp)]
			    ^ lzma_crc32_table[0][D(tmp)];
		}
	}

	while (size-- != 0)
		crc = lzma_crc32_table[0][*buf++ ^ A(crc)] ^ S8(crc);

#ifdef WORDS_BIGENDIAN
	crc = bswap32(crc);
#endif

	return ~crc;
}
#endif


#if defined(CRC32_GENERIC) && defined(CRC32_ARCH_OPTIMIZED)

//////////////////////////
// Function dispatching //
//////////////////////////

// If both the generic and arch-optimized implementations are built, then
// the function to use is selected at runtime because the system running
// the binary might not have the arch-specific instruction set extension(s)
// available. The three dispatch methods in order of priority:
//
// 1. Indirect function (ifunc). This method is slightly more efficient
//    than the constructor method because it will change the entry in the
//    Procedure Linkage Table (PLT) for the function either at load time or
//    at the first call. This avoids having to call the function through a
//    function pointer and will treat the function call like a regular call
//    through the PLT. ifuncs are created by using
//    __attribute__((__ifunc__("resolver"))) on a function which has no
//    body. The "resolver" is the name of the function that chooses at
//    runtime which implementation to use.
//
// 2. Constructor. This method uses __attribute__((__constructor__)) to
//    set crc32_func at load time. This avoids extra computation (and any
//    unlikely threading bugs) on the first call to lzma_crc32() to decide
//    which implementation should be used.
//
// 3. First Call Resolution. On the very first call to lzma_crc32(), the
//    call will be directed to crc32_dispatch() instead. This will set the
//    appropriate implementation function and will not be called again.
//    This method does not use any kind of locking but is safe because if
//    multiple threads run the dispatcher simultaneously then they will all
//    set crc32_func to the same value.

typedef uint32_t (*crc32_func_type)(
		const uint8_t *buf, size_t size, uint32_t crc);

// Clang 16.0.0 and older has a bug where it marks the ifunc resolver
// function as unused since it is static and never used outside of
// __attribute__((__ifunc__())).
#if defined(CRC_USE_IFUNC) && defined(__clang__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-function"
#endif

// This resolver is shared between all three dispatch methods. It serves as
// the ifunc resolver if ifunc is supported, otherwise it is called as a
// regular function by the constructor or first call resolution methods.
static crc32_func_type
crc32_resolve(void)
{
	return is_arch_extension_supported()
			? &crc32_arch_optimized : &crc32_generic;
}

#if defined(CRC_USE_IFUNC) && defined(__clang__)
#	pragma GCC diagnostic pop
#endif

#ifndef CRC_USE_IFUNC

#ifdef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
// Constructor method.
#	define CRC32_SET_FUNC_ATTR __attribute__((__constructor__))
static crc32_func_type crc32_func;
#else
// First Call Resolution method.
#	define CRC32_SET_FUNC_ATTR
static uint32_t crc32_dispatch(const uint8_t *buf, size_t size, uint32_t crc);
static crc32_func_type crc32_func = &crc32_dispatch;
#endif

CRC32_SET_FUNC_ATTR
static void
crc32_set_func(void)
{
	crc32_func = crc32_resolve();
	return;
}

#ifndef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
static uint32_t
crc32_dispatch(const uint8_t *buf, size_t size, uint32_t crc)
{
	// When __attribute__((__ifunc__(...))) and
	// __attribute__((__constructor__)) isn't supported, set the
	// function pointer without any locking. If multiple threads run
	// the detection code in parallel, they will all end up setting
	// the pointer to the same value. This avoids the use of
	// mythread_once() on every call to lzma_crc32() but this likely
	// isn't strictly standards compliant. Let's change it if it breaks.
	crc32_set_func();
	return crc32_func(buf, size, crc);
}

#endif
#endif
#endif


#ifdef CRC_USE_IFUNC
extern LZMA_API(uint32_t)
lzma_crc32(const uint8_t *buf, size_t size, uint32_t crc)
		__attribute__((__ifunc__("crc32_resolve")));
#else
extern LZMA_API(uint32_t)
lzma_crc32(const uint8_t *buf, size_t size, uint32_t crc)
{
#if defined(CRC32_GENERIC) && defined(CRC32_ARCH_OPTIMIZED)
	// On x86-64, if CLMUL is available, it is the best for non-tiny
	// inputs, being over twice as fast as the generic slice-by-four
	// version. However, for size <= 16 it's different. In the extreme
	// case of size == 1 the generic version can be five times faster.
	// At size >= 8 the CLMUL starts to become reasonable. It
	// varies depending on the alignment of buf too.
	//
	// The above doesn't include the overhead of mythread_once().
	// At least on x86-64 GNU/Linux, pthread_once() is very fast but
	// it still makes lzma_crc32(buf, 1, crc) 50-100 % slower. When
	// size reaches 12-16 bytes the overhead becomes negligible.
	//
	// So using the generic version for size <= 16 may give better
	// performance with tiny inputs but if such inputs happen rarely
	// it's not so obvious because then the lookup table of the
	// generic version may not be in the processor cache.
#ifdef CRC_USE_GENERIC_FOR_SMALL_INPUTS
	if (size <= 16)
		return crc32_generic(buf, size, crc);
#endif

/*
#ifndef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
	// See crc32_dispatch(). This would be the alternative which uses
	// locking and doesn't use crc32_dispatch(). Note that on Windows
	// this method needs Vista threads.
	mythread_once(crc64_set_func);
#endif
*/
	return crc32_func(buf, size, crc);

#elif defined(CRC32_ARCH_OPTIMIZED)
	return crc32_arch_optimized(buf, size, crc);

#else
	return crc32_generic(buf, size, crc);
#endif
}
#endif
