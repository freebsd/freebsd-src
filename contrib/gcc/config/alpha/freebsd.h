/* XXX */
/*
 * This file was derived from source obtained from NetBSD/Alpha which
 * is publicly available for ftp. The patch was developed by cgd@netbsd.org
 * during the time he worked at CMU. He claims that CMU own this patch
 * to gcc and that they have not (and will not) release the patch for
 * incorporation in FSF sources. We are supposedly able to use the patch,
 * but we are not allowed to forward it back to FSF for inclusion in
 * their source releases.
 *
 * This all has me (jb@freebsd.org) confused because (a) I see no copyright
 * messages that tell me that use is restricted; and (b) I expected that
 * the patch was originally developed from other files which are subject
 * to GPL.
 *
 * Use of this file is restricted until its CMU ownership is tested.
 */

#include "alpha/alpha.h"

#undef	WCHAR_TYPE
#define	WCHAR_TYPE "int"

#undef	WCHAR_TYPE_SIZE
#define	WCHAR_TYPE_SIZE 32

/* FreeBSD-specific things: */

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-D__FreeBSD__ -D__alpha__ -D__alpha"

/* Look for the include files in the system-defined places.  */

#undef GPLUSPLUS_INCLUDE_DIR
#define GPLUSPLUS_INCLUDE_DIR "/usr/include/g++"

#undef GCC_INCLUDE_DIR
#define GCC_INCLUDE_DIR "/usr/include"

#undef INCLUDE_DEFAULTS
#define INCLUDE_DEFAULTS                \
  {                                     \
    { GPLUSPLUS_INCLUDE_DIR, 1, 1 },    \
    { GCC_INCLUDE_DIR, 0, 0 },          \
    { 0, 0, 0 }                         \
  }


/* Under FreeBSD, the normal location of the `ld' and `as' programs is the
   /usr/bin directory.  */

#undef MD_EXEC_PREFIX
#define MD_EXEC_PREFIX "/usr/bin/"

/* Under FreeBSD, the normal location of the various *crt*.o files is the
   /usr/lib directory.  */

#undef MD_STARTFILE_PREFIX
#define MD_STARTFILE_PREFIX "/usr/lib/"


/* Provide a CPP_SPEC appropriate for FreeBSD.  Current we just deal with
   the GCC option `-posix'.  */

#undef CPP_SPEC
#define CPP_SPEC "%{posix:-D_POSIX_SOURCE}"

/* Provide an ASM_SPEC appropriate for FreeBSD. */

#undef ASM_SPEC
#define ASM_SPEC " %|"

#undef ASM_FINAL_SPEC

/* Provide a LIB_SPEC appropriate for FreeBSD.  Just select the appropriate
   libc, depending on whether we're doing profiling.  */

#undef LIB_SPEC
#define LIB_SPEC "%{!shared:%{!pg:%{!pthread:-lc}%{pthread:-lpthread -lc}}%{pg:%{!pthread:-lc_p}%{pthread:-lpthread_p -lc_p}}}"

/* Provide a LINK_SPEC appropriate for FreeBSD.  Here we provide support
   for the special GCC options -static, -assert, and -nostdlib.  */

#undef LINK_SPEC
#define LINK_SPEC \
  "%{!nostdlib:%{!r*:%{!e*:-e __start}}} -dc -dp %{static:-Bstatic} %{assert*}"

/* Output assembler code to FILE to increment profiler label # LABELNO
   for profiling a function entry.  Under FreeBSD/Alpha, the assembler does
   nothing special with -pg. */

#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)			\
	fputs ("\tjsr $28,_mcount\n", (FILE)); /* at */

/* Show that we need a GP when profiling.  */
#define TARGET_PROFILING_NEEDS_GP

#define bsd4_4
#undef HAS_INIT_SECTION

#undef PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG
