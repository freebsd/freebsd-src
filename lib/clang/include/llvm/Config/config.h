/* $FreeBSD$ */
/* include/llvm/Config/config.h.cmake corresponding to config.h.in. */

#ifndef CONFIG_H
#define CONFIG_H

/* Get __FreeBSD_version. */
#include <osreldate.h>

/* Exported configuration */
#include "llvm/Config/llvm-config.h"

/* Bug report URL. */
#define BUG_REPORT_URL "https://bugs.freebsd.org/submit/"

/* Define if you want backtraces on crash */
#define ENABLE_BACKTRACES

/* Define to enable crash overrides */
#define ENABLE_CRASH_OVERRIDES

/* Define to disable C++ atexit */
#define DISABLE_LLVM_DYLIB_ATEXIT

/* Define if position independent code is enabled */
#define ENABLE_PIC

/* Define if timestamp information (e.g., __DATE__) is allowed */
/* #undef ENABLE_TIMESTAMPS */

/* Define to 1 if you have the `arc4random' function. */
#define HAVE_DECL_ARC4RANDOM 1

/* Define to 1 if you have the `backtrace' function. */
/* #undef HAVE_BACKTRACE */

/* Define to 1 if you have the `bcopy' function. */
#undef HAVE_BCOPY

/* Define to 1 if you have the `closedir' function. */
#define HAVE_CLOSEDIR 1

/* Define to 1 if you have the <cxxabi.h> header file. */
#define HAVE_CXXABI_H 1

/* Define to 1 if you have the <CrashReporterClient.h> header file. */
#undef HAVE_CRASHREPORTERCLIENT_H

/* can use __crashreporter_info__ */
#undef HAVE_CRASHREPORTER_INFO

/* Define to 1 if you have the declaration of `strerror_s', and to 0 if you
   don't. */
#define HAVE_DECL_STRERROR_S 0

/* Define to 1 if you have the DIA SDK installed, and to 0 if you don't. */
/* #undef HAVE_DIA_SDK */

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define if you have the GNU dld library. */
#undef HAVE_DLD

/* Define to 1 if you have the `dlerror' function. */
#define HAVE_DLERROR 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define if dlopen() is available on this platform. */
#define HAVE_DLOPEN 1

/* Define if you have the _dyld_func_lookup function. */
#undef HAVE_DYLD

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the <execinfo.h> header file. */
#if __FreeBSD_version >= 1000052
#define HAVE_EXECINFO_H 1
#endif

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the <fenv.h> header file. */
#define HAVE_FENV_H 1

/* Define if libffi is available on this platform. */
/* #undef HAVE_FFI_CALL */

/* Define to 1 if you have the <ffi/ffi.h> header file. */
/* #undef HAVE_FFI_FFI_H */

/* Define to 1 if you have the <ffi.h> header file. */
/* #undef HAVE_FFI_H */

/* Define to 1 if you have the `futimes' function. */
#define HAVE_FUTIMES 1

/* Define to 1 if you have the `futimens' function */
#if __FreeBSD_version >= 1100056
#define HAVE_FUTIMENS 1
#endif

/* Define to 1 if you have the `getcwd' function. */
#define HAVE_GETCWD 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the `getrlimit' function. */
#define HAVE_GETRLIMIT 1

/* Define to 1 if you have the `getrusage' function. */
#define HAVE_GETRUSAGE 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if the system has the type `int64_t'. */
#define HAVE_INT64_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `isatty' function. */
#define HAVE_ISATTY 1

/* Define if you have the libdl library or equivalent. */
/* #undef HAVE_LIBDL */

/* Define to 1 if you have the `m' library (-lm). */
#undef HAVE_LIBM

/* Define to 1 if you have the `ole32' library (-lole32). */
#undef HAVE_LIBOLE32

/* Define to 1 if you have the `psapi' library (-lpsapi). */
/* #undef HAVE_LIBPSAPI */

/* Define to 1 if you have the `pthread' library (-lpthread). */
#define HAVE_LIBPTHREAD 1

