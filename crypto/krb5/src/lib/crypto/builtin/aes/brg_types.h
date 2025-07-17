/*
---------------------------------------------------------------------------
Copyright (c) 1998-2013, Brian Gladman, Worcester, UK. All rights reserved.

The redistribution and use of this software (with or without changes)
is allowed without the payment of fees or royalties provided that:

  source code distributions include the above copyright notice, this
  list of conditions and the following disclaimer;

  binary distributions include the above copyright notice, this list
  of conditions and the following disclaimer in their documentation.

This software is provided 'as is' with no explicit or implied warranties
in respect of its operation, including, but not limited to, correctness
and fitness for purpose.
---------------------------------------------------------------------------
Issue Date: 30/09/2017
*/

#ifndef _BRG_TYPES_H
#define _BRG_TYPES_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <limits.h>
#include <stdint.h>

#if defined( _MSC_VER ) && ( _MSC_VER >= 1300 )
#  include <stddef.h>
#  define ptrint_t intptr_t
#elif defined( __ECOS__ )
#  define intptr_t unsigned int
#  define ptrint_t intptr_t
#elif defined( __GNUC__ ) && ( __GNUC__ >= 3 ) && !(defined( __HAIKU__ ) || defined( __VxWorks__ ))
#  define ptrint_t intptr_t
#else
#  define ptrint_t int
#endif

/* define unsigned 8-bit type if not available in stdint.h */
#if !defined(UINT8_MAX)
  typedef unsigned char uint8_t;
#endif

/* define unsigned 16-bit type if not available in stdint.h */
#if !defined(UINT16_MAX)
  typedef unsigned short uint16_t;
#endif

/* define unsigned 32-bit type if not available in stdint.h and define the
   macro li_32(h) which converts a sequence of eight hexadecimal characters
   into a 32 bit constant 
*/
#if defined(UINT_MAX) && UINT_MAX == 4294967295u
#  define li_32(h) 0x##h##u
#  if !defined(UINT32_MAX)
     typedef unsigned int uint32_t;
#  endif
#elif defined(ULONG_MAX) && ULONG_MAX == 4294967295u
#  define li_32(h) 0x##h##ul
#  if !defined(UINT32_MAX)
     typedef unsigned long uint32_t;
#  endif
#elif defined( _CRAY )
#  error This code needs 32-bit data types, which Cray machines do not provide
#else
#  error Please define uint32_t as a 32-bit unsigned integer type in brg_types.h
#endif

/* define unsigned 64-bit type if not available in stdint.h and define the
   macro li_64(h) which converts a sequence of eight hexadecimal characters
   into a 64 bit constant 
*/
#if defined( __BORLANDC__ ) && !defined( __MSDOS__ )
#  define li_64(h) 0x##h##ui64
#  if !defined(UINT64_MAX)
     typedef unsigned __int64 uint64_t;  
#  endif
#elif defined( _MSC_VER ) && ( _MSC_VER < 1300 )    /* 1300 == VC++ 7.0 */
#  define li_64(h) 0x##h##ui64
#  if !defined(UINT64_MAX)
     typedef unsigned __int64 uint64_t;
#  endif
#elif defined( __sun ) && defined( ULONG_MAX ) && ULONG_MAX == 0xfffffffful
#  define li_64(h) 0x##h##ull
#  if !defined(UINT64_MAX)
     typedef unsigned long long uint64_t;
#  endif
#elif defined( __MVS__ )
#  define li_64(h) 0x##h##ull
#  if !defined(UINT64_MAX)
     typedef unsigned long long uint64_t;
#  endif
#elif defined( UINT_MAX ) && UINT_MAX > 4294967295u
#  if UINT_MAX == 18446744073709551615u
#    define li_64(h) 0x##h##u
#    if !defined(UINT64_MAX)
       typedef unsigned int uint64_t;
#    endif
#  endif
#elif defined( ULONG_MAX ) && ULONG_MAX > 4294967295u
#  if ULONG_MAX == 18446744073709551615ul
#    define li_64(h) 0x##h##ul
#    if !defined(UINT64_MAX) && !defined(_UINT64_T)
       typedef unsigned long uint64_t;
#    endif
#  endif
#elif defined( ULLONG_MAX ) && ULLONG_MAX > 4294967295u
#  if ULLONG_MAX == 18446744073709551615ull
#    define li_64(h) 0x##h##ull
#    if !defined(UINT64_MAX) && !defined( __HAIKU__ )
       typedef unsigned long long uint64_t;
