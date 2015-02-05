/*
 * stdint_msvc.h - C99 integer types for older Visual C compilers
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 *
 * ----------------------------------------------------------------------
 *
 * Fairly straight forward implementation of the C99 standard integer
 * types.
 */

#ifndef __STDINT_INCLUDED
#define __STDINT_INCLUDED

#if !defined(_MSC_VER) || _MSC_VER >= 1800
# error Use only with MSVC6 - MSVC11(VS2012)
#endif

#include <crtdefs.h>
#include <limits.h>

/* ---------------------------------------------------------------------
 * We declare the min/max values, using the MSVC syntax for literals of
 * a given bit width.
 */

#define _VC_SI_LIT(lit,wbit)	(lit ##  i ## wbit)
#define _VC_UI_LIT(lit,wbit)	(lit ## ui ## wbit)

/* ---------------------------------------------------------------------
 * Exact width integer types
 */
typedef	__int8	int8_t;
typedef	__int16	int16_t;
typedef	__int32	int32_t;
typedef	__int64	int64_t;

typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

#if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)
# define INT8_MIN   _I8_MIN
# define INT8_MAX   _I8_MAX
# define UINT8_MAX  _UI8_MAX
# define INT16_MIN  _I16_MIN
# define INT16_MAX  _I16_MAX
# define UINT16_MAX _UI16_MAX
# define INT32_MIN  _I32_MIN
# define INT32_MAX  _I32_MAX
# define UINT32_MAX _UI32_MAX
# define INT64_MIN  _I64_MIN
# define INT64_MAX  _I64_MAX
# define UINT64_MAX _UI64_MAX
#endif

/* ---------------------------------------------------------------------
 * Least-size integers
 *
 * These are mapped to exact size.
 */
typedef	__int8	int_least8_t;
typedef	__int16	int_least16_t;
typedef	__int32	int_least32_t;
typedef	__int64	int_least64_t;

typedef unsigned  __int8 uint_least8_t;
typedef unsigned __int16 uint_least16_t;
typedef unsigned __int32 uint_least32_t;
typedef unsigned __int64 uint_least64_t;

#if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)
#define INT_LEAST8_MIN   _I8_MIN
#define INT_LEAST8_MAX	 _I8_MAX
#define UINT_LEAST8_MAX  _UI8_MAX
#define INT_LEAST16_MIN	 _I16_MIN
#define INT_LEAST16_MAX	 _I16_MAX
#define UINT_LEAST16_MAX _UI16_MAX
#define INT_LEAST32_MIN	 _I32_MIN
#define INT_LEAST32_MAX	 _I32_MAX
#define UINT_LEAST32_MAX _UI32_MAX
#define INT_LEAST64_MIN  _I64_MIN
#define INT_LEAST64_MAX	 _I64_MAX
#define UINT_LEAST64_MAX _UI64_MAX
#endif

/* ---------------------------------------------------------------------
 * least-size, fastest integer
 *
 * The 'FAST' types are all 32 bits, except the 64 bit quantity; as the
 * natural register width is 32 bits, quantities of that size are fastest
 * to operate on naturally. (This even holds for the x86_64; MSVC uses
 * the 'llp64' model.
 */
typedef	__int32	int_fast8_t;
typedef	__int32	int_fast16_t;
typedef	__int32	int_fast32_t;
typedef	__int64	int_fast64_t;

typedef unsigned __int32 uint_fast8_t;
typedef unsigned __int32 uint_fast16_t;
typedef unsigned __int32 uint_fast32_t;
typedef unsigned __int64 uint_fast64_t;

#if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)
#define INT_FAST8_MIN   _I32_MIN
#define INT_FAST8_MAX   _I32_MAX
#define UINT_FAST8_MAX	_UI32_MAX
#define INT_FAST16_MIN  _I32_MIN
#define INT_FAST16_MAX  _I32_MAX
#define UINT_FAST16_MAX	_UI32_MAX
#define INT_FAST32_MIN  _I32_MIN
#define INT_FAST32_MAX  _I32_MAX
#define UINT_FAST32_MAX	_UI32_MAX
#define INT_FAST64_MIN  _I64_MIN
#define INT_FAST64_MAX  _I64_MAX
#define UINT_FAST64_MAX	_UI64_MAX
#endif

