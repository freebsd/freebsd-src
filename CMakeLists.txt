#
#
PROJECT(libarchive C)
#
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.3 FATAL_ERROR)
SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build/cmake")
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${libarchive_BINARY_DIR}/bin)
endif()

#
# Version - read from 'version' file.
#
FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/build/version _version)
STRING(REGEX REPLACE
 "^([0-9])[0-9][0-9][0-9][0-9][0-9][0-9][a-z]?$" "\\1" _major ${_version})
STRING(REGEX REPLACE
 "^[0-9]([0-9][0-9][0-9])[0-9][0-9][0-9][a-z]?$" "\\1" _minor ${_version})
STRING(REGEX REPLACE
 "^[0-9][0-9][0-9][0-9]([0-9][0-9][0-9])[a-z]?$" "\\1" _revision ${_version})
STRING(REGEX REPLACE
 "^[0-9][0-9][0-9][0-9][0-9][0-9][0-9]([a-z]?)$" "\\1" _quality ${_version})
SET(_version_number ${_major}${_minor}${_revision})
STRING(REGEX REPLACE "[0]*([^0][0-9]*)$" "\\1" _minor ${_minor})
STRING(REGEX REPLACE "[0]*([^0][0-9]*)$" "\\1" _revision ${_revision})
#
SET(VERSION                    "${_major}.${_minor}.${_revision}${_quality}")
SET(BSDCPIO_VERSION_STRING     "${VERSION}")
SET(BSDTAR_VERSION_STRING      "${VERSION}")
SET(LIBARCHIVE_VERSION_NUMBER  "${_version_number}")
SET(LIBARCHIVE_VERSION_STRING  "${VERSION}")
# Shared library number
SET(SOVERSION                   8)

# Enable CTest/CDash support
include(CTest)

# Provide ADD_TEST_28 macro to approximate CMake 2.8 ADD_TEST(NAME).
# TODO: Require CMake 2.8 and drop this workaround (perhaps late 2010).
INCLUDE(AddTest28)

OPTION(ENABLE_OPENSSL "Enable use of OpenSSL" ON)
OPTION(ENABLE_TAR "Enable tar building" ON)
OPTION(ENABLE_TAR_SHARED "Enable dynamic build of tar" OFF)
OPTION(ENABLE_CPIO "Enable cpio building" ON)
OPTION(ENABLE_CPIO_SHARED "Enable dynamic build of cpio" OFF)
OPTION(ENABLE_XATTR "Enable extended attribute support" ON)
OPTION(ENABLE_ACL "Enable ACL support" ON)
OPTION(ENABLE_TEST "Enable unit and regression tests" ON)

IF(ENABLE_TEST)
	ENABLE_TESTING()
ENDIF(ENABLE_TEST)
IF(WIN32 AND NOT CYGWIN)
  # Currently, dynamic build only.
  SET(ENABLE_TAR_SHARED ON)
  SET(ENABLE_CPIO_SHARED ON)
ENDIF(WIN32 AND NOT CYGWIN)

IF(WIN32)
  SET(_WIN32_WINNT 0x0500 CACHE INTERNAL "Setting _WIN32_WINNT to 0x0500 for Windows 2000 APIs")
  SET(WINVER 0x0500 CACHE INTERNAL "Setting WINVER to 0x0500 for Windows 2000 APIs")
ENDIF(WIN32)

#
INCLUDE(CheckCSourceRuns)
INCLUDE(CheckFileOffsetBits)
INCLUDE(CheckFuncs)
INCLUDE(CheckHeaderDirent)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckStructMember)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckTypeExists)
INCLUDE(CheckTypeSize)

#
# Generate list.h
#
MACRO (GENERATE_LIST_H _listfile _cmlist __list_sources)
  SET(_argv ${ARGV})
  # Remove _listfile and _cmlist from _argv
  LIST(REMOVE_AT _argv 0 1)
  IF (NOT EXISTS "${_listfile}" OR
     ${_cmlist} IS_NEWER_THAN "${_listfile}")

    MESSAGE(STATUS "Generating ${_listfile}")
    FILE(WRITE ${_listfile} "")
    FOREACH (testfile ${_argv})
      IF (testfile MATCHES "^test_[^/]+[.]c$")
        FILE(STRINGS ${testfile} testvar REGEX "^DEFINE_TEST")
        FOREACH (deftest ${testvar})
          FILE(APPEND ${_listfile} "${deftest}\n")
        ENDFOREACH (deftest)
      ENDIF (testfile MATCHES "^test_[^/]+[.]c$")
    ENDFOREACH (testfile)

  ENDIF (NOT EXISTS "${_listfile}" OR
     ${_cmlist} IS_NEWER_THAN "${_listfile}")
ENDMACRO (GENERATE_LIST_H)
#
# Generate installation rules for man pages.
#
MACRO (INSTALL_MAN __mans)
  FOREACH (_man ${ARGV})
    STRING(REGEX REPLACE "^.+[.]([1-9])" "\\1" _mansect ${_man})
    INSTALL(FILES ${_man} DESTINATION "share/man/man${_mansect}")
  ENDFOREACH (_man)
ENDMACRO (INSTALL_MAN __mans)

#
# Check compress/decompress libraries
#
IF(WIN32 AND NOT CMAKE_CL_64 AND NOT CYGWIN)
  # GnuWin32 is only for Win32, not Win64.
  SET(__GNUWIN32PATH "C:/Program Files/GnuWin32")
ENDIF(WIN32 AND NOT CMAKE_CL_64 AND NOT CYGWIN)
IF(DEFINED __GNUWIN32PATH AND EXISTS "${__GNUWIN32PATH}")
  # You have to add a path availabel DLL file into PATH environment variable.
  # Maybe DLL path is "C:/Program Files/GnuWin32/bin".
  # The zlib and the bzip2 Setup program have installed programs and DLLs into
  # "C:/Program Files/GnuWin32" by default.
  # This is convenience setting for Windows.
  SET(CMAKE_PREFIX_PATH ${__GNUWIN32PATH} $(CMAKE_PREFIX_PATH))
  #
  # If you didn't use Setup program or installed into nonstandard path,
  # cmake cannot find out your zlib or bzip2 libraries and include files,
  # you should execute cmake with  -DCMAKE_PREFIX_PATH option.
  #   e.g.
  #     cmake -DCMAKE_PREFIX_PATH=<your-GnuWin32-path> <path-to-source>
  #
  # If compiling error occured in zconf.h, You may need patch to zconf.h.
  #--- zconf.h.orig	2005-07-21 00:40:26.000000000
  #+++ zconf.h	2009-01-19 11:39:10.093750000
  #@@ -286,7 +286,7 @@
  # 
  # #if 1           /* HAVE_UNISTD_H -- this line is updated by ./configure */
  # #  include <sys/types.h> /* for off_t */
  #-#  include <unistd.h>    /* for SEEK_* and off_t */
  #+#  include <stdio.h>    /* for SEEK_* and off_t */
  # #  ifdef VMS
  # #    include <unixio.h>   /* for off_t */
  # #  endif
ENDIF(DEFINED __GNUWIN32PATH AND EXISTS "${__GNUWIN32PATH}")

SET(ADDITIONAL_LIBS "")
#
# Find ZLIB
#
FIND_PACKAGE(ZLIB)
IF(ZLIB_FOUND)
  SET(HAVE_LIBZ 1)
  SET(HAVE_ZLIB_H 1)
  INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${ZLIB_LIBRARIES})
  IF(WIN32 AND NOT CYGWIN)
    SET(CMAKE_REQUIRED_INCLUDES ${ZLIB_INCLUDE_DIR})
    SET(CMAKE_REQUIRED_LIBRARIES ${ZLIB_LIBRARIES})
    CHECK_C_SOURCE_Runs(
      "#ifndef ZLIB_WINAPI\n#define ZLIB_WINAPI\n#endif\n#include <zlib.h>\nint main() {uLong f = zlibCompileFlags(); return (f&(1U<<10))?0:-1; }"
      ZLIB_WINAPI)
  ENDIF(WIN32 AND NOT CYGWIN)