/* Define to 1 if you have the `shell32' library (-lshell32). */
/* #undef HAVE_LIBSHELL32 */

/* Define to 1 if you have the 'z' library (-lz). */
#define HAVE_LIBZ 1

/* Define to 1 if you have the 'edit' library (-ledit). */
#define HAVE_LIBEDIT 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <link.h> header file. */
#define HAVE_LINK_H 1

/* Define if you can use -rdynamic. */
#define HAVE_LINK_EXPORT_DYNAMIC 1

/* Define if you can use -Wl,-R. to pass -R. to the linker, in order to add
   the current directory to the dynamic linker search path. */
#undef HAVE_LINK_R

/* Define to 1 if you have the `longjmp' function. */
/* #undef HAVE_LONGJMP */

/* Define to 1 if you have the <mach/mach.h> header file. */
/* #undef HAVE_MACH_MACH_H */

/* Define to 1 if you have the <mach-o/dyld.h> header file. */
/* #undef HAVE_MACH_O_DYLD_H */

/* Define if mallinfo() is available on this platform. */
/* #undef HAVE_MALLINFO */

/* Define to 1 if you have the <malloc.h> header file. */
/* #undef HAVE_MALLOC_H */

/* Define to 1 if you have the <malloc/malloc.h> header file. */
/* #undef HAVE_MALLOC_MALLOC_H */

/* Define to 1 if you have the `malloc_zone_statistics' function. */
/* #undef HAVE_MALLOC_ZONE_STATISTICS */

/* Define to 1 if you have the `mallctl` function. */
#define HAVE_MALLCTL 1

/* Define to 1 if you have the `mkdtemp' function. */
#define HAVE_MKDTEMP 1

/* Define to 1 if you have the `mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Define to 1 if you have the `mktemp' function. */
#define HAVE_MKTEMP 1

/* Define to 1 if you have a working `mmap' system call. */
#undef HAVE_MMAP

/* Define if mmap() uses MAP_ANONYMOUS to map anonymous pages, or undefine if
   it uses MAP_ANON */
#undef HAVE_MMAP_ANONYMOUS

/* Define if mmap() can map files into memory */
#undef HAVE_MMAP_FILE

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the `opendir' function. */
#define HAVE_OPENDIR 1

/* Define to 1 if you have the `posix_spawn' function. */
#define HAVE_POSIX_SPAWN 1

/* Define to 1 if you have the `pread' function. */
#define HAVE_PREAD 1

/* Define if libtool can extract symbol lists from object files. */
#undef HAVE_PRELOADED_SYMBOLS

/* Define to have the %a format string */
#undef HAVE_PRINTF_A

/* Have pthread_getspecific */
#define HAVE_PTHREAD_GETSPECIFIC 1

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Have pthread_mutex_lock */
#define HAVE_PTHREAD_MUTEX_LOCK 1

/* Have pthread_rwlock_init */
#define HAVE_PTHREAD_RWLOCK_INIT 1

/* Define to 1 if srand48/lrand48/drand48 exist in <stdlib.h> */
#define HAVE_RAND48 1

/* Define to 1 if you have the `readdir' function. */
#define HAVE_READDIR 1

/* Define to 1 if you have the `realpath' function. */
#define HAVE_REALPATH 1

/* Define to 1 if you have the `sbrk' function. */
#define HAVE_SBRK 1

/* Define to 1 if you have the `setenv' function. */
#define HAVE_SETENV 1

/* Define to 1 if you have the `setjmp' function. */
/* #undef HAVE_SETJMP */

/* Define to 1 if you have the `setrlimit' function. */
#define HAVE_SETRLIMIT 1

/* Define if you have the shl_load function. */
#undef HAVE_SHL_LOAD

/* Define to 1 if you have the `siglongjmp' function. */
/* #undef HAVE_SIGLONGJMP */

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the `sigsetjmp' function. */
/* #undef HAVE_SIGSETJMP */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Set to 1 if the std::isinf function is found in <cmath> */
#undef HAVE_STD_ISINF_IN_CMATH

/* Set to 1 if the std::isnan function is found in <cmath> */
#undef HAVE_STD_ISNAN_IN_CMATH

/* Define to 1 if you have the `strdup' function. */
/* #undef HAVE_STRDUP */

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the `strerror_r' function. */
#define HAVE_STRERROR_R 1

/* Define to 1 if you have the `strtoll' function. */
#define HAVE_STRTOLL 1

