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
Issue Date: 10/09/2018
*/

#ifndef _BRG_ENDIAN_H
#define _BRG_ENDIAN_H

#define IS_BIG_ENDIAN      4321 /* byte 0 is most significant (mc68k) */
#define IS_LITTLE_ENDIAN   1234 /* byte 0 is least significant (i386) */

/* This is needed when using clang with MSVC to avoid including */
/* endian.h and byteswap.h which are not present on Windows     */
#if defined( _MSC_VER ) && defined( __clang__ )
#  undef __GNUC__
#endif

/* Include files where endian defines and byteswap functions may reside */
#if defined( __sun )
#  include <sys/isa_defs.h>
#elif defined( __FreeBSD__ ) || defined( __OpenBSD__ ) || defined( __NetBSD__ )
#  include <sys/endian.h>
#elif defined( BSD ) && ( BSD >= 199103 ) || defined( __APPLE__ ) || \
      defined( __CYGWIN32__ ) || defined( __DJGPP__ ) || defined( __osf__ )
#  include <machine/endian.h>
#elif defined( __linux__ ) || defined( __GNUC__ ) || defined( __GNU_LIBRARY__ )
#  if !defined( __MINGW32__ ) && !defined( _AIX )
#    include <endian.h>
#    if !defined( __BEOS__ )
#      include <byteswap.h>
#    endif
#  endif
#endif

/* Now attempt to set the define for platform byte order using any  */
/* of the four forms SYMBOL, _SYMBOL, __SYMBOL & __SYMBOL__, ...    */
/* which seem to encompass most endian symbol definitions           */

#if defined( __ORDER_BIG_ENDIAN__ ) && defined( __ORDER_LITTLE_ENDIAN__ )
#  if defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#  elif defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#  endif
#elif defined( __ORDER_BIG_ENDIAN__ )
#  define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#elif defined( __ORDER_LITTLE_ENDIAN__ )
#  define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#endif

#if defined( BIG_ENDIAN ) && defined( LITTLE_ENDIAN )
#  if defined( BYTE_ORDER ) && BYTE_ORDER == BIG_ENDIAN
#    define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#  elif defined( BYTE_ORDER ) && BYTE_ORDER == LITTLE_ENDIAN
#    define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#  endif
#elif defined( BIG_ENDIAN )
#  define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#elif defined( LITTLE_ENDIAN )
#  define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#endif

#if defined( _BIG_ENDIAN ) && defined( _LITTLE_ENDIAN )
#  if defined( _BYTE_ORDER ) && _BYTE_ORDER == _BIG_ENDIAN
#    define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#  elif defined( _BYTE_ORDER ) && _BYTE_ORDER == _LITTLE_ENDIAN
#    define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#  endif
#elif defined( _BIG_ENDIAN )
#  define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#elif defined( _LITTLE_ENDIAN )
#  define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#endif

#if defined( __BIG_ENDIAN ) && defined( __LITTLE_ENDIAN )
#  if defined( __BYTE_ORDER ) && __BYTE_ORDER == __BIG_ENDIAN
#    define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#  elif defined( __BYTE_ORDER ) && __BYTE_ORDER == __LITTLE_ENDIAN
#    define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#  endif
#elif defined( __BIG_ENDIAN )
#  define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#elif defined( __LITTLE_ENDIAN )
#  define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#endif

#if defined( __BIG_ENDIAN__ ) && defined( __LITTLE_ENDIAN__ )
#  if defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __BIG_ENDIAN__
#    define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#  elif defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __LITTLE_ENDIAN__
#    define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#  endif
#elif defined( __BIG_ENDIAN__ )
#  define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#elif defined( __LITTLE_ENDIAN__ )
#  define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#endif

/*  if the platform byte order could not be determined, then try to */
/*  set this define using common machine defines                    */
#if !defined(PLATFORM_BYTE_ORDER)

#if   defined( __alpha__ ) || defined( __alpha ) || defined( i386 )       || \
      defined( __i386__ )  || defined( _M_I86 )  || defined( _M_IX86 )    || \
      defined( __OS2__ )   || defined( sun386 )  || defined( __TURBOC__ ) || \
      defined( vax )       || defined( vms )     || defined( VMS )        || \
      defined( __VMS )     || defined( _M_X64 )
#  define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN

#elif defined( AMIGA )   || defined( applec )    || defined( __AS400__ )  || \
      defined( _CRAY )   || defined( __hppa )    || defined( __hp9000 )   || \
      defined( ibm370 )  || defined( mc68000 )   || defined( m68k )       || \
      defined( __MRC__ ) || defined( __MVS__ )   || defined( __MWERKS__ ) || \
      defined( sparc )   || defined( __sparc)    || defined( SYMANTEC_C ) || \
      defined( __VOS__ ) || defined( __TIGCC__ ) || defined( __TANDEM )   || \
      defined( THINK_C ) || defined( __VMCMS__ ) || defined( _AIX )
#  define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN

#elif 0     /* **** EDIT HERE IF NECESSARY **** */
#  define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#elif 0     /* **** EDIT HERE IF NECESSARY **** */
#  define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#else
#  error Please edit lines 126 or 128 in brg_endian.h to set the platform byte order
#endif

#endif

#endif
