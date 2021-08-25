/* $FreeBSD$ */
/* This generated file is for internal use. Do not include it from headers. */

#ifdef CLANG_CONFIG_H
#error config.h can only be included once
#else
#define CLANG_CONFIG_H

/* Bug report URL. */
#define BUG_REPORT_URL "https://bugs.freebsd.org/submit/"

/* Default linker to use. */
#define CLANG_DEFAULT_LINKER ""

/* Default C/ObjC standard to use. */
/* #undef CLANG_DEFAULT_STD_C */

/* Default C++/ObjC++ standard to use. */
/* #undef CLANG_DEFAULT_STD_CXX */

/* Default C++ stdlib to use. */
#define CLANG_DEFAULT_CXX_STDLIB ""

/* Default runtime library to use. */
#define CLANG_DEFAULT_RTLIB ""

/* Default unwind library to use. */
#define CLANG_DEFAULT_UNWINDLIB ""

/* Default objcopy to use */
#define CLANG_DEFAULT_OBJCOPY "objcopy"

/* Default OpenMP runtime used by -fopenmp. */
#define CLANG_DEFAULT_OPENMP_RUNTIME "libomp"

/* Default architecture for OpenMP offloading to Nvidia GPUs. */
#define CLANG_OPENMP_NVPTX_DEFAULT_ARCH "sm_35"

/* Default architecture for SystemZ. */
#define CLANG_SYSTEMZ_DEFAULT_ARCH "z10"

/* Multilib suffix for libdir. */
#define CLANG_LIBDIR_SUFFIX ""

/* Relative directory for resource files */
#define CLANG_RESOURCE_DIR ""

/* Directories clang will search for headers */
#define C_INCLUDE_DIRS ""

/* Directories clang will search for configuration files */
/* #undef CLANG_CONFIG_FILE_SYSTEM_DIR */
/* #undef CLANG_CONFIG_FILE_USER_DIR */

/* Default <path> to all compiler invocations for --sysroot=<path>. */
/* #undef DEFAULT_SYSROOT */

/* Directory where gcc is installed. */
#define GCC_INSTALL_PREFIX ""

/* Define if we have libxml2 */
/* #undef CLANG_HAVE_LIBXML */

/* Define if we have sys/resource.h (rlimits) */
#define CLANG_HAVE_RLIMITS 1

/* The LLVM product name and version */
#define BACKEND_PACKAGE_STRING "LLVM 13.0.0"

/* Linker version detected at compile time. */
/* #undef HOST_LINK_VERSION */

/* pass --build-id to ld */
/* #undef ENABLE_LINKER_BUILD_ID */

/* enable x86 relax relocations by default */
#define ENABLE_X86_RELAX_RELOCATIONS 1

/* Enable each functionality of modules */
/* #undef CLANG_ENABLE_ARCMT */
/* #undef CLANG_ENABLE_OBJC_REWRITER */
/* #undef CLANG_ENABLE_STATIC_ANALYZER */

/* Spawn a new process clang.exe for the CC1 tool invocation, when necessary */
#define CLANG_SPAWN_CC1 0

#endif