ENDIF(ZLIB_FOUND)
MARK_AS_ADVANCED(CLEAR ZLIB_INCLUDE_DIR)
MARK_AS_ADVANCED(CLEAR ZLIB_LIBRARY)
#
# Find BZip2
#
FIND_PACKAGE(BZip2)
IF(BZIP2_FOUND)
  SET(HAVE_LIBBZ2 1)
  SET(HAVE_BZLIB_H 1)
  INCLUDE_DIRECTORIES(${BZIP2_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${BZIP2_LIBRARIES})
ENDIF(BZIP2_FOUND)
MARK_AS_ADVANCED(CLEAR BZIP2_INCLUDE_DIR)
MARK_AS_ADVANCED(CLEAR BZIP2_LIBRARY)
#
# Find LZMA
#
FIND_PACKAGE(LZMA)
IF(LZMA_FOUND)
  SET(HAVE_LIBLZMA 1)
  SET(HAVE_LZMA_H 1)
  INCLUDE_DIRECTORIES(${LZMA_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${LZMA_LIBRARIES})
  MARK_AS_ADVANCED(CLEAR LZMA_INCLUDE_DIR)
  MARK_AS_ADVANCED(CLEAR LZMA_LIBRARY)
ELSEIF(LZMADEC_FOUND)
  SET(HAVE_LIBLZMADEC 1)
  SET(HAVE_LZMADEC_H 1)
  INCLUDE_DIRECTORIES(${LZMADEC_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${LZMADEC_LIBRARIES})
  MARK_AS_ADVANCED(CLEAR LZMADEC_INCLUDE_DIR)
  MARK_AS_ADVANCED(CLEAR LZMADEC_LIBRARY)
ELSE(LZMA_FOUND)
  MARK_AS_ADVANCED(CLEAR LZMA_INCLUDE_DIR)
  MARK_AS_ADVANCED(CLEAR LZMA_LIBRARY)
ENDIF(LZMA_FOUND)

#
# Check headers
#
CHECK_HEADER_DIRENT()

SET(INCLUDES "")
MACRO (LA_CHECK_INCLUDE_FILE header var)
      CHECK_INCLUDE_FILES("${INCLUDES};${header}" ${var})
      IF (${var})
      	 SET(INCLUDES ${INCLUDES} ${header})
      ENDIF (${var})
ENDMACRO (LA_CHECK_INCLUDE_FILE)

# Some FreeBSD headers assume sys/types.h was already included.
LA_CHECK_INCLUDE_FILE("sys/types.h" HAVE_SYS_TYPES_H)

# Alphabetize the rest unless there's a compelling reason
LA_CHECK_INCLUDE_FILE("acl/libacl.h" HAVE_ACL_LIBACL_H)
LA_CHECK_INCLUDE_FILE("ctype.h" HAVE_CTYPE_H)
LA_CHECK_INCLUDE_FILE("direct.h" HAVE_DIRECT_H)
LA_CHECK_INCLUDE_FILE("dlfcn.h" HAVE_DLFCN_H)
LA_CHECK_INCLUDE_FILE("errno.h" HAVE_ERRNO_H)
LA_CHECK_INCLUDE_FILE("ext2fs/ext2_fs.h" HAVE_EXT2FS_EXT2_FS_H)
LA_CHECK_INCLUDE_FILE("fcntl.h" HAVE_FCNTL_H)
LA_CHECK_INCLUDE_FILE("grp.h" HAVE_GRP_H)
LA_CHECK_INCLUDE_FILE("inttypes.h" HAVE_INTTYPES_H)
LA_CHECK_INCLUDE_FILE("io.h" HAVE_IO_H)
LA_CHECK_INCLUDE_FILE("langinfo.h" HAVE_LANGINFO_H)
LA_CHECK_INCLUDE_FILE("limits.h" HAVE_LIMITS_H)
LA_CHECK_INCLUDE_FILE("linux/fs.h" HAVE_LINUX_FS_H)
LA_CHECK_INCLUDE_FILE("locale.h" HAVE_LOCALE_H)
LA_CHECK_INCLUDE_FILE("memory.h" HAVE_MEMORY_H)
LA_CHECK_INCLUDE_FILE("paths.h" HAVE_PATHS_H)
LA_CHECK_INCLUDE_FILE("poll.h" HAVE_POLL_H)
LA_CHECK_INCLUDE_FILE("process.h" HAVE_PROCESS_H)
LA_CHECK_INCLUDE_FILE("pwd.h" HAVE_PWD_H)
LA_CHECK_INCLUDE_FILE("regex.h" HAVE_REGEX_H)
LA_CHECK_INCLUDE_FILE("signal.h" HAVE_SIGNAL_H)
LA_CHECK_INCLUDE_FILE("stdarg.h" HAVE_STDARG_H)
LA_CHECK_INCLUDE_FILE("stdint.h" HAVE_STDINT_H)
LA_CHECK_INCLUDE_FILE("stdlib.h" HAVE_STDLIB_H)
LA_CHECK_INCLUDE_FILE("string.h" HAVE_STRING_H)
LA_CHECK_INCLUDE_FILE("strings.h" HAVE_STRINGS_H)
LA_CHECK_INCLUDE_FILE("sys/acl.h" HAVE_SYS_ACL_H)
LA_CHECK_INCLUDE_FILE("sys/cdefs.h" HAVE_SYS_CDEFS_H)
LA_CHECK_INCLUDE_FILE("sys/ioctl.h" HAVE_SYS_IOCTL_H)
LA_CHECK_INCLUDE_FILE("sys/mkdev.h" HAVE_SYS_MKDEV_H)
LA_CHECK_INCLUDE_FILE("sys/param.h" HAVE_SYS_PARAM_H)
LA_CHECK_INCLUDE_FILE("sys/poll.h" HAVE_SYS_POLL_H)
LA_CHECK_INCLUDE_FILE("sys/select.h" HAVE_SYS_SELECT_H)
LA_CHECK_INCLUDE_FILE("sys/stat.h" HAVE_SYS_STAT_H)
LA_CHECK_INCLUDE_FILE("sys/time.h" HAVE_SYS_TIME_H)
LA_CHECK_INCLUDE_FILE("sys/utime.h" HAVE_SYS_UTIME_H)
LA_CHECK_INCLUDE_FILE("sys/wait.h" HAVE_SYS_WAIT_H)
LA_CHECK_INCLUDE_FILE("time.h" HAVE_TIME_H)
LA_CHECK_INCLUDE_FILE("unistd.h" HAVE_UNISTD_H)
LA_CHECK_INCLUDE_FILE("utime.h" HAVE_UTIME_H)
LA_CHECK_INCLUDE_FILE("wchar.h" HAVE_WCHAR_H)
LA_CHECK_INCLUDE_FILE("wctype.h" HAVE_WCTYPE_H)
LA_CHECK_INCLUDE_FILE("windows.h" HAVE_WINDOWS_H)


#
# Some headers require extra includes when they're available.
#

#
# Find OpenSSL
#
IF(ENABLE_OPENSSL)
  FIND_PACKAGE(OpenSSL)
ENDIF()

# FreeBSD libmd
CHECK_LIBRARY_EXISTS(md "MD5Init" "" LIBMD_FOUND)
IF(LIBMD_FOUND)
  SET(CMAKE_REQUIRED_LIBRARIES "md")
  FIND_LIBRARY(LIBMD_LIBRARY NAMES md)
  LIST(APPEND ADDITIONAL_LIBS ${LIBMD_LIBRARY})
ENDIF(LIBMD_FOUND)

#
# How to prove that HASH functions, which have several names on various
# platforms, can be mapped to archive_{hash name}_init,
# archive_{hash name}_update and archive_{hash name}_final through
# archive_hash.h
#
MACRO(CHECK_MD HASH IMPLEMENTATIONS)
    IF(HAVE_SYS_TYPES_H)
        SET(HASH_HEADER_CONFIG "#define HAVE_SYS_TYPES_H 1\n")
    ELSE(HAVE_SYS_TYPES_H)
        SET(HASH_HEADER_CONFIG "")
    ENDIF(HAVE_SYS_TYPES_H)

    FOREACH(IMPLEMENTATION ${IMPLEMENTATIONS})
      IF(NOT DEFINED ARCHIVE_HASH_${HASH}_${IMPLEMENTATION})
	STRING(TOLOWER "${HASH}" lower_hash)
	STRING(TOUPPER "${HASH}" hash)

	SET(HASH_REQUIRED_INCLUDES)
	SET(TRY_HASH_REQUIRED_INCLUDES
	  "-DINCLUDE_DIRECTORIES:STRING=${CMAKE_CURRENT_SOURCE_DIR}/libarchive")
	SET(HASH_REQUIRED_LIBS)
	SET(TRY_HASH_REQUIRED_LIBS)
	IF ("${IMPLEMENTATION}" MATCHES "^OPENSSL$" AND OPENSSL_FOUND)
	    SET(HASH_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
	    SET(TRY_HASH_REQUIRED_INCLUDES
	      "${TRY_HASH_REQUIRED_INCLUDES};${OPENSSL_INCLUDE_DIR}")
	    SET(HASH_REQUIRED_LIBS ${OPENSSL_LIBRARIES})
	    SET(TRY_HASH_REQUIRED_LIBS
	        "-DLINK_LIBRARIES:STRING=${OPENSSL_LIBRARIES}")
	ELSEIF("${IMPLEMENTATION}" MATCHES "^LIBMD$" AND LIBMD_FOUND)
	    SET(TRY_HASH_REQUIRED_LIBS
	        "-DLINK_LIBRARIES:STRING=${LIBMD_LIBRARY}")
	ENDIF ("${IMPLEMENTATION}" MATCHES "^OPENSSL$" AND OPENSSL_FOUND)

	SET(SOURCE "#define ${hash}_COMPILE_TEST
#define ARCHIVE_HASH_${hash}_${IMPLEMENTATION}
#define __LIBARCHIVE_BUILD
${HASH_HEADER_CONFIG}
#include \"archive_hash.h\"

int
main(int argc, char **argv)
{
	archive_${lower_hash}_ctx ctx;

	archive_${lower_hash}_init(&ctx);
	archive_${lower_hash}_update(&ctx, *argv, argc);
	archive_${lower_hash}_final(&ctx, *argv);
	return 0;
}
")

	FILE(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/check_hash_md.c" "${SOURCE}")
	MESSAGE(STATUS "Checking support for ARCHIVE_HASH_${HASH}_${IMPLEMENTATION}")

	TRY_COMPILE(ARCHIVE_HASH_${HASH}_${IMPLEMENTATION}
	  ${CMAKE_BINARY_DIR}
	  ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/check_hash_md.c
	  CMAKE_FLAGS
	   "${TRY_HASH_REQUIRED_LIBS}"
	   "${TRY_HASH_REQUIRED_INCLUDES}"
	  OUTPUT_VARIABLE OUTPUT)

	IF (ARCHIVE_HASH_${HASH}_${IMPLEMENTATION})
	    MESSAGE(STATUS "Checking support for ARCHIVE_HASH_${HASH}_${IMPLEMENTATION} -- found")
	    IF (HASH_REQUIRED_INCLUDES)
	        INCLUDE_DIRECTORIES(${HASH_REQUIRED_INCLUDES})
	    ENDIF (HASH_REQUIRED_INCLUDES)
	    IF (HASH_REQUIRED_LIBS)
	        LIST(APPEND ADDITIONAL_LIBS ${HASH_REQUIRED_LIBS})
	        LIST(REMOVE_DUPLICATES ADDITIONAL_LIBS)
	    ENDIF (HASH_REQUIRED_LIBS)
	    BREAK()
	ENDIF (ARCHIVE_HASH_${HASH}_${IMPLEMENTATION})

	MESSAGE(STATUS "Checking support for ARCHIVE_HASH_${HASH}_${IMPLEMENTATION} -- not found")
    	FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
    	    "Checking support for ARCHIVE_HASH_${HASH}_${IMPLEMENTATION} failed with the following output:\n"
    	    "${OUTPUT}\n"
    	    "Source file was:\n${SOURCE}\n")
      ELSE(NOT DEFINED ARCHIVE_HASH_${HASH}_${IMPLEMENTATION})
        IF(ARCHIVE_HASH_${HASH}_${IMPLEMENTATION})
           BREAK()
        ENDIF(ARCHIVE_HASH_${HASH}_${IMPLEMENTATION})
      ENDIF(NOT DEFINED ARCHIVE_HASH_${HASH}_${IMPLEMENTATION})
    ENDFOREACH(IMPLEMENTATION)
ENDMACRO(CHECK_MD HASH IMPLEMENTATIONS)

#
# HASH functions on Windows is defined at archive_windows.c, thus we do not
# need the test what the functions can be mapped to archive_{hash name}_init,
# archive_{hash name}_update and archive_{hash name}_final.
# The functions on Windows use CALG_{hash name} macro to create a crypt object
# and then we need to know what CALG_{hash name} macros is available to show
# ARCHIVE_HASH_{hash name}_WIN macros because Windows 2000 and earlier version
# of Windows XP do not support SHA256, SHA384 and SHA512.
#
MACRO(CHECK_HASH_WIN HASH_LIST)
  IF(WIN32 AND NOT CYGWIN)
    FOREACH(HASH ${HASH_LIST})
      IF(NOT DEFINED ARCHIVE_HASH_${HASH}_WIN)
	STRING(TOUPPER "${HASH}" hash)
	SET(ALGID "")
	IF ("${HASH}" MATCHES "^MD5$")
	    SET(ALGID "CALG_MD5")
	ENDIF ("${HASH}" MATCHES "^MD5$")
	IF ("${HASH}" MATCHES "^SHA1$")
	    SET(ALGID "CALG_SHA1")
	ENDIF ("${HASH}" MATCHES "^SHA1$")
	IF ("${HASH}" MATCHES "^SHA256$")
	    SET(ALGID "CALG_SHA_256")
	ENDIF ("${HASH}" MATCHES "^SHA256$")
	IF ("${HASH}" MATCHES "^SHA384$")
	    SET(ALGID "CALG_SHA_384")
	ENDIF ("${HASH}" MATCHES "^SHA384$")
	IF ("${HASH}" MATCHES "^SHA512$")
	    SET(ALGID "CALG_SHA_512")
	ENDIF ("${HASH}" MATCHES "^SHA512$")

	SET(SOURCE "#define ${hash}_COMPILE_TEST
#define _WIN32_WINNT ${_WIN32_WINNT}
#define WINVER ${WINVER}
#include <windows.h>
#include <wincrypt.h>

int
main(int argc, char **argv)
{
	return ${ALGID};
}
")
	SET(SOURCE_FILE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/check_hash_win.c")

	FILE(WRITE "${SOURCE_FILE}" "${SOURCE}")
	MESSAGE(STATUS "Checking support for ARCHIVE_HASH_${HASH}_WIN")

	TRY_COMPILE(ARCHIVE_HASH_${HASH}_WIN
	  ${CMAKE_BINARY_DIR}
	  ${SOURCE_FILE}
	  CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:STRING=${CMAKE_CURRENT_SOURCE_DIR}/libarchive"
	  OUTPUT_VARIABLE OUTPUT)

	IF (ARCHIVE_HASH_${HASH}_WIN)
	    MESSAGE(STATUS
	        "Checking support for ARCHIVE_HASH_${HASH}_WIN -- found")
	ELSE (ARCHIVE_HASH_${HASH}_WIN)
	    MESSAGE(STATUS
	         "Checking support for ARCHIVE_HASH_${HASH}_WIN -- not found")
    	    FILE(APPEND
	        ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
                "Checking support for ARCHIVE_HASH_${HASH}_WIN failed with the following output:\n"
        	"${OUTPUT}\n"
        	"Source file was:\n${SOURCE}\n")
	ENDIF (ARCHIVE_HASH_${HASH}_WIN)

      ENDIF(NOT DEFINED ARCHIVE_HASH_${HASH}_WIN)
    ENDFOREACH(HASH)
  ENDIF(WIN32 AND NOT CYGWIN)
ENDMACRO(CHECK_HASH_WIN HASH_LIST)

#
# Check MD5/RMD160/SHA support
#
CHECK_MD(MD5 "LIBC;LIBMD;LIBSYSTEM;OPENSSL")
CHECK_MD(RMD160 "LIBC;OPENSSL")
CHECK_MD(SHA1 "LIBC;LIBMD;LIBSYSTEM;OPENSSL")
CHECK_MD(SHA256 "LIBC;LIBC2;LIBC3;LIBMD;LIBSYSTEM;OPENSSL")
CHECK_MD(SHA384 "LIBC;LIBC2;LIBC3;LIBSYSTEM;OPENSSL")
CHECK_MD(SHA512 "LIBC;LIBC2;LIBC3;LIBMD;LIBSYSTEM;OPENSSL")
CHECK_HASH_WIN("MD5;SHA1;SHA256;SHA384;SHA512")

#
# Find Libxml2
#
FIND_PACKAGE(LibXml2)
IF(LIBXML2_FOUND)
  INCLUDE_DIRECTORIES(${LIBXML2_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${LIBXML2_LIBRARIES})
  SET(HAVE_LIBXML2 1)
  # libxml2's include files use iconv.h
  # We need a directory path of iconv.h so that it won't fail to check
  # "libxml/xmlreader.h".
  FIND_PATH(ICONV_INCLUDE_DIR iconv.h)
  INCLUDE_DIRECTORIES(${ICONV_INCLUDE_DIR})
  SET(CMAKE_REQUIRED_INCLUDES ${ICONV_INCLUDE_DIR} ${LIBXML2_INCLUDE_DIR})
  CHECK_INCLUDE_FILES("libxml/xmlreader.h" HAVE_LIBXML_XMLREADER_H)
  SET(CMAKE_REQUIRED_INCLUDES "")
ELSE(LIBXML2_FOUND)
  #
  # Find Expat
  #
  FIND_PACKAGE(EXPAT)
  IF(EXPAT_FOUND)
    INCLUDE_DIRECTORIES(${EXPAT_INCLUDE_DIR})
    LIST(APPEND ADDITIONAL_LIBS ${EXPAT_LIBRARIES})
    SET(HAVE_LIBEXPAT 1)
    LA_CHECK_INCLUDE_FILE("expat.h" HAVE_EXPAT_H)
  ENDIF(EXPAT_FOUND)
ENDIF(LIBXML2_FOUND)

#
# Check functions
#
CHECK_SYMBOL_EXISTS(CreateHardLinkA "windows.h" HAVE_CREATEHARDLINKA)
CHECK_SYMBOL_EXISTS(CreateHardLinkW "windows.h" HAVE_CREATEHARDLINKW)
CHECK_SYMBOL_EXISTS(_CrtSetReportMode "crtdbg.h" HAVE__CrtSetReportMode)
CHECK_FUNCTION_EXISTS_GLIBC(chflags HAVE_CHFLAGS)
CHECK_FUNCTION_EXISTS_GLIBC(chown HAVE_CHOWN)
CHECK_FUNCTION_EXISTS_GLIBC(chroot HAVE_CHROOT)
CHECK_FUNCTION_EXISTS_GLIBC(fchdir HAVE_FCHDIR)
CHECK_FUNCTION_EXISTS_GLIBC(fchflags HAVE_FCHFLAGS)
CHECK_FUNCTION_EXISTS_GLIBC(fchmod HAVE_FCHMOD)
CHECK_FUNCTION_EXISTS_GLIBC(fchown HAVE_FCHOWN)
CHECK_FUNCTION_EXISTS_GLIBC(fcntl HAVE_FCNTL)
CHECK_FUNCTION_EXISTS_GLIBC(fork HAVE_FORK)
CHECK_FUNCTION_EXISTS_GLIBC(fstat HAVE_FSTAT)
CHECK_FUNCTION_EXISTS_GLIBC(ftruncate HAVE_FTRUNCATE)
CHECK_FUNCTION_EXISTS_GLIBC(futimens HAVE_FUTIMENS)
CHECK_FUNCTION_EXISTS_GLIBC(futimes HAVE_FUTIMES)
CHECK_FUNCTION_EXISTS_GLIBC(geteuid HAVE_GETEUID)
CHECK_FUNCTION_EXISTS_GLIBC(getgrgid_r HAVE_GETGRGID_R)
CHECK_FUNCTION_EXISTS_GLIBC(getgrnam_r HAVE_GETGRNAM_R)
CHECK_FUNCTION_EXISTS_GLIBC(getpwnam_r HAVE_GETPWNAM_R)
CHECK_FUNCTION_EXISTS_GLIBC(getpwuid_r HAVE_GETPWUID_R)
CHECK_FUNCTION_EXISTS_GLIBC(getpid HAVE_GETPID)
CHECK_FUNCTION_EXISTS_GLIBC(lchflags HAVE_LCHFLAGS)
CHECK_FUNCTION_EXISTS_GLIBC(lchmod HAVE_LCHMOD)
CHECK_FUNCTION_EXISTS_GLIBC(lchown HAVE_LCHOWN)
CHECK_FUNCTION_EXISTS_GLIBC(link HAVE_LINK)
CHECK_FUNCTION_EXISTS_GLIBC(lstat HAVE_LSTAT)
CHECK_FUNCTION_EXISTS_GLIBC(lutimes HAVE_LUTIMES)
CHECK_FUNCTION_EXISTS_GLIBC(memmove HAVE_MEMMOVE)
CHECK_FUNCTION_EXISTS_GLIBC(mkdir HAVE_MKDIR)
CHECK_FUNCTION_EXISTS_GLIBC(mkfifo HAVE_MKFIFO)
CHECK_FUNCTION_EXISTS_GLIBC(mknod HAVE_MKNOD)
CHECK_FUNCTION_EXISTS_GLIBC(nl_langinfo HAVE_NL_LANGINFO)
CHECK_FUNCTION_EXISTS_GLIBC(pipe HAVE_PIPE)
CHECK_FUNCTION_EXISTS_GLIBC(poll HAVE_POLL)
CHECK_FUNCTION_EXISTS_GLIBC(readlink HAVE_READLINK)
CHECK_FUNCTION_EXISTS_GLIBC(select HAVE_SELECT)
CHECK_FUNCTION_EXISTS_GLIBC(setenv HAVE_SETENV)
CHECK_FUNCTION_EXISTS_GLIBC(setlocale HAVE_SETLOCALE)
CHECK_FUNCTION_EXISTS_GLIBC(sigaction HAVE_SIGACTION)
CHECK_FUNCTION_EXISTS_GLIBC(strchr HAVE_STRCHR)
CHECK_FUNCTION_EXISTS_GLIBC(strdup HAVE_STRDUP)
CHECK_FUNCTION_EXISTS_GLIBC(strerror HAVE_STRERROR)
CHECK_FUNCTION_EXISTS_GLIBC(strncpy_s HAVE_STRNCPY_S)
CHECK_FUNCTION_EXISTS_GLIBC(strrchr HAVE_STRRCHR)
CHECK_FUNCTION_EXISTS_GLIBC(symlink HAVE_SYMLINK)
CHECK_FUNCTION_EXISTS_GLIBC(timegm HAVE_TIMEGM)
CHECK_FUNCTION_EXISTS_GLIBC(tzset HAVE_TZSET)
CHECK_FUNCTION_EXISTS_GLIBC(unsetenv HAVE_UNSETENV)
CHECK_FUNCTION_EXISTS_GLIBC(utime HAVE_UTIME)
CHECK_FUNCTION_EXISTS_GLIBC(utimes HAVE_UTIMES)
CHECK_FUNCTION_EXISTS_GLIBC(utimensat HAVE_UTIMENSAT)
CHECK_FUNCTION_EXISTS_GLIBC(vfork HAVE_VFORK)
CHECK_FUNCTION_EXISTS_GLIBC(wcrtomb HAVE_WCRTOMB)
CHECK_FUNCTION_EXISTS_GLIBC(wcscmp HAVE_WCSCMP)
CHECK_FUNCTION_EXISTS_GLIBC(wcscpy HAVE_WCSCPY)
CHECK_FUNCTION_EXISTS_GLIBC(wcslen HAVE_WCSLEN)
CHECK_FUNCTION_EXISTS_GLIBC(wctomb HAVE_WCTOMB)
CHECK_SYMBOL_EXISTS(wmemcmp "wchar.h" HAVE_WMEMCMP)
CHECK_SYMBOL_EXISTS(wmemcpy "wchar.h" HAVE_WMEMCPY)

SET(CMAKE_REQUIRED_LIBRARIES "")
CHECK_SYMBOL_EXISTS(fseeko           "stdio.h"    HAVE_FSEEKO)
CHECK_SYMBOL_EXISTS(strerror_r       "string.h"   HAVE_STRERROR_R)
CHECK_SYMBOL_EXISTS(strftime         "time.h"     HAVE_STRFTIME)
CHECK_SYMBOL_EXISTS(vprintf          "stdio.h"    HAVE_VPRINTF)
CHECK_SYMBOL_EXISTS(cygwin_conv_path "sys/cygwin.h" HAVE_CYGWIN_CONV_PATH)

CHECK_SYMBOL_EXISTS(major            "sys/mkdev.h"     MAJOR_IN_MKDEV)
CHECK_SYMBOL_EXISTS(major            "sys/sysmacros.h" MAJOR_IN_SYSMACROS)

IF(HAVE_STRERROR_R)
  SET(HAVE_DECL_STRERROR_R 1)
ENDIF(HAVE_STRERROR_R)

#
# Check defines
#
SET(headers "limits.h")
IF(HAVE_STDINT_H)
  LIST(APPEND headers "stdint.h")
ENDIF(HAVE_STDINT_H)
IF(HAVE_INTTYPES_H)
  LIST(APPEND headers "inttypes.h")
ENDIF(HAVE_INTTYPES_H)
CHECK_SYMBOL_EXISTS(EFTYPE           "errno.h"    HAVE_EFTYPE)
CHECK_SYMBOL_EXISTS(EILSEQ           "errno.h"    HAVE_EILSEQ)
CHECK_SYMBOL_EXISTS(D_MD_ORDER       "langinfo.h" HAVE_D_MD_ORDER)
CHECK_SYMBOL_EXISTS(optarg           "unistd.h"   HAVE_DECL_OPTARG)
CHECK_SYMBOL_EXISTS(optind           "unistd.h"   HAVE_DECL_OPTIND)
CHECK_SYMBOL_EXISTS(INT64_MAX        "${headers}" HAVE_DECL_INT64_MAX)
CHECK_SYMBOL_EXISTS(INT64_MIN        "${headers}" HAVE_DECL_INT64_MIN)
CHECK_SYMBOL_EXISTS(UINT32_MAX       "${headers}" HAVE_DECL_UINT32_MAX)
CHECK_SYMBOL_EXISTS(UINT64_MAX       "${headers}" HAVE_DECL_UINT64_MAX)
CHECK_SYMBOL_EXISTS(SIZE_MAX         "${headers}" HAVE_DECL_SIZE_MAX)
CHECK_SYMBOL_EXISTS(SSIZE_MAX        "limits.h"   HAVE_DECL_SSIZE_MAX)

#
# Check struct members
#
# Check for birthtime in struct stat
CHECK_STRUCT_MEMBER("struct stat" st_birthtime
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_BIRTHTIME)

# Check for high-resolution timestamps in struct stat
CHECK_STRUCT_MEMBER("struct stat" st_birthtimespec.tv_nsec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC)
CHECK_STRUCT_MEMBER("struct stat" st_mtimespec.tv_nsec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC)
CHECK_STRUCT_MEMBER("struct stat" st_mtim.tv_nsec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
CHECK_STRUCT_MEMBER("struct stat" st_mtime_n
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIME_N)
CHECK_STRUCT_MEMBER("struct stat" st_umtime
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_UMTIME)
CHECK_STRUCT_MEMBER("struct stat" st_mtime_usec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIME_USEC)
# Check for block size support in struct stat
CHECK_STRUCT_MEMBER("struct stat" st_blksize
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_BLKSIZE)
# Check for st_flags in struct stat (BSD fflags)
CHECK_STRUCT_MEMBER("struct stat" st_flags
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_FLAGS)
#
#
CHECK_STRUCT_MEMBER("struct tm" tm_sec
    "sys/types.h;sys/time.h;time.h" TIME_WITH_SYS_TIME)

#
# Check for integer types
#
# XXX There must be a way to make this simpler <sigh> XXXX
#
CHECK_TYPE_SIZE("long long int"            LONG_LONG_INT)
CHECK_TYPE_SIZE("unsigned long long"       UNSIGNED_LONG_LONG)
CHECK_TYPE_SIZE("unsigned long long int"   UNSIGNED_LONG_LONG_INT)

#
CHECK_TYPE_SIZE(dev_t       DEV_T)
IF(NOT HAVE_DEV_T)
  IF(MSVC)
    SET(dev_t "unsigned int")
  ENDIF(MSVC)
ENDIF(NOT HAVE_DEV_T)
#
CHECK_TYPE_SIZE(gid_t       GID_T)
IF(NOT HAVE_GID_T)
  IF(WIN32)
    SET(gid_t "short")
  ELSE(WIN32)
    SET(gid_t "unsigned int")
  ENDIF(WIN32)
ENDIF(NOT HAVE_GID_T)
#
CHECK_TYPE_SIZE(id_t        ID_T)
IF(NOT HAVE_ID_T)
  IF(WIN32)
    SET(id_t "short")
  ELSE(WIN32)
    SET(id_t "unsigned int")
  ENDIF(WIN32)
ENDIF(NOT HAVE_ID_T)
#
CHECK_TYPE_SIZE(int32_t    INT32_T)
IF(NOT HAVE_INT32_T)
  SET(int32_t "int")
ENDIF(NOT HAVE_INT32_T)
#
CHECK_TYPE_SIZE(int64_t     INT64_T)
IF(NOT HAVE_INT64_T)
  IF(WIN32)
    SET(int64_t __int64)
  ENDIF(WIN32)
ENDIF(NOT HAVE_INT64_T)
#
CHECK_TYPE_SIZE(intmax_t    INTMAX_T)
IF(NOT HAVE_INTMAX_T)
  SET(intmax_t "int64_t")
ENDIF(NOT HAVE_INTMAX_T)
#
CHECK_TYPE_SIZE(mode_t      MODE_T)
IF(NOT HAVE_MODE_T)
  IF(WIN32)
    SET(mode_t "unsigned short")
  ELSE(WIN32)
    SET(mode_t "int")
  ENDIF(WIN32)
ENDIF(NOT HAVE_MODE_T)
#
CHECK_TYPE_SIZE(off_t       OFF_T)
IF(NOT HAVE_OFF_T)
  SET(off_t "__int64")
ENDIF(NOT HAVE_OFF_T)
#
CHECK_TYPE_SIZE(size_t      SIZE_T)
IF(NOT HAVE_SIZE_T)
  IF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(size_t "uint64_t")
  ELSE("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(size_t   "uint32_t")
  ENDIF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
ENDIF(NOT HAVE_SIZE_T)
#
CHECK_TYPE_SIZE(ssize_t     SSIZE_T)
IF(NOT HAVE_SSIZE_T)
  IF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(ssize_t "int64_t")
  ELSE("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(ssize_t "long")
  ENDIF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
ENDIF(NOT HAVE_SSIZE_T)
#
CHECK_TYPE_SIZE(uid_t       UID_T)
IF(NOT HAVE_UID_T)
  IF(WIN32)
    SET(uid_t "short")
  ELSE(WIN32)
    SET(uid_t "unsigned int")
  ENDIF(WIN32)
ENDIF(NOT HAVE_UID_T)
#
CHECK_TYPE_SIZE(pid_t       PID_T)
IF(NOT HAVE_PID_T)
  IF(WIN32)
    SET(pid_t "int")
  ELSE(WIN32)
    MESSAGE(FATAL_ERROR "pid_t doesn't exist on this platform?")
  ENDIF(WIN32)
ENDIF(NOT HAVE_PID_T)
#
CHECK_TYPE_SIZE(uint16_t    UINT16_T)
IF(NOT HAVE_UINT16_T)
  SET(uint16_t "unsigned short")
ENDIF(NOT HAVE_UINT16_T)
#
CHECK_TYPE_SIZE(uint32_t    UINT32_T)
IF(NOT HAVE_UINT32_T)
  SET(uint32_t "unsigned int")
ENDIF(NOT HAVE_UINT32_T)
#
CHECK_TYPE_SIZE(uint64_t    UINT64_T)
IF(NOT HAVE_UINT64_T)
  IF(WIN32)
    SET(uint64_t "unsigned __int64")
  ENDIF(WIN32)
ENDIF(NOT HAVE_UINT64_T)
#
CHECK_TYPE_SIZE(uintmax_t   UINTMAX_T)
IF(NOT HAVE_UINTMAX_T)
  SET(uintmax_t "uint64_t")
ENDIF(NOT HAVE_UINTMAX_T)
#
CHECK_TYPE_SIZE(intptr_t   INTPTR_T)
IF(NOT HAVE_INTPTR_T)
  IF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(intptr_t "int64_t")
  ELSE()
    SET(intptr_t "int32_t")
  ENDIF()
ENDIF(NOT HAVE_INTPTR_T)
#
CHECK_TYPE_SIZE(uintptr_t   UINTPTR_T)
IF(NOT HAVE_UINTPTR_T)
  IF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(uintptr_t "uint64_t")
  ELSE()
    SET(uintptr_t "uint32_t")
  ENDIF()
ENDIF(NOT HAVE_UINTPTR_T)
#
CHECK_TYPE_SIZE(wchar_t     SIZEOF_WCHAR_T)
IF(HAVE_SIZEOF_WCHAR_T)
  SET(HAVE_WCHAR_T 1)
ENDIF(HAVE_SIZEOF_WCHAR_T)
#
# Check if _FILE_OFFSET_BITS macro needed for large files
#
CHECK_FILE_OFFSET_BITS()



#
# Check for Extended Attribute libraries, headers, and functions
#
IF(ENABLE_XATTR)
  LA_CHECK_INCLUDE_FILE(attr/xattr.h     HAVE_ATTR_XATTR_H)
  LA_CHECK_INCLUDE_FILE(sys/xattr.h      HAVE_SYS_XATTR_H)
  LA_CHECK_INCLUDE_FILE(sys/extattr.h      HAVE_SYS_EXTATTR_H)
  CHECK_LIBRARY_EXISTS(attr "setxattr" "" HAVE_ATTR_LIB)
  IF(HAVE_ATTR_LIB)
    SET(CMAKE_REQUIRED_LIBRARIES "attr")
  ENDIF(HAVE_ATTR_LIB)
  CHECK_SYMBOL_EXISTS(EXTATTR_NAMESPACE_USER "sys/types.h;sys/extattr.h" HAVE_DECL_EXTATTR_NAMESPACE_USER)
  CHECK_FUNCTION_EXISTS_GLIBC(extattr_get_file HAVE_EXTATTR_GET_FILE)
  CHECK_FUNCTION_EXISTS_GLIBC(extattr_list_file HAVE_EXTATTR_LIST_FILE)
  CHECK_FUNCTION_EXISTS_GLIBC(extattr_set_fd HAVE_EXTATTR_SET_FD)
  CHECK_FUNCTION_EXISTS_GLIBC(extattr_set_file HAVE_EXTATTR_SET_FILE)
  CHECK_FUNCTION_EXISTS_GLIBC(fsetxattr HAVE_FSETXATTR)
  CHECK_FUNCTION_EXISTS_GLIBC(getxattr HAVE_GETXATTR)
  CHECK_FUNCTION_EXISTS_GLIBC(lgetxattr HAVE_LGETXATTR)
  CHECK_FUNCTION_EXISTS_GLIBC(listxattr HAVE_LISTXATTR)
  CHECK_FUNCTION_EXISTS_GLIBC(llistxattr HAVE_LLISTXATTR)
  CHECK_FUNCTION_EXISTS_GLIBC(lsetxattr HAVE_LSETXATTR)
ENDIF(ENABLE_XATTR)

#
# Check for ACL libraries, headers, and functions
#
# The ACL support in libarchive is written against the POSIX1e draft,
# which was never officially approved and varies quite a bit across
# platforms.  Worse, some systems have completely non-POSIX acl functions,
# which makes the following checks rather more complex than I would like.
#
IF(ENABLE_ACL)
  CHECK_LIBRARY_EXISTS(acl "acl_get_file" "" HAVE_ACL_LIB)
  IF(HAVE_ACL_LIB)
    SET(CMAKE_REQUIRED_LIBRARIES "acl")
    FIND_LIBRARY(ACL_LIBRARY NAMES acl)
    LIST(APPEND ADDITIONAL_LIBS ${ACL_LIBRARY})
  ENDIF(HAVE_ACL_LIB)
  #
  CHECK_FUNCTION_EXISTS_GLIBC(acl_create_entry HAVE_ACL_CREATE_ENTRY)
  CHECK_FUNCTION_EXISTS_GLIBC(acl_init HAVE_ACL_INIT)
  CHECK_FUNCTION_EXISTS_GLIBC(acl_set_fd HAVE_ACL_SET_FD)
  CHECK_FUNCTION_EXISTS_GLIBC(acl_set_fd_np HAVE_ACL_SET_FD_NP)
  CHECK_FUNCTION_EXISTS_GLIBC(acl_set_file HAVE_ACL_SET_FILE)
  CHECK_TYPE_EXISTS(acl_permset_t "${INCLUDES}"    HAVE_ACL_PERMSET_T)

  # The "acl_get_perm()" function was omitted from the POSIX draft.
  # (It's a pretty obvious oversight; otherwise, there's no way to
  # test for specific permissions in a permset.)  Linux uses the obvious
  # name, FreeBSD adds _np to mark it as "non-Posix extension."
  # Test for both as a double-check that we really have POSIX-style ACL support.
  CHECK_SYMBOL_EXISTS(acl_get_perm "${INCLUDES}" HAVE_ACL_GET_PERM)
  CHECK_SYMBOL_EXISTS(acl_get_perm_np "${INCLUDES}" HAVE_ACL_GET_PERM_NP)
  CHECK_SYMBOL_EXISTS(acl_get_link "${INCLUDES}" HAVE_ACL_GET_LINK)
  CHECK_SYMBOL_EXISTS(acl_get_link_np "${INCLUDES}" HAVE_ACL_GET_LINK_NP)

  # MacOS has an acl.h that isn't POSIX.  It can be detected by
  # checking for ACL_USER
  CHECK_SYMBOL_EXISTS(ACL_USER "${INCLUDES}" HAVE_ACL_USER)
ENDIF(ENABLE_ACL)

# Generate "config.h" from "build/cmake/config.h.in"
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/build/cmake/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)
INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
ADD_DEFINITIONS(-DHAVE_CONFIG_H)

#
# Register installation of PDF documents.
#
IF(WIN32 AND NOT CYGWIN)
  #
  # On Windows platform, It's better that we install PDF documents
  # on one's computer.
  # These PDF documents are available in the release package.
  #
  IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/doc/pdf)
    INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc/pdf
            DESTINATION share/man
            FILES_MATCHING PATTERN "*.pdf"
    )
  ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/doc/pdf)
ENDIF(WIN32 AND NOT CYGWIN)
#
#
#
INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/libarchive)
#
IF(MSVC)
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
ENDIF(MSVC)
# Especially for early development, we want to be a little
# aggressive about diagnosing build problems; this can get
# relaxed somewhat in final shipping versions.
IF ("CMAKE_C_COMPILER_ID" MATCHES "^GNU$")
  ADD_DEFINITIONS(-Wall -Werror)
ENDIF ("CMAKE_C_COMPILER_ID" MATCHES "^GNU$")

IF(ENABLE_TEST)
ADD_CUSTOM_TARGET(run_all_tests)
ENDIF(ENABLE_TEST)

add_subdirectory(libarchive)
add_subdirectory(tar)
add_subdirectory(cpio)
