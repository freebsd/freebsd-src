/* $FreeBSD$ */

/* FREEBSD_NATIVE is defined when gcc is integrated into the FreeBSD
   source tree so it can be configured appropriately without using
   the GNU configure/build mechanism. */

#undef  FREEBSD_NATIVE
#define FREEBSD_NATIVE 1

#undef SYSTEM_INCLUDE_DIR		/* We don't need one for now. */
#undef GCC_INCLUDE_DIR			/* We don't need one for now. */
#undef TOOL_INCLUDE_DIR			/* We don't need one for now. */
#undef LOCAL_INCLUDE_DIR		/* We don't wish to support one. */

/* Look for the include files in the system-defined places.  */
#define GPLUSPLUS_INCLUDE_DIR		PREFIX"/include/g++"
#define GCC_INCLUDE_DIR			PREFIX"/include"
#ifdef CROSS_COMPILE
#define CROSS_INCLUDE_DIR		PREFIX"/include"
#endif

/* Under FreeBSD, the normal location of the compiler back ends is the
   /usr/libexec directory.

   ``cc --print-search-dirs'' gives:
   install: STANDARD_EXEC_PREFIX/(null)
   programs: /usr/libexec/<OBJFORMAT>/:MD_EXEC_PREFIX
   libraries: MD_EXEC_PREFIX:MD_STARTFILE_PREFIX:STANDARD_STARTFILE_PREFIX

   We really don't need a "STANDARD_EXEC_PREFIX".  However w/o it,
   "--print-search-dirs" reports "install: /usr/local/lib/gcc-lib/(null)".
   It is not harmful, but is just plain wrong.  So we define a
   "STANDARD_EXEC_PREFIX" to not be misleading.  */

#undef STANDARD_EXEC_PREFIX
#undef TOOLDIR_BASE_PREFIX		/* Old??  This is not documented. */
#undef MD_EXEC_PREFIX

#define STANDARD_EXEC_PREFIX		PREFIX"/libexec/"
#define MD_EXEC_PREFIX			PREFIX"/libexec/"

/* Under FreeBSD, the normal location of the various *crt*.o files is the
   /usr/lib directory.  */

#undef STANDARD_STARTFILE_PREFIX
#undef MD_STARTFILE_PREFIX		/* We don't need one for now. */

#define STANDARD_STARTFILE_PREFIX	PREFIX"/lib/"

/* For the native system compiler, we actually build libgcc in a profiled
   version.  So we should use it with -pg.  */
#define LIBGCC_SPEC "%{!pg: -lgcc} %{pg: -lgcc_p}"

/* FreeBSD is 4.4BSD derived */
#define bsd4_4
