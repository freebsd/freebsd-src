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

#include "alpha/freebsd.h"
#include "alpha/elf.h"

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (FreeBSD/alpha ELF)");

#undef SDB_DEBUGGING_INFO
#define SDB_DEBUGGING_INFO
#undef DBS_DEBUGGING_INFO
#define DBX_DEBUGGING_INFO

#undef PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE  \
 ((len > 1 && !strncmp (str, "gsdb", len)) ? SDB_DEBUG : DBX_DEBUG)

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-Dunix -D__alpha -D__alpha__ -D__ELF__ -D__FreeBSD__=3 -Asystem(unix) -Asystem(FreeBSD) -Acpu(alpha) -Amachine(alpha)"

#undef LINK_SPEC
#define LINK_SPEC "-m elf64alpha					\
  %{O*:-O3} %{!O*:-O1}						\
  %{assert*}							\
  %{shared:-shared}						\
  %{!shared:							\
    -dc -dp							\
    %{!nostdlib:%{!r*:%{!e*:-e _start}}}			\
    %{!static:							\
      %{rdynamic:-export-dynamic}				\
      %{!dynamic-linker:-dynamic-linker /usr/libexec/ld-elf.so.1}} \
    %{static:-static}}"

/* Provide a STARTFILE_SPEC for FreeBSD that is compatible with the
   non-aout version used on i386. */
   
#undef	STARTFILE_SPEC
#define STARTFILE_SPEC \
 "%{!shared: %{pg:gcrt1.o%s} %{!pg:%{p:gcrt1.o%s} %{!p:crt1.o%s}}} \
    %{!shared:crtbegin.o%s} %{shared:crtbeginS.o%s}"

/* Provide a ENDFILE_SPEC appropriate for FreeBSD.  Here we tack on
   the file which provides part of the support for getting C++
   file-scope static object deconstructed after exiting `main' */

#undef	ENDFILE_SPEC
#define ENDFILE_SPEC \
  "%{!shared:crtend.o%s} %{shared:crtendS.o%s}"

/* Handle #pragma weak and #pragma pack.  */

#define HANDLE_SYSV_PRAGMA

#undef SET_ASM_OP
#define SET_ASM_OP	".set"