/* Define to 1 if you have the `strtoq' function. */
#define HAVE_STRTOQ 1

/* Define to 1 if you have the `sysconf' function. */
#undef HAVE_SYSCONF

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
#define HAVE_SYS_DIR_H 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
/* #undef HAVE_SYS_TYPES_H */

/* Define to 1 if you have the <sys/uio.h> header file. */
#define HAVE_SYS_UIO_H 1

/* Define if the setupterm() function is supported this platform. */
#define HAVE_TERMINFO 1

/* Define to 1 if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define to 1 if the system has the type `uint64_t'. */
#define HAVE_UINT64_T 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Define to 1 if the system has the type `u_int64_t'. */
#define HAVE_U_INT64_T 1

/* Define to 1 if you have the <valgrind/valgrind.h> header file. */
/* #undef HAVE_VALGRIND_VALGRIND_H */

/* Define to 1 if you have the `writev' function. */
#define HAVE_WRITEV 1

/* Define to 1 if you have the <zlib.h> header file. */
#define HAVE_ZLIB_H 1

/* Have host's _alloca */
/* #undef HAVE__ALLOCA */

/* Have host's __alloca */
/* #undef HAVE___ALLOCA */

/* Have host's __ashldi3 */
/* #undef HAVE___ASHLDI3 */

/* Have host's __ashrdi3 */
/* #undef HAVE___ASHRDI3 */

/* Have host's __chkstk */
/* #undef HAVE___CHKSTK */

/* Have host's __chkstk_ms */
/* #undef HAVE___CHKSTK_MS */

/* Have host's __cmpdi2 */
/* #undef HAVE___CMPDI2 */

/* Have host's __divdi3 */
/* #undef HAVE___DIVDI3 */

/* Define to 1 if you have the `__dso_handle' function. */
#undef HAVE___DSO_HANDLE

/* Have host's __fixdfdi */
/* #undef HAVE___FIXDFDI */

/* Have host's __fixsfdi */
/* #undef HAVE___FIXSFDI */

/* Have host's __floatdidf */
/* #undef HAVE___FLOATDIDF */

/* Have host's __lshrdi3 */
/* #undef HAVE___LSHRDI3 */

/* Have host's __main */
/* #undef HAVE___MAIN */

/* Have host's __moddi3 */
/* #undef HAVE___MODDI3 */

/* Have host's __udivdi3 */
/* #undef HAVE___UDIVDI3 */

/* Have host's __umoddi3 */
/* #undef HAVE___UMODDI3 */

/* Have host's ___chkstk */
/* #undef HAVE____CHKSTK */

/* Have host's ___chkstk_ms */
/* #undef HAVE____CHKSTK_MS */

/* Linker version detected at compile time. */
#undef HOST_LINK_VERSION

/* Installation directory for binary executables */
/* #undef LLVM_BINDIR */

/* Time at which LLVM was configured */
/* #undef LLVM_CONFIGTIME */

/* Installation directory for data files */
/* #undef LLVM_DATADIR */

/* Target triple LLVM will generate code for by default
 * Doesn't use `cmakedefine` because it is allowed to be empty.
 */
/* #undef LLVM_DEFAULT_TARGET_TRIPLE */

/* Installation directory for documentation */
/* #undef LLVM_DOCSDIR */

/* Define if LLVM is built with asserts and checks that change the layout of
   client-visible data structures.  */
#define LLVM_ENABLE_ABI_BREAKING_CHECKS

/* Define if threads enabled */
#define LLVM_ENABLE_THREADS 1

/* Define if zlib compression is available */
#define LLVM_ENABLE_ZLIB 1

/* Installation directory for config files */
/* #undef LLVM_ETCDIR */

