/*
 *	JNPR: stdarg.h,v 1.3 2006/09/15 12:52:34 katta
 * $FreeBSD$
 */

#ifndef _MACHINE_STDARG_H_
#define	_MACHINE_STDARG_H_
#include <sys/cdefs.h>
#include <sys/_types.h>


#if __GNUC__ >= 3

#ifndef _VA_LIST_DECLARED
#define	_VA_LIST_DECLARED
typedef __va_list	va_list;
#endif
#define	va_start(v,l)	__builtin_va_start((v),l)
#define	va_end		__builtin_va_end
#define	va_arg		__builtin_va_arg
#define	va_copy		__builtin_va_copy

#else  /* __GNUC__ */


/* ---------------------------------------- */
/*	     VARARGS  for MIPS/GNU CC	    */
/* ---------------------------------------- */

#include <machine/endian.h>

/* These macros implement varargs for GNU C--either traditional or ANSI.  */

/* Define __gnuc_va_list.  */

#ifndef __GNUC_VA_LIST
#define	__GNUC_VA_LIST

typedef char * __gnuc_va_list;
typedef __gnuc_va_list va_list;

#endif /* ! __GNUC_VA_LIST */

/* If this is for internal libc use, don't define anything but
   __gnuc_va_list.  */

#ifndef _VA_MIPS_H_ENUM
#define	_VA_MIPS_H_ENUM
enum {
	__no_type_class = -1,
	__void_type_class,
	__integer_type_class,
	__char_type_class,
	__enumeral_type_class,
	__boolean_type_class,
	__pointer_type_class,
	__reference_type_class,
	__offset_type_class,
	__real_type_class,
	__complex_type_class,
	__function_type_class,
	__method_type_class,
	__record_type_class,
	__union_type_class,
	__array_type_class,
	__string_type_class,
	__set_type_class,
	__file_type_class,
	__lang_type_class
};
#endif

/* In GCC version 2, we want an ellipsis at the end of the declaration
   of the argument list.  GCC version 1 can't parse it.	 */

#if __GNUC__ > 1
#define	__va_ellipsis ...
#else
#define	__va_ellipsis
#endif


#define	va_start(__AP, __LASTARG) \
	(__AP = (__gnuc_va_list) __builtin_next_arg (__LASTARG))

#define	va_end(__AP)	((void)0)


/* We cast to void * and then to TYPE * because this avoids
   a warning about increasing the alignment requirement.  */
/* The __mips64 cases are reversed from the 32 bit cases, because the standard
   32 bit calling convention left-aligns all parameters smaller than a word,
   whereas the __mips64 calling convention does not (and hence they are
   right aligned).  */

#ifdef __mips64

#define	__va_rounded_size(__TYPE)	(((sizeof (__TYPE) + 8 - 1) / 8) * 8)

#define	__va_reg_size			8

#if defined(__MIPSEB__) || (BYTE_ORDER == BIG_ENDIAN)
#define	va_arg(__AP, __type)						\
	((__type *) (void *) (__AP = (char *)				\
	    ((((__PTRDIFF_TYPE__)__AP + 8 - 1) & -8)			\
	    + __va_rounded_size (__type))))[-1]
#else	/* ! __MIPSEB__ && !BYTE_ORDER == BIG_ENDIAN */
#define	va_arg(__AP, __type)						\
	((__AP = (char *) ((((__PTRDIFF_TYPE__)__AP + 8 - 1) & -8)	\
	    + __va_rounded_size (__type))),				\
	    *(__type *) (void *) (__AP - __va_rounded_size (__type)))
#endif	/* ! __MIPSEB__ && !BYTE_ORDER == BIG_ENDIAN */

#else	/* ! __mips64 */

#define	__va_rounded_size(__TYPE)					\
	(((sizeof (__TYPE) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))

#define	__va_reg_size 4

#if defined(__MIPSEB__) || (BYTE_ORDER == BIG_ENDIAN)
/* For big-endian machines.  */
#define	va_arg(__AP, __type)					\
	((__AP = (char *) ((__alignof__ (__type) > 4		\
	    ? ((__PTRDIFF_TYPE__)__AP + 8 - 1) & -8		\
	    : ((__PTRDIFF_TYPE__)__AP + 4 - 1) & -4)		\
	    + __va_rounded_size (__type))),			\
	*(__type *) (void *) (__AP - __va_rounded_size (__type)))
#else	/* ! __MIPSEB__ && !BYTE_ORDER == BIG_ENDIAN */
/* For little-endian machines.	*/
#define	va_arg(__AP, __type)						\
	((__type *) (void *) (__AP = (char *) ((__alignof__(__type) > 4	\
	    ? ((__PTRDIFF_TYPE__)__AP + 8 - 1) & -8			\
	    : ((__PTRDIFF_TYPE__)__AP + 4 - 1) & -4)			\
	    + __va_rounded_size(__type))))[-1]
#endif	/* ! __MIPSEB__ && !BYTE_ORDER == BIG_ENDIAN */
#endif	/* ! __mips64 */

/* Copy __gnuc_va_list into another variable of this type.  */
#define	__va_copy(dest, src)	(dest) = (src)
#define	va_copy(dest, src)	(dest) = (src)

#endif /* __GNUC__ */
#endif /* _MACHINE_STDARG_H_ */