/* ---------------------------------------------------------------------
 * The (u)intptr_t, ptrdiff_t and size_t definitions depend on the
 * target: 32bit for x86, and 64bit for x64, aka amd64. Well, we
 * have to bite the bullet.
 */

/* ------------------------------------------------------------------ */
# if defined(_WIN64) || defined(WIN64)
/* ------------------------------------------------------------------ */

# ifndef _INTPTR_T_DEFINED
#  define _INTPTR_T_DEFINED
   typedef __int64 intptr_t;
# endif

# ifndef _UINTPTR_T_DEFINED
#  define _UINTPTR_T_DEFINED
   typedef unsigned __int64 uintptr_t;
# endif

# ifndef _PTRDIFF_T_DEFINED
#  define _PTRDIFF_T_DEFINED
   typedef __int64 ptrdiff_t;
# endif

# if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)
#  ifndef INTPTR_MIN
#   define INTPTR_MIN _I64_MIN
#  endif
#  ifndef INTPTR_MAX
#   define INTPTR_MAX _I64_MAX
#  endif
#  ifndef UINTPTR_MAX
#   define UINTPTR_MAX _UI64_MAX
#  endif
#  ifndef PTRDIFF_MIN
#   define PTRDIFF_MIN _I64_MIN
#  endif
#  ifndef PTRDIFF_MAX
#   define PTRDIFF_MAX _I64_MAX
#  endif
# endif

/* ------------------------------------------------------------------ */
#else   /* 32 bit target assumed here! */
/* ------------------------------------------------------------------ */

# ifndef _INTPTR_T_DEFINED
#  define _INTPTR_T_DEFINED
   typedef __int32 intptr_t;
# endif

# ifndef _UINTPTR_T_DEFINED
#  define _UINTPTR_T_DEFINED
   typedef unsigned __int32 uintptr_t;
# endif

# ifndef _PTRDIFF_T_DEFINED
#  define _PTRDIFF_T_DEFINED
   typedef __int64 ptrdiff_t;
# endif

# if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)
#  ifndef INTPTR_MIN
#   define INTPTR_MIN _I32_MIN
#  endif
#  ifndef INTPTR_MAX
#   define INTPTR_MAX _I32_MAX
#  endif
#  ifndef UINTPTR_MAX
#   define UINTPTR_MAX _UI32_MAX
#  endif
#  ifndef PTRDIFF_MIN
#   define PTRDIFF_MIN _I32_MIN
#  endif
#  ifndef PTRDIFF_MAX
#   define PTRDIFF_MAX _I32_MAX
#  endif
# endif
#endif /* platform dependent stuff */


/* ---------------------------------------------------------------------
 * max integer is 64-bit integer
 */
typedef __int64	intmax_t;
typedef unsigned __int64 uintmax_t;

#if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)
# define INTMAX_MIN  _I64_MIN
# define INTMAX_MAX  _I64_MAX
# define UINTMAX_MAX _UI64_MAX
#endif

/* ---------------------------------------------------------------------
 * limit for size_t (older MSVC versions lack that one)
 */
#if _MSC_VER <=1200
# if defined(_WIN64) || defined(WIN64)
#  define SIZE_MAX _UI64_MAX
#else
#  define SIZE_MAX _UI32_MAX
# endif
#endif

/* ---------------------------------------------------------------------
 * construct numerical literals with precise size
 */
#if !defined(__cplusplus) || defined(__STDC_CONSTANT_MACROS)
# define INT8_C(lit)    _VC_SI_LIT(lit,8)
# define UINT8_C(lit)   _VC_UI_LIT(lit,8)
# define INT16_C(lit)   _VC_SI_LIT(lit,16)
# define UINT16_C(lit)  _VC_UI_LIT(lit,16)
# define INT32_C(lit)   _VC_SI_LIT(lit,32)
# define UINT32_C(lit)  _VC_UI_LIT(lit,32)
# define INT64_C(lit)   _VC_SI_LIT(lit,64)
# define UINT64_C(lit)  _VC_UI_LIT(lit,64)
# define INTMAX_C(lit)  _VC_SI_LIT(lit,64)
# define UINTMAX_C(lit) _VC_UI_LIT(lit,64)
#endif

#endif
/**** EOF ****/
