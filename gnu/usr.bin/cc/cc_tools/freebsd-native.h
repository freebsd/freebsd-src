/* $FreeBSD$ */

/* FREEBSD_NATIVE is defined when gcc is integrated into the FreeBSD
   source tree so it can be configured appropriately without using
   the GNU configure/build mechanism. */

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
   programs: /usr/libexec/<OBJFORMAT>/:STANDARD_EXEC_PREFIX:MD_EXEC_PREFIX
   libraries: MD_EXEC_PREFIX:MD_STARTFILE_PREFIX:STANDARD_STARTFILE_PREFIX
*/

#undef  TOOLDIR_BASE_PREFIX		/* Old??  This is not documented. */
#define STANDARD_EXEC_PREFIX		PREFIX"/libexec/"
#undef  MD_EXEC_PREFIX			/* We don't want one. */

/* Under FreeBSD, the normal location of the various *crt*.o files is the
   /usr/lib directory.  */

#define STANDARD_STARTFILE_PREFIX	PREFIX"/lib/"
#undef  MD_STARTFILE_PREFIX		/* We don't need one for now. */

/* For the native system compiler, we actually build libgcc in a profiled
   version.  So we should use it with -pg.  */
#define LIBGCC_SPEC "%{!pg: -lgcc} %{pg: -lgcc_p}"

/* FreeBSD is 4.4BSD derived */
#define bsd4_4