#    endif
#  endif
#elif defined( ULONG_LONG_MAX ) && ULONG_LONG_MAX > 4294967295u
#  if ULONG_LONG_MAX == 18446744073709551615ull
#    define li_64(h) 0x##h##ull
#    if !defined(UINT64_MAX)
       typedef unsigned long long uint64_t;
#    endif
#  endif
#endif

#if !defined( li_64 )
#  if defined( NEED_UINT_64T )
#    error Please define uint64_t as an unsigned 64 bit type in brg_types.h
#  endif
#endif

#ifndef RETURN_VALUES
#  define RETURN_VALUES
#  if defined( DLL_EXPORT )
#    if defined( _MSC_VER ) || defined ( __INTEL_COMPILER )
#      define VOID_RETURN    __declspec( dllexport ) void __stdcall
#      define INT_RETURN     __declspec( dllexport ) int  __stdcall
#    elif defined( __GNUC__ )
#      define VOID_RETURN    __declspec( __dllexport__ ) void
#      define INT_RETURN     __declspec( __dllexport__ ) int
#    else
#      error Use of the DLL is only available on the Microsoft, Intel and GCC compilers
#    endif
#  elif defined( DLL_IMPORT )
#    if defined( _MSC_VER ) || defined ( __INTEL_COMPILER )
#      define VOID_RETURN    __declspec( dllimport ) void __stdcall
#      define INT_RETURN     __declspec( dllimport ) int  __stdcall
#    elif defined( __GNUC__ )
#      define VOID_RETURN    __declspec( __dllimport__ ) void
#      define INT_RETURN     __declspec( __dllimport__ ) int
#    else
#      error Use of the DLL is only available on the Microsoft, Intel and GCC compilers
#    endif
#  elif defined( __WATCOMC__ )
#    define VOID_RETURN  void __cdecl
#    define INT_RETURN   int  __cdecl
#  else
#    define VOID_RETURN  void
#    define INT_RETURN   int
#  endif
#endif

/*	These defines are used to detect and set the memory alignment of pointers.
    Note that offsets are in bytes.

    ALIGN_OFFSET(x,n)			return the positive or zero offset of 
                                the memory addressed by the pointer 'x' 
                                from an address that is aligned on an 
                                'n' byte boundary ('n' is a power of 2)

    ALIGN_FLOOR(x,n)			return a pointer that points to memory
                                that is aligned on an 'n' byte boundary 
                                and is not higher than the memory address
                                pointed to by 'x' ('n' is a power of 2)

    ALIGN_CEIL(x,n)				return a pointer that points to memory
                                that is aligned on an 'n' byte boundary 
                                and is not lower than the memory address
                                pointed to by 'x' ('n' is a power of 2)
*/

#define ALIGN_OFFSET(x,n)	(((ptrint_t)(x)) & ((n) - 1))
#define ALIGN_FLOOR(x,n)	((uint8_t*)(x) - ( ((ptrint_t)(x)) & ((n) - 1)))
#define ALIGN_CEIL(x,n)		((uint8_t*)(x) + (-((ptrint_t)(x)) & ((n) - 1)))

/*  These defines are used to declare buffers in a way that allows
    faster operations on longer variables to be used.  In all these
    defines 'size' must be a power of 2 and >= 8. NOTE that the 
    buffer size is in bytes but the type length is in bits

    UNIT_TYPEDEF(x,size)        declares a variable 'x' of length 
                                'size' bits

    BUFR_TYPEDEF(x,size,bsize)  declares a buffer 'x' of length 'bsize' 
                                bytes defined as an array of variables
                                each of 'size' bits (bsize must be a 
                                multiple of size / 8)

    UNIT_CAST(x,size)           casts a variable to a type of 
                                length 'size' bits

    UPTR_CAST(x,size)           casts a pointer to a pointer to a 
                                variable of length 'size' bits
*/

#define UI_TYPE(size)               uint##size##_t
#define UNIT_TYPEDEF(x,size)        typedef UI_TYPE(size) x
#define BUFR_TYPEDEF(x,size,bsize)  typedef UI_TYPE(size) x[bsize / (size >> 3)]
#define UNIT_CAST(x,size)           ((UI_TYPE(size) )(x))  
#define UPTR_CAST(x,size)           ((UI_TYPE(size)*)(x))

#if defined(__cplusplus)
}
#endif

#endif