/* Has gcc/MSVC atomic intrinsics */
#define LLVM_HAS_ATOMICS 1

/* Host triple LLVM will be executed on */
/* #undef LLVM_HOST_TRIPLE */

/* Installation directory for include files */
/* #undef LLVM_INCLUDEDIR */

/* Installation directory for .info files */
/* #undef LLVM_INFODIR */

/* Installation directory for man pages */
/* #undef LLVM_MANDIR */

/* LLVM architecture name for the native architecture, if available */
#define LLVM_NATIVE_ARCH X86

/* LLVM name for the native AsmParser init function, if available */
#define LLVM_NATIVE_ASMPARSER LLVMInitializeX86AsmParser

/* LLVM name for the native AsmPrinter init function, if available */
#define LLVM_NATIVE_ASMPRINTER LLVMInitializeX86AsmPrinter

/* LLVM name for the native Disassembler init function, if available */
#define LLVM_NATIVE_DISASSEMBLER LLVMInitializeX86Disassembler

/* LLVM name for the native Target init function, if available */
#define LLVM_NATIVE_TARGET LLVMInitializeX86Target

/* LLVM name for the native TargetInfo init function, if available */
#define LLVM_NATIVE_TARGETINFO LLVMInitializeX86TargetInfo

/* LLVM name for the native target MC init function, if available */
#define LLVM_NATIVE_TARGETMC LLVMInitializeX86TargetMC

/* Define if this is Unixish platform */
#define LLVM_ON_UNIX 1

/* Define if this is Win32ish platform */
/* #undef LLVM_ON_WIN32 */

/* Installation prefix directory */
#define LLVM_PREFIX "/usr"

/* Define if we have the Intel JIT API runtime support library */
/* #undef LLVM_USE_INTEL_JITEVENTS */

/* Define if we have the oprofile JIT-support library */
/* #undef LLVM_USE_OPROFILE */

/* Major version of the LLVM API */
#define LLVM_VERSION_MAJOR 3

/* Minor version of the LLVM API */
#define LLVM_VERSION_MINOR 8

/* Patch version of the LLVM API */
#define LLVM_VERSION_PATCH 0

/* LLVM version string */
#define LLVM_VERSION_STRING "3.8.0"

/* Define if we link Polly to the tools */
/* #undef LINK_POLLY_INTO_TOOLS */

/* Define if the OS needs help to load dependent libraries for dlopen(). */
/* #undef LTDL_DLOPEN_DEPLIBS */

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#undef LTDL_OBJDIR

/* Define to the extension used for shared libraries, say, ".so". */
#define LTDL_SHLIB_EXT ".so"

/* Define to the system default library search path. */
/* #undef LTDL_SYSSEARCHPATH */

/* Define if /dev/zero should be used when mapping RWX memory, or undefine if
   its not necessary */
#undef NEED_DEV_ZERO_FOR_MMAP

/* Define if dlsym() requires a leading underscore in symbol names. */
#undef NEED_USCORE

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "https://bugs.freebsd.org/submit/"

/* Define to the full name of this package. */
#define PACKAGE_NAME "LLVM"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "LLVM 3.8.0"

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the version of this package. */
#define PACKAGE_VERSION "3.8.0"

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define to 1 if the `S_IS*' macros in <sys/stat.h> do not work properly. */
#undef STAT_MACROS_BROKEN

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#undef TIME_WITH_SYS_TIME

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
#undef TM_IN_SYS_TIME

/* Type of 1st arg on ELM Callback */
/* #undef WIN32_ELMCB_PCSTR */

/* Define to `int' if <sys/types.h> does not define. */
#undef pid_t

/* Define to `unsigned int' if <sys/types.h> does not define. */
#undef size_t

/* Define to a function replacing strtoll */
/* #undef strtoll */

/* Define to a function implementing strtoull */
/* #undef strtoull */

/* Define to a function implementing stricmp */
/* #undef stricmp */

/* Define to a function implementing strdup */
/* #undef strdup */

/* Define to 1 if you have the `_chsize_s' function. */
/* #undef HAVE__CHSIZE_S */

#endif
