/* vms-conf.h.  Generated manually from conf.in,
   and used by config-gas.com when constructing config.h.  */

/* Define if using alloca.c.  */
#ifdef __GNUC__
#undef C_ALLOCA
#else
#define C_ALLOCA
#endif

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems.  */
#undef CRAY_STACKSEG_END

/* Define if you have alloca, as a function or macro.  */
#undef HAVE_ALLOCA

/* Define if you have <alloca.h> and it should be used (not on Ultrix).  */
#undef HAVE_ALLOCA_H

/* Define as __inline if that's what the C compiler calls it.  */
#ifdef __GNUC__
#undef inline
#else
#define inline
#endif

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
 STACK_DIRECTION > 0 => grows toward higher addresses
 STACK_DIRECTION < 0 => grows toward lower addresses
 STACK_DIRECTION = 0 => direction of growth unknown
 */
#define STACK_DIRECTION (-1)

/* Define if lex declares yytext as a char * by default, not a char[].  */
#undef YYTEXT_POINTER

/* Name of package.  */
#undef PACKAGE

/* Version of package.  */
/* Define in by config-gas.com */
/* #undef VERSION */

/* Should gas use high-level BFD interfaces?  */
#undef BFD_ASSEMBLER

/* Some assert/preprocessor combinations are incapable of handling
   certain kinds of constructs in the argument of assert.  For example,
   quoted strings (if requoting isn't done right) or newlines.  */
#ifdef __GNUC__
#undef BROKEN_ASSERT
#else
#define BROKEN_ASSERT
#endif

/* If we aren't doing cross-assembling, some operations can be optimized,
   since byte orders and value sizes don't need to be adjusted.  */
#undef CROSS_COMPILE

/* Some gas code wants to know these parameters.  */
#define TARGET_ALIAS	"vms"
#define TARGET_CPU	"vax"
#define TARGET_CANONICAL	"vax-dec-vms"
#define TARGET_OS	"vms"
#define TARGET_VENDOR	"dec"

/* Sometimes the system header files don't declare strstr.  */
#undef NEED_DECLARATION_STRSTR

/* Sometimes the system header files don't declare malloc and realloc.  */
#undef NEED_DECLARATION_MALLOC

/* Sometimes the system header files don't declare free.  */
#undef NEED_DECLARATION_FREE

/* Sometimes the system header files don't declare sbrk.  */
#undef NEED_DECLARATION_SBRK

/* Sometimes errno.h doesn't declare errno itself.  */
#undef NEED_DECLARATION_ERRNO

#undef MANY_SEGMENTS

/* The configure script defines this for some targets based on the
   target name used.  It is not always defined.  */
#undef TARGET_BYTES_BIG_ENDIAN

/* Needed only for some configurations that can produce multiple output
   formats.  */
#undef DEFAULT_EMULATION
#undef EMULATIONS
#undef USE_EMULATIONS
#undef OBJ_MAYBE_AOUT
#undef OBJ_MAYBE_BOUT
#undef OBJ_MAYBE_COFF
#undef OBJ_MAYBE_ECOFF
#undef OBJ_MAYBE_ELF
#undef OBJ_MAYBE_GENERIC
#undef OBJ_MAYBE_HP300
#undef OBJ_MAYBE_IEEE
#undef OBJ_MAYBE_SOM
#undef OBJ_MAYBE_VMS

/* Used for some of the COFF configurations, when the COFF code needs
   to select something based on the CPU type before it knows it...  */
#undef I386COFF
#undef M68KCOFF
#undef M88KCOFF

/* Using cgen code?  */
#undef USING_CGEN

/* Needed only for sparc configuration.  */
#undef DEFAULT_ARCH

/* Needed only for PowerPC Solaris.  */
#undef TARGET_SOLARIS_COMMENT

/* Needed only for SCO 5.  */
#undef SCO_ELF

/* Define if you have the remove function.  */
#define HAVE_REMOVE

/* Define if you have the sbrk function.  */
/* sbrk() is available, but we don't want gas to use it.  */
#undef HAVE_SBRK

/* Define if you have the unlink function.  */
#undef HAVE_UNLINK

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H

/* Define if you have the <memory.h> header file.  */
#undef HAVE_MEMORY_H

/* Define if you have the <stdarg.h> header file.  */
#define HAVE_STDARG_H

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H

/* Define if you have the <strings.h> header file.  */
#undef HAVE_STRINGS_H

/* Define if you have the <sys/types.h> header file.  */
#ifdef __GNUC__
#define HAVE_SYS_TYPES_H
#else
#undef HAVE_SYS_TYPES_H
#endif

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H	/* config-gas.com will make one if necessary */

/* Define if you have the <varargs.h> header file.  */
#undef HAVE_VARARGS_H

/* VMS-specific:  we need to set up EXIT_xxx here because the default
   values in as.h are inappropriate for VMS, but we also want to prevent
   as.h's inclusion of <stdlib.h> from triggering redefinition warnings.
   <stdlib.h> guards itself against multiple inclusion, so including it
   here turns as.h's later #include into a no-op.  (We can't simply use
   #ifndef HAVE_STDLIB_H here, because the <stdlib.h> in several older
   gcc-vms distributions neglects to define these two required macros.)  */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#undef EXIT_SUCCESS
#undef EXIT_FAILURE
#endif
#define EXIT_SUCCESS 1			/* SS$_NORMAL, STS$K_SUCCESS */
#define EXIT_FAILURE 0x10000002		/* (STS$K_ERROR | STS$M_INHIB_MSG) */
