/* $Id: freebsd.h,v 1.8 1999/04/22 17:45:01 obrien Exp $ */

/* FREEBSD_NATIVE is defined when gcc is integrated into the FreeBSD
   source tree so it can be configured appropriately without using
   the GNU configure/build mechanism. */

/* Look for the include files in the system-defined places.  */

#define GPLUSPLUS_INCLUDE_DIR		"/usr/include/g++"
#define GCC_INCLUDE_DIR			"/usr/include"

/* Now that GCC knows what the include path applies to, put the G++ one first.
   C++ can now have include files that override the default C ones.  */
#undef INCLUDE_DEFAULTS
#define INCLUDE_DEFAULTS			\
  {						\
    { GPLUSPLUS_INCLUDE_DIR, "C++", 1, 1 },	\
    { GCC_INCLUDE_DIR, "GCC", 0, 0 },		\
    { 0, 0, 0, 0 }				\
  }

/* Under FreeBSD, the normal location of the compiler back ends is the
   /usr/libexec directory.  */

#undef STANDARD_EXEC_PREFIX
#undef TOOLDIR_BASE_PREFIX
#undef MD_EXEC_PREFIX

#define STANDARD_EXEC_PREFIX		"/usr/libexec/"
#define TOOLDIR_BASE_PREFIX		"/usr/libexec/"
#define MD_EXEC_PREFIX			"/usr/libexec/"

/* Under FreeBSD, the normal location of the various *crt*.o files is the
   /usr/lib directory.  */

#undef STANDARD_STARTFILE_PREFIX
#define STANDARD_STARTFILE_PREFIX	"/usr/lib/"

/* FreeBSD is 4.4BSD derived */
#define bsd4_4
