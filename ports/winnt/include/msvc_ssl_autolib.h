/*
 * msvc_ssl_autolib.h -- automatic link library selection
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 * --------------------------------------------------------------------
 *
 * OpenSSL library names changed over time, at least once when v1.1.0
 * emerged. For systems where the build system is inspected before
 * the build environment is created (autconf, CMake, SCONS,...) this
 * would be harmless, once it's known what libraries should be looked
 * for. With MSVC / MSBUILD that's much trickier.
 *
 * So instead of manipulating the build environment we use the build
 * tools themselves to create requests for linking the right library.
 *
 * Unless you compile OpenSSL on your own, using the precompiled
 * VC binaries from Shining Light Productions is probably easiest:
 *   https://slproweb.com/products/Win32OpenSSL.html
 *
 * If 'OPENSSL_AUTOLINK_STRICT' is defined, then target bit width,
 * runtime model and debug/release info are incoded into the library
 * file name, according to this scheme:
 *
 *  basename<width><RT><DebRel>.lib
 *
 * so that e.g. libcrypto64MTd.lib is a 64bit binary, using a static
 * multithreaded runtime in debug version. See the code below how this
 * is handled.
 * --------------------------------------------------------------------
 */
#pragma once

#if !defined(_MSC_VER)

# error use this header only with Micro$oft Visual C compilers!

#elif defined(OPENSSL_NO_AUTOLINK)

# pragma message("automatic OpenSSL library selection disabled")

#elif defined(OPENSSL_AUTOLINK_STRICT)

 /* ---------------------------------------------------------------- */
/*  selection dance to get the right libraries linked               */
/* ---------------------------------------------------------------- */

/* --*-- check if this a DEBUG or a RELEASE build --*--
 * The '_DEBUG' macro is only set for DEBUG build variants
 */
# ifdef _DEBUG
#  define LTAG_DEBUG "d"
# else
#  define LTAG_DEBUG ""
# endif

/* --*-- check platform (32 bit vs. 64 bit) --*--
 * '_WIN64' is defined for X64 Platform only
 */
# ifdef _WIN64
#  define LTAG_SIZE "64"
# else
#  define LTAG_SIZE "32"
# endif

/* --*-- check VC runtime model --*--
 * '_DLL' is set if a runtime-in-a-dll code generation model is chosen.
 * Note that we do not check for the single-threaded static runtime.
 * This would make no sense, since the Windows Build *uses* threads and
 * therefore needs a multithread runtime anyway. And Micro$oft decided
 * somewhere after VS2008 to drop that model anyway, so it's no longer
 * available on newer platforms.
 */
# ifdef _DLL
#  #define LTAG_RTLIB "MD"
# else
#  define LTAG_RTLIB "MT"
# endif

/* --*-- place linker request in object file --*--
 * Here we come to the reason for all that trouble: the library names
 * to link depend on the OpenSSL version...
 *
 * Before v1.1.0, the 'old' (ancient?) name 'libeay32' was used,
 * no matter what platform. (The corresponding 'ssleay32.lib' was/is
 * not used by NTP.) Since v1.1.0, the name stems are libcrypto
 * and libssl, and they contain the platform size (32 or 64).
 *
 * So we use '#pragma comment(lib,...)' to place a proper linker
 * request in the object file, depending on the SSL version and the
 * build variant.
 */

#  if OPENSSL_VERSION_NUMBER >= 0x10100000L
#   pragma comment(lib, "libcrypto" LTAG_SIZE LTAG_RTLIB LTAG_DEBUG ".lib")
#  else
#   pragma comment(lib, "libeay32" LTAG_RTLIB LTAG_DEBUG ".lib")
#  endif

# else

# if OPENSSL_VERSION_NUMBER >= 0x10100000L
#  pragma comment(lib, "libcrypto.lib")
# else
#  pragma comment(lib, "libeay32.lib")
# endif

#endif /*!defined(OPENSSL_NO_AUTOLINK)*/
