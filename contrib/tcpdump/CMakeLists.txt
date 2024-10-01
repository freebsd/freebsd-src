if(WIN32)
    #
    # We need 3.12 or later, so that we can set policy CMP0074; see
    # below.
    #
    cmake_minimum_required(VERSION 3.12)
else(WIN32)
    #
    # For now:
    #
    #  if this is a version of CMake less than 3.5, require only
    #  2.8.12, just in case somebody is configuring with CMake
    #  on a "long-term support" version # of some OS and that
    #  version supplies an older version of CMake;
    #
    #  otherwise, require 3.5, so we don't get messages warning
    #  that support for versions of CMake lower than 3.5 is
    #  deprecated.
    #
    if(CMAKE_VERSION VERSION_LESS "3.5")
        cmake_minimum_required(VERSION 2.8.12)
    else()
        cmake_minimum_required(VERSION 3.5)
    endif()
endif(WIN32)

#
# We want find_path() and find_library() to honor {packagename}_ROOT,
# as that appears to be the standard way to say "hey, look here for
# this package" from the command line.
#
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

#
# OK, this is a pain.
#
# When building on NetBSD, with a libpcap installed from pkgsrc,
# a -Wl,-rpath,/usr/pkg/lib option is added to the options when
# linking tcpdump.  This puts /usr/pkg/lib into the run-time path.
#
# However, by default, CMake adds a rule to the install CMake script
# a CMake command (using an undocumented subcommand of file()) that
# strips /usr/pkg/lib *out* of the run-time path; the message in the
# output for the "install" target is
#
#    -- Set runtime path of "{target-directory}/tcpdump" to ""
#
# I am not certain what the rationale is for doing this, but a
# *consequence* of this is that, when you run the installed tcpdump,
# it fails to find libpcap.so:
#
#    $ {target-directory}/tcpdump -h
#    {target-directory}/tcpdump: Shared object "libpcap.so.0" not found
#
# It also appears to be the case that, on Ubuntu 22.04, FreeBSD 12,
# DragonFly BSD 5.8, OpenBSD 6.6, and Solaris 11.4,
#
# On Ubuntu and Solaris, even if you have a libpcap in /usr/local, you
# have to provide not only -I/usr/local/include and -L/usr/local/lib,
# you also must provide -Wl,-rpath,/usr/local/lib in order to have
# the run-time linker look in /usr/local/lib for libpcap.  If it's not
# specified, then, if the shared library major version number of the
# libpcap in /usr/lib is the same as the shared major version number
# of the libpcap in /usr/local/lib, the run-time linker will find the
# libpcap in /usr/lib; if the versions are different, the run-time
# linker will fail to find the libpcap in /usr/lib, so the program will
# fail to run.
#
# We suppress this by setting CMAKE_INSTALL_RPATH_USE_LINK_PATH to TRUE;
# as the documentation for that variable says:
#
#    Add paths to linker search and installed rpath.
#
#    CMAKE_INSTALL_RPATH_USE_LINK_PATH is a boolean that if set to True
#    will append to the runtime search path (rpath) of installed
#    binaries any directories outside the project that are in the linker
#    search path or contain linked library files. The directories are
#    appended after the value of the INSTALL_RPATH target property.
#
# If, for whatever reason, directories in which we search for external
# libraries, other than the standard system library directories, are
# added to the executable's rpath in the build process, we most
# definitely want them in the installed image's rpath if they are
# necessary in order to find the libraries at run time.
#
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

#
# We explicitly indicate what languages are used in tcpdump to avoid
# checking for a C++ compiler.
#
# One reason to avoid that check is that there's no need to waste
# configuration time performing it.
#
# Another reason is that:
#
# CMake will try to determine the sizes of some data types, including
# void *, early in the process of configuration; apparently, it's done
# as part of processing the project() command.
#
# At least as of CMake 2.8.6, it does so by checking the size of
# "void *" in C, setting CMAKE_C_SIZEOF_DATA_PTR based on that,
# setting CMAKE_SIZEOF_VOID_P to that, and then checking the size
# of "void *" in C++, setting CMAKE_CXX_SIZEOF_DATA_PTR based on
# that, and then setting CMAKE_SIZEOF_VOID_P to *that*.
#
# The compile tests include whatever C flags may have been provided
# to CMake in the CFLAGS and CXXFLAGS environment variables.
#
# If you set an architecture flag such as -m32 or -m64 in CFLAGS
# but *not* in CXXFLAGS, the size for C++ will win, and hilarity
# will ensue.
#
# Or if, at least on Solaris, you have a newer version of GCC
# installed, but *not* a newer version of G++, and you have Oracle
# Studio installed, it will find GCC, which will default to building
# 64-bit, and Oracle Studio's C++ compiler, which will default to
# building 32-bit, the size for C++ will win, and, again, hilarity
# will ensue.
#
project(tcpdump C)

#
# Export the size of void * as SIZEOF_VOID_P so that it can be
# tested with #if.
#
set(SIZEOF_VOID_P "${CMAKE_SIZEOF_VOID_P}")

#
# Show the bit width for which we're compiling.
# This can help debug problems if you're dealing with a compiler that
# defaults to generating 32-bit code even when running on a 64-bit
# platform, and where that platform may provide only 64-bit versions of
# libraries that we might use (looking at *you*, Oracle Studio!).
#
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(STATUS "Building 32-bit")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(STATUS "Building 64-bit")
endif()

#
# Solaris pkg-config is annoying.  For at least one package (D-Bus, I'm
# looking at *you*!), there are separate include files for 32-bit and
# 64-bit builds (I guess using "unsigned long long" as a 64-bit integer
# type on a 64-bit build is like crossing the beams or something), and
# there are two separate .pc files, so if we're doing a 32-bit build we
# should make sure we look in /usr/lib/pkgconfig for .pc files and if
# we're doing a 64-bit build we should make sure we look in
# /usr/lib/amd64/pkgconfig for .pc files.
#
if(CMAKE_SYSTEM_NAME STREQUAL "SunOS" AND CMAKE_SYSTEM_VERSION MATCHES "5[.][0-9.]*")
    #
    # Note: string(REPLACE) does not appear to support using ENV{...}
    # as an argument, so we set a variable and then use set() to set
    # the environment variable.
    #
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        #
        # 64-bit build.  If /usr/lib/pkgconfig appears in the path,
        # prepend /usr/lib/amd64/pkgconfig to it; otherwise,
        # put /usr/lib/amd64 at the end.
        #
        if((NOT DEFINED ENV{PKG_CONFIG_PATH}) OR "$ENV{PKG_CONFIG_PATH}" EQUAL "")
            #
            # Not set, or empty.  Set it to /usr/lib/amd64/pkgconfig.
            #
            set(fixed_path "/usr/lib/amd64/pkgconfig")
        elseif("$ENV{PKG_CONFIG_PATH}" MATCHES "/usr/lib/pkgconfig")
            #
            # It contains /usr/lib/pkgconfig.  Prepend
            # /usr/lib/amd64/pkgconfig to /usr/lib/pkgconfig.
            #
            string(REPLACE "/usr/lib/pkgconfig"
                "/usr/lib/amd64/pkgconfig:/usr/lib/pkgconfig"
                fixed_path "$ENV{PKG_CONFIG_PATH}")
        else()
            #
            # Not empty, but doesn't contain /usr/lib/pkgconfig.
            # Append /usr/lib/amd64/pkgconfig to it.
            #
            set(fixed_path "$ENV{PKG_CONFIG_PATH}:/usr/lib/amd64/pkgconfig")
        endif()
        set(ENV{PKG_CONFIG_PATH} "${fixed_path}")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        #
        # 32-bit build.  If /usr/amd64/lib/pkgconfig appears in the path,
        # prepend /usr/lib/pkgconfig to it.
        #
        if("$ENV{PKG_CONFIG_PATH}" MATCHES "/usr/lib/amd64/pkgconfig")
            #
            # It contains /usr/lib/amd64/pkgconfig.  Prepend
            # /usr/lib/pkgconfig to /usr/lib/amd64/pkgconfig.
            #
            string(REPLACE "/usr/lib/amd64/pkgconfig"
                "/usr/lib/pkgconfig:/usr/lib/amd64/pkgconfig"
                fixed_path "$ENV{PKG_CONFIG_PATH}")
            set(ENV{PKG_CONFIG_PATH} "${fixed_path}")
        endif()
    endif()
endif()

#
# For checking if a compiler flag works and adding it if it does.
#
include(CheckCCompilerFlag)
macro(check_and_add_compiler_option _option)
    message(STATUS "Checking C compiler flag ${_option}")
    string(REPLACE "=" "-" _temp_option_variable ${_option})
    string(REGEX REPLACE "^-" "" _option_variable ${_temp_option_variable})
    check_c_compiler_flag("${_option}" ${_option_variable})
    if(${${_option_variable}})
        set(C_ADDITIONAL_FLAGS "${C_ADDITIONAL_FLAGS} ${_option}")
    endif()
endmacro()

#
# If we're building with Visual Studio, we require Visual Studio 2015,
# in order to get sufficient C99 compatibility.  Check for that.
#
# If not, try the appropriate flag for the compiler to enable C99
# features.
#
set(C_ADDITIONAL_FLAGS "")
if(MSVC)
    if(MSVC_VERSION LESS 1900)
        message(FATAL_ERROR "Visual Studio 2015 or later is required")
    endif()

    #
    # Treat source files as being in UTF-8 with MSVC if it's not using
    # the Clang front end.
    # We assume that UTF-8 source is OK with other compilers and with
    # MSVC if it's using the Clang front end.
    #
    if(NOT ${CMAKE_C_COMPILER} MATCHES "clang*")
        set(C_ADDITIONAL_FLAGS "${C_ADDITIONAL_FLAGS} /utf-8")
    endif(NOT ${CMAKE_C_COMPILER} MATCHES "clang*")
else(MSVC)
    #
    # Try to enable as many C99 features as we can.
    # At minimum, we want C++/C99-style // comments.
    #
    # Newer versions of compilers might default to supporting C99, but
    # older versions may require a special flag.
    #
    # Prior to CMake 3.1, setting CMAKE_C_STANDARD will not have any effect,
    # so, unless and until we require CMake 3.1 or later, we have to do it
    # ourselves on pre-3.1 CMake, so we just do it ourselves on all versions
    # of CMake.
    #
    # Note: with CMake 3.1 through 3.5, the only compilers for which CMake
    # handles CMAKE_C_STANDARD are GCC and Clang.  3.6 adds support only
    # for Intel C; 3.9 adds support for PGI C, Sun C, and IBM XL C, and
    # 3.10 adds support for Cray C and IAR C, but no version of CMake has
    # support for HP C.  Therefore, even if we use CMAKE_C_STANDARD with
    # compilers for which CMake supports it, we may still have to do it
    # ourselves on other compilers.
    #
    # See the CMake documentation for the CMAKE_<LANG>_COMPILER_ID variables
    # for a list of compiler IDs.
    #
    # XXX - this just tests whether the option works and adds it if it does.
    # We don't test whether it's necessary in order to get the C99 features
    # that we use; if we ever have a user who tries to compile with a compiler
    # that can't be made to support those features, we can add a test to make
    # sure we actually *have* C99 support.
    #
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR
       CMAKE_C_COMPILER_ID MATCHES "Clang")
        check_and_add_compiler_option("-std=gnu99")
    elseif(CMAKE_C_COMPILER_ID MATCHES "XL")
        #
        # We want support for extensions picked up for GNU C compatibility,
        # so we use -qlanglvl=extc99.
        #
        check_and_add_compiler_option("-qlanglvl=extc99")
    elseif(CMAKE_C_COMPILER_ID MATCHES "HP")
        check_and_add_compiler_option("-AC99")
    elseif(CMAKE_C_COMPILER_ID MATCHES "Sun")
        check_and_add_compiler_option("-xc99")
    elseif(CMAKE_C_COMPILER_ID MATCHES "Intel")
        check_and_add_compiler_option("-c99")
    endif()
endif(MSVC)

set(LIBRARY_NAME netdissect)

###################################################################
#   Parameters
###################################################################

option(WITH_SMI "Build with libsmi, if available" ON)
option(WITH_CRYPTO "Build with OpenSSL/libressl libcrypto, if available" ON)
option(WITH_CAPSICUM "Build with Capsicum security functions, if available" ON)
option(WITH_CAP_NG "Use libcap-ng, if available" ON)
option(ENABLE_SMB "Build with the SMB dissector" OFF)

#
# String parameters.  Neither of them are set, initially; only if the
# user explicitly configures them are they set.
#
# WITH_CHROOT is STRING, not PATH, as the directory need not exist
# when CMake is run.
#
set(WITH_CHROOT CACHE STRING
    "Directory to which to chroot when dropping privileges")
set(WITH_USER CACHE STRING
    "User to whom to set the UID when dropping privileges")

#
# By default, build universal with the appropriate set of architectures
# for the OS on which we're doing the build.
#
if(APPLE AND "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "")
    #
    # Get the major version of Darwin.
    #
    string(REGEX MATCH "^([0-9]+)" SYSTEM_VERSION_MAJOR "${CMAKE_SYSTEM_VERSION}")

    if(SYSTEM_VERSION_MAJOR EQUAL 9)
        #
        # Leopard.  Build for x86 and 32-bit PowerPC, with
        # x86 first.  (That's what Apple does.)
        #
        set(CMAKE_OSX_ARCHITECTURES "i386;ppc")
    elseif(SYSTEM_VERSION_MAJOR EQUAL 10)
        #
        # Snow Leopard.  Build for x86-64 and x86, with
        # x86-64 first.  (That's what Apple does.)
        #
        set(CMAKE_OSX_ARCHITECTURES "x86_64;i386")
    endif()
endif()

###################################################################
#   Versioning
###################################################################

# Get, parse, format and set tcpdump's version string from
# [tcpdump_root]/VERSION for later use.

# Get MAJOR, MINOR, PATCH & SUFFIX
file(STRINGS ${tcpdump_SOURCE_DIR}/VERSION
    PACKAGE_VERSION
    LIMIT_COUNT 1 # Read only the first line
)

######################################
# Project settings
######################################

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${tcpdump_SOURCE_DIR}
)

if(MSVC)
    add_definitions(-D__STDC__)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif(MSVC)

if(MSVC)
    if (USE_STATIC_RT)
        MESSAGE(STATUS "Use STATIC runtime")
        set(NAME_RT MT)
        set (CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
        set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
        set (CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set (CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

        set (CMAKE_C_FLAGS_MINSIZEREL       "${CMAKE_C_FLAGS_MINSIZEREL} /MT")
        set (CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MT")
        set (CMAKE_C_FLAGS_RELEASE          "${CMAKE_C_FLAGS_RELEASE} /MT")
        set (CMAKE_C_FLAGS_DEBUG            "${CMAKE_C_FLAGS_DEBUG} /MTd")
    else (USE_STATIC_RT)
        MESSAGE(STATUS "Use DYNAMIC runtime")
        set(NAME_RT MD)
        set (CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} /MD")
        set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MD")
        set (CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} /MD")
        set (CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} /MDd")

        set (CMAKE_C_FLAGS_MINSIZEREL       "${CMAKE_C_FLAGS_MINSIZEREL} /MD")
        set (CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MD")
        set (CMAKE_C_FLAGS_RELEASE          "${CMAKE_C_FLAGS_RELEASE} /MD")
        set (CMAKE_C_FLAGS_DEBUG            "${CMAKE_C_FLAGS_DEBUG} /MDd")
   endif (USE_STATIC_RT)
endif(MSVC)

###################################################################
#   Detect available platform features
###################################################################

include(CMakePushCheckState)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckStructHasMember)
include(CheckVariableExists)
include(CheckTypeSize)

#
# Get the size of a time_t, to know whether it's 32-bit or 64-bit.
#
cmake_push_check_state()
set(CMAKE_EXTRA_INCLUDE_FILES time.h)
check_type_size("time_t" SIZEOF_TIME_T)
cmake_pop_check_state()

#
# Header files.
#
check_include_file(rpc/rpc.h HAVE_RPC_RPC_H)
check_include_file(net/if.h HAVE_NET_IF_H)
if(HAVE_RPC_RPC_H)
    check_include_files("rpc/rpc.h;rpc/rpcent.h" HAVE_RPC_RPCENT_H)
endif(HAVE_RPC_RPC_H)

#
# Functions.
#
check_function_exists(strlcat HAVE_STRLCAT)
check_function_exists(strlcpy HAVE_STRLCPY)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strsep HAVE_STRSEP)

#
# Find library needed for gethostbyaddr.
# NOTE: if you hand check_library_exists as its last argument a variable
# that's been set, it skips the test, so we need different variables.
#
set(TCPDUMP_LINK_LIBRARIES "")
if(WIN32)
    #
    # We need winsock2.h and ws2tcpip.h.
    #
    cmake_push_check_state()
    set(CMAKE_REQUIRED_LIBRARIES ws2_32)
    check_symbol_exists(gethostbyaddr "winsock2.h;ws2tcpip.h" LIBWS2_32_HAS_GETHOSTBYADDR)
    cmake_pop_check_state()
    if(LIBWS2_32_HAS_GETHOSTBYADDR)
        set(TCPDUMP_LINK_LIBRARIES ws2_32 ${TCPDUMP_LINK_LIBRARIES})
    else(LIBWS2_32_HAS_GETHOSTBYADDR)
        message(FATAL_ERROR "gethostbyaddr is required, but wasn't found")
    endif(LIBWS2_32_HAS_GETHOSTBYADDR)
else(WIN32)
    check_function_exists(gethostbyaddr STDLIBS_HAVE_GETHOSTBYADDR)
    if(NOT STDLIBS_HAVE_GETHOSTBYADDR)
        check_library_exists(socket gethostbyaddr "" LIBSOCKET_HAS_GETHOSTBYADDR)
        if(LIBSOCKET_HAS_GETHOSTBYADDR)
            set(TCPDUMP_LINK_LIBRARIES ${TCPDUMP_LINK_LIBRARIES} socket)
        else(LIBSOCKET_HAS_GETHOSTBYADDR)
            check_library_exists(nsl gethostbyaddr "" LIBNSL_HAS_GETHOSTBYADDR)
            if(LIBNSL_HAS_GETHOSTBYADDR)
                set(TCPDUMP_LINK_LIBRARIES ${TCPDUMP_LINK_LIBRARIES} nsl)
            else(LIBNSL_HAS_GETHOSTBYADDR)
                check_library_exists(network gethostbyaddr "" LIBNETWORK_HAS_GETHOSTBYADDR)
                if(LIBNETWORK_HAS_GETHOSTBYADDR)
                    set(TCPDUMP_LINK_LIBRARIES ${TCPDUMP_LINK_LIBRARIES} network)
                else(LIBNETWORK_HAS_GETHOSTBYADDR)
                    message(FATAL_ERROR "gethostbyaddr is required, but wasn't found")
                endif(LIBNETWORK_HAS_GETHOSTBYADDR)
            endif(LIBNSL_HAS_GETHOSTBYADDR)
        endif(LIBSOCKET_HAS_GETHOSTBYADDR)
    endif(NOT STDLIBS_HAVE_GETHOSTBYADDR)
endif(WIN32)

#
# This may require additional libraries.
#
cmake_push_check_state()
set(CMAKE_REQUIRED_LIBRARIES ${TCPDUMP_LINK_LIBRARIES})
check_function_exists(getservent STDLIBS_HAVE_GETSERVENT)
if(STDLIBS_HAVE_GETSERVENT)
    set(HAVE_GETSERVENT TRUE)
else(STDLIBS_HAVE_GETSERVENT)
    #
    # Some platforms may need -lsocket for getservent.
    #
    set(CMAKE_REQUIRED_LIBRARIES socket ${TCPDUMP_LINK_LIBRARIES})
    check_function_exists(getservent LIBSOCKET_HAS_GETSERVENT)
    if(LIBSOCKET_HAS_GETSERVENT)
        set(HAVE_GETSERVENT TRUE)
        set(TCPDUMP_LINK_LIBRARIES socket ${TCPDUMP_LINK_LIBRARIES})
    endif(LIBSOCKET_HAS_GETSERVENT)
endif(STDLIBS_HAVE_GETSERVENT)
cmake_pop_check_state()

#
# Make sure we have snprintf(); we require it.
# We use check_symbol_exists(), as it isn't necessarily an external
# function - in Visual Studio, for example, it is an inline function
# calling an external function.
#
check_symbol_exists(snprintf "stdio.h" HAVE_SNPRINTF)
if(NOT HAVE_SNPRINTF)
    message(FATAL_ERROR "snprintf() is required but wasn't found")
endif()

#
# Require a proof of suitable snprintf(3), same as in Autoconf.
#
include(CheckCSourceRuns)
check_c_source_runs("
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>

int main()
{
  char buf[100];
  uint64_t t = (uint64_t)1 << 32;

  snprintf(buf, sizeof(buf), \"%zu\", sizeof(buf));
  if (strncmp(buf, \"100\", sizeof(buf)))
    return 1;

  snprintf(buf, sizeof(buf), \"%zd\", -sizeof(buf));
  if (strncmp(buf, \"-100\", sizeof(buf)))
    return 2;

  snprintf(buf, sizeof(buf), \"%\" PRId64, -t);
  if (strncmp(buf, \"-4294967296\", sizeof(buf)))
    return 3;

  snprintf(buf, sizeof(buf), \"0o%\" PRIo64, t);
  if (strncmp(buf, \"0o40000000000\", sizeof(buf)))
    return 4;

  snprintf(buf, sizeof(buf), \"0x%\" PRIx64, t);
  if (strncmp(buf, \"0x100000000\", sizeof(buf)))
    return 5;

  snprintf(buf, sizeof(buf), \"%\" PRIu64, t);
  if (strncmp(buf, \"4294967296\", sizeof(buf)))
    return 6;

  return 0;
}

"
    SUITABLE_SNPRINTF
)
if(NOT SUITABLE_SNPRINTF)
    message(FATAL_ERROR
"The snprintf(3) implementation in this libc is not suitable,
tcpdump would not work correctly even if it managed to compile."
    )
endif()

check_function_exists(getopt_long HAVE_GETOPT_LONG)
check_function_exists(setlinebuf HAVE_SETLINEBUF)
#
# For Windows,  don't need to waste time checking for fork() or vfork().
#
if(NOT WIN32)
    check_function_exists(fork HAVE_FORK)
    check_function_exists(vfork HAVE_VFORK)
endif(NOT WIN32)

#
# Some platforms may need -lnsl for getrpcbynumber.
#
cmake_push_check_state()
set(CMAKE_REQUIRED_LIBRARIES ${TCPDUMP_LINK_LIBRARIES})
check_function_exists(getrpcbynumber STDLIBS_HAVE_GETRPCBYNUMBER)
if(STDLIBS_HAVE_GETRPCBYNUMBER)
    set(HAVE_GETRPCBYNUMBER TRUE)
else(STDLIBS_HAVE_GETRPCBYNUMBER)
    set(CMAKE_REQUIRED_LIBRARIES ${TCPDUMP_LINK_LIBRARIES} nsl)
    check_function_exists(getrpcbynumber LIBNSL_HAS_GETRPCBYNUMBER)
    if(LIBNSL_HAS_GETRPCBYNUMBER)
        set(HAVE_GETRPCBYNUMBER TRUE)
        set(TCPDUMP_LINK_LIBRARIES ${TCPDUMP_LINK_LIBRARIES} nsl)
    endif(LIBNSL_HAS_GETRPCBYNUMBER)
endif(STDLIBS_HAVE_GETRPCBYNUMBER)
cmake_pop_check_state()

#
# This requires the libraries we require, as ether_ntohost might be
# in one of those libraries.  That means we have to do this after
# we check for those libraries.
#
# You are in a twisty little maze of UN*Xes, all different.
# Some might not have ether_ntohost().
# Some might have it and declare it in <net/ethernet.h>.
# Some might have it and declare it in <netinet/ether.h>
# Some might have it and declare it in <sys/ethernet.h>.
# Some might have it and declare it in <arpa/inet.h>.
# Some might have it and declare it in <netinet/if_ether.h>.
# Some might have it and not declare it in any header file.
#
# Before you is a C compiler.
#
cmake_push_check_state()
set(CMAKE_REQUIRED_LIBRARIES ${TCPDUMP_LINK_LIBRARIES})
check_function_exists(ether_ntohost HAVE_ETHER_NTOHOST)
if(HAVE_ETHER_NTOHOST)
    #
    # OK, we have ether_ntohost().  We don't check whether it's buggy,
    # as we assume any system that has CMake is likely to be new enough
    # that, if it has ether_ntohost(), whatever bug is checked for in
    # autotools is fixed; we just decide to use it.
    #
    set(USE_ETHER_NTOHOST TRUE)

    #
    # Is it declared in <net/ethernet.h>?
    #
    # This test fails if we don't have <net/ethernet.h> or if we do
    # but it doesn't declare ether_ntohost().
    #
    check_symbol_exists(ether_ntohost net/ethernet.h NET_ETHERNET_H_DECLARES_ETHER_NTOHOST)
    if(NET_ETHERNET_H_DECLARES_ETHER_NTOHOST)
        #
        # Yes - we have it declared.
        #
        set(HAVE_DECL_ETHER_NTOHOST TRUE)
    endif()
    #
    # Did that succeed?
    #
    if(NOT HAVE_DECL_ETHER_NTOHOST)
        #
        # No - how about <netinet/ether.h>, as on Linux?
        #
        # This test fails if we don't have <netinet/ether.h>
        # or if we do but it doesn't declare ether_ntohost().
        #
        check_symbol_exists(ether_ntohost netinet/ether.h NETINET_ETHER_H_DECLARES_ETHER_NTOHOST)
        if(NETINET_ETHER_H_DECLARES_ETHER_NTOHOST)
            #
            # Yes - we have it declared.
            #
            set(HAVE_DECL_ETHER_NTOHOST TRUE)
        endif()
    endif()
    #
    # Did that succeed?
    #
    if(NOT HAVE_DECL_ETHER_NTOHOST)
        #
        # No - how about <sys/ethernet.h>, as on Solaris 10 and later?
        #
        # This test fails if we don't have <sys/ethernet.h>
        # or if we do but it doesn't declare ether_ntohost().
        #
        check_symbol_exists(ether_ntohost sys/ethernet.h SYS_ETHERNET_H_DECLARES_ETHER_NTOHOST)
        if(SYS_ETHERNET_H_DECLARES_ETHER_NTOHOST)
            #
            # Yes - we have it declared.
            #
            set(HAVE_DECL_ETHER_NTOHOST TRUE)
        endif()
    endif()
    #
    # Did that succeed?
    #
    if(NOT HAVE_DECL_ETHER_NTOHOST)
        #
        # No, how about <arpa/inet.h>, as on AIX?
        #
        # This test fails if we don't have <arpa/inet.h>
        # or if we do but it doesn't declare ether_ntohost().
        #
        check_symbol_exists(ether_ntohost arpa/inet.h ARPA_INET_H_DECLARES_ETHER_NTOHOST)
        if(ARPA_INET_H_DECLARES_ETHER_NTOHOST)
            #
            # Yes - we have it declared.
            #
            set(HAVE_DECL_ETHER_NTOHOST TRUE)
        endif()
    endif()
    #
    # Did that succeed?
    #
    if(NOT HAVE_DECL_ETHER_NTOHOST)
        #
        # No, how about <netinet/if_ether.h>?
        # On some platforms, it requires <net/if.h> and
        # <netinet/in.h>, and we always include it with
        # both of them, so test it with both of them.
        #
        # This test fails if we don't have <netinet/if_ether.h>
        # and the headers we include before it, or if we do but
        # <netinet/if_ether.h> doesn't declare ether_ntohost().
        #
        check_symbol_exists(ether_ntohost "sys/types.h;sys/socket.h;net/if.h;netinet/in.h;netinet/if_ether.h" NETINET_IF_ETHER_H_DECLARES_ETHER_NTOHOST)
        if(NETINET_IF_ETHER_H_DECLARES_ETHER_NTOHOST)
            #
            # Yes - we have it declared.
            #
            set(HAVE_DECL_ETHER_NTOHOST TRUE)
        endif()
    endif()
    #
    # After all that, is ether_ntohost() declared?
    #
    if(NOT HAVE_DECL_ETHER_NTOHOST)
        #
        # No, we'll have to declare it ourselves.
        # Do we have "struct ether_addr" if we include<netinet/if_ether.h>?
        #
        check_struct_has_member("struct ether_addr" octet "sys/types.h;sys/socket.h;net/if.h;netinet/in.h;netinet/if_ether.h" HAVE_STRUCT_ETHER_ADDR)
    endif()
endif()
cmake_pop_check_state()

#
# Data types.
#
# XXX - there's no check_struct() macro that's like check_struct_has_member()
# except that it only checks for the existence of the structure type,
# so we use check_struct_has_member() and look for ss_family.
#

#
# Check for IPv6 support.
# We just check for AF_INET6 and struct in6_addr.
#
cmake_push_check_state()
if(WIN32)
    set(CMAKE_EXTRA_INCLUDE_FILES sys/types.h ws2tcpip.h)
    check_symbol_exists(AF_INET6 "sys/types.h;ws2tcpip.h" HAVE_AF_INET6)
else(WIN32)
    set(CMAKE_EXTRA_INCLUDE_FILES sys/types.h sys/socket.h netinet/in.h)
    check_symbol_exists(AF_INET6 "sys/types.h;sys/socket.h;netinet/in.h" HAVE_AF_INET6)
endif(WIN32)
check_type_size("struct in6_addr" HAVE_STRUCT_IN6_ADDR)
cmake_pop_check_state()
if(HAVE_AF_INET6 AND HAVE_STRUCT_IN6_ADDR)
    set(HAVE_OS_IPV6_SUPPORT TRUE)
endif(HAVE_AF_INET6 AND HAVE_STRUCT_IN6_ADDR)

######################################
# External dependencies
######################################

#
# libpcap/WinPcap/Npcap.
# First, find it.
#
find_package(PCAP REQUIRED)
include_directories(${PCAP_INCLUDE_DIRS})

cmake_push_check_state()

#
# Now check headers.
#
set(CMAKE_REQUIRED_INCLUDES ${PCAP_INCLUDE_DIRS})

#
# Check whether we have pcap/pcap-inttypes.h.
# If we do, we use that to get the C99 types defined.
#
check_include_file(pcap/pcap-inttypes.h HAVE_PCAP_PCAP_INTTYPES_H)

#
# At compile time HAVE_PCAP_FINDALLDEVS depends on HAVE_PCAP_IF_T.
#
cmake_push_check_state()
set(CMAKE_EXTRA_INCLUDE_FILES pcap.h)
check_type_size(pcap_if_t PCAP_IF_T)
cmake_pop_check_state()

#
# Check for various functions in libpcap/WinPcap/Npcap.
#
cmake_push_check_state()
set(CMAKE_REQUIRED_LIBRARIES ${PCAP_LIBRARIES})

#
# Check for "pcap_list_datalinks()" and use a substitute version if
# it's not present.  If it is present, check for "pcap_free_datalinks()";
# if it's not present, we don't replace it for now.  (We could do so
# on UN*X, but not on Windows, where hilarity ensues if a program
# built with one version of the MSVC support library tries to free
# something allocated by a library built with another version of
# the MSVC support library.)
#
check_function_exists(pcap_list_datalinks HAVE_PCAP_LIST_DATALINKS)
if(HAVE_PCAP_LIST_DATALINKS)
    check_function_exists(pcap_free_datalinks HAVE_PCAP_FREE_DATALINKS)
endif(HAVE_PCAP_LIST_DATALINKS)

#
# Check for "pcap_datalink_name_to_val()", and use a substitute
# version if it's not present.  If it is present, check for
# "pcap_datalink_val_to_description()", and if we don't have it,
# use a substitute version.
#
check_function_exists(pcap_datalink_name_to_val HAVE_PCAP_DATALINK_NAME_TO_VAL)
if(HAVE_PCAP_DATALINK_NAME_TO_VAL)
    check_function_exists(pcap_datalink_val_to_description HAVE_PCAP_DATALINK_VAL_TO_DESCRIPTION)
endif(HAVE_PCAP_DATALINK_NAME_TO_VAL)

#
# Check for "pcap_set_datalink()"; you can't substitute for it if
# it's absent (it has hooks into libpcap), so just define the
# HAVE_ value if it's there.
#
check_function_exists(pcap_set_datalink HAVE_PCAP_SET_DATALINK)

#
# Check for "pcap_breakloop()"; you can't substitute for it if
# it's absent (it has hooks into the live capture routines),
# so just define the HAVE_ value if it's there.
#
check_function_exists(pcap_breakloop HAVE_PCAP_BREAKLOOP)

#
# Check for "pcap_dump_ftell()"; we use a substitute version
# if it's not present.
#
check_function_exists(pcap_dump_ftell HAVE_PCAP_DUMP_FTELL)

#
# Do we have the new open API?  Check for pcap_create() and for
# pcap_statustostr(), and assume that, if we have both of them,
# we also have pcap_activate() and the other new routines
# introduced in libpcap 1.0.0.  (We check for pcap_statustostr()
# as well, because WinPcap 4.1.3 screwed up and exported pcap_create()
# but not other routines such as pcap_statustostr(), even though it
# defined them and even though you really want pcap_statustostr() to
# get strings corresponding to some of the status returns from the
# new routines.)
#
check_function_exists(pcap_statustostr HAVE_PCAP_STATUSTOSTR)
#
# If we don't have pcap_statustostr(), don't check for pcap_create(),
# so we pretend we don't have it.
#
if(HAVE_PCAP_STATUSTOSTR)
    check_function_exists(pcap_create HAVE_PCAP_CREATE)
endif(HAVE_PCAP_STATUSTOSTR)
if(HAVE_PCAP_CREATE)
    #
    # OK, do we have pcap_set_tstamp_type?  If so, assume we have
    # pcap_list_tstamp_types and pcap_free_tstamp_types as well.
    #
    check_function_exists(pcap_set_tstamp_type HAVE_PCAP_SET_TSTAMP_TYPE)

    #
    # And do we have pcap_set_tstamp_precision?  If so, we assume
    # we also have pcap_open_offline_with_tstamp_precision.
    #
    check_function_exists(pcap_set_tstamp_precision HAVE_PCAP_SET_TSTAMP_PRECISION)
endif(HAVE_PCAP_CREATE)

#
# Check for a miscellaneous collection of functions which we use
# if we have them.
#
check_function_exists(pcap_findalldevs HAVE_PCAP_FINDALLDEVS)
check_function_exists(pcap_dump_flush HAVE_PCAP_DUMP_FLUSH)
check_function_exists(pcap_lib_version HAVE_PCAP_LIB_VERSION)
if(NOT HAVE_PCAP_LIB_VERSION)
    # Check for the pcap_version string variable and set HAVE_PCAP_VERSION
endif(NOT HAVE_PCAP_LIB_VERSION)
check_function_exists(pcap_setdirection HAVE_PCAP_SETDIRECTION)
check_function_exists(pcap_set_immediate_mode HAVE_PCAP_SET_IMMEDIATE_MODE)
check_function_exists(pcap_dump_ftell64 HAVE_PCAP_DUMP_FTELL64)
#
# macOS Sonoma's libpcap includes stub versions of the remote-
# capture APIs.  They are exported as "weakly linked symbols".
#
# Xcode 15 offers only a macOS Sonoma SDK, which has a .tbd
# file for libpcap that claims it includes those APIs.  (Newer
# versions of macOS don't provide the system shared libraries,
# they only provide the dyld shared cache containing those
# libraries, so the OS provides SDKs that include a .tbd file
# to use when linking.)
#
# This means that check_function_exists() will think that
# the remote-capture APIs are present, including pcap_open()
# and pcap_findalldevs_ex().
#
# However, they are *not* present in macOS Ventura and earlier,
# which means that building on Ventura with Xcode 15 produces
# executables that fail to start because one of those APIs
# isn't found in the system libpcap.
#
# Protecting calls to those APIs with __builtin_available()
# does not prevent this, because the libpcap header files
# in the Sonoma SDK mark them as being first available
# in macOS 10.13, just like all the other routines introduced
# in libpcap 1.9, even though they're only available if libpcap
# is built with remote capture enabled or stub routines are
# provided.  (A fix to enable this has been checked into the
# libpcap repository, and may end up in a later version of
# the SDK.)
#
# Given all that, and given that the versions of the
# remote-capture APIs in Sonoma are stubs that always fail,
# there doesn't seem to be any point in checking for pcap_open()
# and pcap_findalldevs_ex() if we're linking against the Apple libpcap.
#
# However, if we're *not* linking against the Apple libpcap,
# we should check for it, so that we can use it if it's present.
#
# So we check for pcap_open() and pcap_findalldevs_ex() if 1) this isn't
# macOS or 2) the the libpcap we found is not a system library, meaning
# that its path begins neither with /usr/lib (meaning it's a system
# dylib) nor /Application/Xcode.app (meaning it's a file in
# the Xcode SDK).
#
if(NOT APPLE OR NOT
   (PCAP_LIBRARIES MATCHES "/usr/lib/.*" OR
    PCAP_LIBRARIES MATCHES "/Application/Xcode.app/.*"))
    check_function_exists(pcap_open HAVE_PCAP_OPEN)
    check_function_exists(pcap_findalldevs_ex HAVE_PCAP_FINDALLDEVS_EX)
endif()

#
# On Windows, check for pcap_wsockinit(); if we don't have it, check for
# wsockinit().
#
if(WIN32)
    check_function_exists(pcap_wsockinit HAVE_PCAP_WSOCKINIT)
    if(NOT HAVE_PCAP_WSOCKINIT)
        check_function_exists(wsockinit HAVE_WSOCKINIT)
    endif(NOT HAVE_PCAP_WSOCKINIT)
endif(WIN32)

#
# Check for special debugging functions
#
check_function_exists(pcap_set_parser_debug HAVE_PCAP_SET_PARSER_DEBUG)
if(NOT HAVE_PCAP_SET_PARSER_DEBUG)
    # Check whether libpcap defines pcap_debug or yydebug
    check_variable_exists(pcap_debug HAVE_PCAP_DEBUG)
    if(NOT HAVE_PCAP_DEBUG)
        check_variable_exists(yydebug HAVE_YYDEBUG)
    endif(NOT HAVE_PCAP_DEBUG)
endif(NOT HAVE_PCAP_SET_PARSER_DEBUG)

check_function_exists(pcap_set_optimizer_debug HAVE_PCAP_SET_OPTIMIZER_DEBUG)
check_function_exists(bpf_dump HAVE_BPF_DUMP)

cmake_pop_check_state()

#
# We have libpcap.
#
include_directories(SYSTEM ${PCAP_INCLUDE_DIRS})
set(TCPDUMP_LINK_LIBRARIES ${PCAP_LIBRARIES} ${TCPDUMP_LINK_LIBRARIES})

#
# Optional libraries.
#

#
# libsmi.
#
if(WITH_SMI)
    find_package(SMI)
    if(SMI_FOUND)
        include_directories(SYSTEM ${SMI_INCLUDE_DIRS})
        set(TCPDUMP_LINK_LIBRARIES ${TCPDUMP_LINK_LIBRARIES} ${SMI_LIBRARIES})
        set(USE_LIBSMI ON)
    endif(SMI_FOUND)
endif(WITH_SMI)

#
# OpenSSL/libressl libcrypto.
#
if(WITH_CRYPTO)
    find_package(CRYPTO)
    if(CRYPTO_FOUND)
        #
        # 1) do we have EVP_CIPHER_CTX_new?
        # If so, we use it to allocate an EVP_CIPHER_CTX, as
        # EVP_CIPHER_CTX may be opaque; otherwise, we allocate
        # it ourselves.
        #
        cmake_push_check_state()
        set(CMAKE_REQUIRED_LIBRARIES "${CRYPTO_LIBRARIES}")

        check_function_exists(EVP_CIPHER_CTX_new HAVE_EVP_CIPHER_CTX_NEW)

        #
        # 2) do we have EVP_DecryptInit_ex()?
        # If so, we use it, because we need to be able to make two
        # "initialize the cipher" calls, one with the cipher and key,
        # and one with the IV, and, as of OpenSSL 1.1, You Can't Do That
        # with EVP_DecryptInit(), because a call to EVP_DecryptInit() will
        # unconditionally clear the context, and if you don't supply a
        # cipher, it'll clear the cipher, rendering the context unusable
        # and causing a crash.
        #
        check_function_exists(EVP_DecryptInit_ex HAVE_EVP_DECRYPTINIT_EX)

        cmake_pop_check_state()

        #
        # We have libcrypto.
        #
        include_directories(SYSTEM ${CRYPTO_INCLUDE_DIRS})
        set(TCPDUMP_LINK_LIBRARIES ${TCPDUMP_LINK_LIBRARIES} ${CRYPTO_LIBRARIES})
        set(HAVE_LIBCRYPTO ON)
    endif(CRYPTO_FOUND)
endif(WITH_CRYPTO)

#
# Capsicum sandboxing.
# Some of this is in the system library, some of it is in other libraries.
#
if(WITH_CAPSICUM)
    check_include_files("sys/capsicum.h" HAVE_SYS_CAPSICUM_H)
    if(HAVE_SYS_CAPSICUM_H)
        check_function_exists(cap_enter HAVE_CAP_ENTER)
        check_function_exists(cap_rights_limit HAVE_CAP_RIGHTS_LIMIT)
        check_function_exists(cap_ioctls_limit HAVE_CAP_IOCTLS_LIMIT)
        check_function_exists(openat HAVE_OPENAT)
        if(HAVE_CAP_ENTER AND HAVE_CAP_RIGHTS_LIMIT AND
           HAVE_CAP_IOCTLS_LIMIT AND HAVE_OPENAT)
            #
            # OK, we have the functions we need to support Capsicum.
            #
            set(HAVE_CAPSICUM TRUE)

            #
            # OK, can we use Casper?
            #
            check_library_exists(casper cap_init "" HAVE_CAP_INIT)
            if(HAVE_CAP_INIT)
                cmake_push_check_state()
                set(CMAKE_REQUIRED_LIBRARIES casper)
                check_library_exists(cap_dns cap_gethostbyaddr "" HAVE_CAP_GETHOSTBYADDR)
                cmake_pop_check_state()
                if(HAVE_CAP_GETHOSTBYADDR)
                    set(HAVE_CASPER TRUE)
                    set(TCPDUMP_LINK_LIBRARIES ${TCPDUMP_LINK_LIBRARIES} casper cap_dns)
                endif(HAVE_CAP_GETHOSTBYADDR)
            endif(HAVE_CAP_INIT)
        endif(HAVE_CAP_ENTER AND HAVE_CAP_RIGHTS_LIMIT AND
              HAVE_CAP_IOCTLS_LIMIT AND HAVE_OPENAT)
    endif(HAVE_SYS_CAPSICUM_H)
endif(WITH_CAPSICUM)

#
# libcap-ng.
#
if(WITH_CAP_NG)
    check_include_file(cap-ng.h HAVE_CAP_NG_H)
    check_library_exists(cap-ng capng_change_id "" HAVE_LIBCAP_NG)
    if(HAVE_LIBCAP_NG)
        set(TCPDUMP_LINK_LIBRARIES ${TCPDUMP_LINK_LIBRARIES} cap-ng)
    endif(HAVE_LIBCAP_NG)
endif(WITH_CAP_NG)

###################################################################
#   Warning options
###################################################################

#
# Check and add warning options if we have a .devel file.
#
if(EXISTS ${CMAKE_SOURCE_DIR}/.devel OR EXISTS ${CMAKE_BINARY_DIR}/.devel)
    #
    # Warning options.
    #
    if(MSVC AND NOT ${CMAKE_C_COMPILER} MATCHES "clang*")
        #
        # MSVC, with Microsoft's front end and code generator.
        # "MSVC" is also set for Microsoft's compiler with a Clang
        # front end and their code generator ("Clang/C2"), so we
        # check for clang.exe and treat that differently.
        #
        check_and_add_compiler_option(-Wall)
        #
        # Disable some pointless warnings that /Wall turns on.
        #
        # Unfortunately, MSVC does not appear to have an equivalent
        # to "__attribute__((unused))" to mark a particular function
        # parameter as being known to be unused, so that the compiler
        # won't warn about it (for example, the function might have
        # that parameter because a pointer to it is being used, and
        # the signature of that function includes that parameter).
        # C++ lets you give a parameter a type but no name, but C
        # doesn't have that.
        #
        check_and_add_compiler_option(-wd4100)
        #
        # In theory, we care whether somebody uses f() rather than
        # f(void) to declare a function with no arguments, but, in
        # practice, there are places in the Windows header files
        # that appear to do that, so we squelch that warning.
        #
        check_and_add_compiler_option(-wd4255)
        #
        # Windows FD_SET() generates this, so we suppress it.
        #
        check_and_add_compiler_option(-wd4548)
        #
        # Perhaps testing something #defined to be 0 with #ifdef is an
        # error, and it should be tested with #if, but perhaps it's
        # not, and Microsoft does that in its headers, so we squelch
        # that warning.
        #
        check_and_add_compiler_option(-wd4574)
        #
        # The Windows headers also test not-defined values in #if, so
        # we don't want warnings about that, either.
        #
        check_and_add_compiler_option(-wd4668)
        #
        # We do *not* care whether some function is, or isn't, going to be
        # expanded inline.
        #
        check_and_add_compiler_option(-wd4710)
        check_and_add_compiler_option(-wd4711)
        #
        # We do *not* care whether we're adding padding bytes after
        # structure members.
        #
        check_and_add_compiler_option(-wd4820)
        #
        # We do *not* care about every single place the compiler would
        # have inserted Spectre mitigation if only we had told it to
        # do so with /Qspectre.  I guess the theory is that it's seeing
        # bounds checks that would prevent out-of-bounds loads and that
        # those out-of-bounds loads could be done speculatively and that
        # the Spectre attack could detect the value of the out-of-bounds
        # data *if* it's within our address space, but unless I'm
        # missing something I don't see that as being any form of
        # security hole.
        #
        # XXX - add /Qspectre if that is really worth doing.
        #
        check_and_add_compiler_option(-wd5045)
        #
        # We do *not* care whether a structure had padding added at
        # the end because of __declspec(align) - *we* don't use
        # __declspec(align), because the only structures whose layout
        # we precisely specify are those that get overlaid on packet
        # data, and in those every element is an array of octets so
        # that we have full control over the size and alignment, and,
        # apparently, jmp_buf has such a declaration on x86, meaning
        # that everything that includes netdissect.h, i.e. almost every
        # file in tcpdump, gets a warning.
        #
        check_and_add_compiler_option(-wd4324)
    else()
        #
        # Other compilers, including MSVC with a Clang front end and
        # Microsoft's code generator.  We currently treat them as if
        # they might support GCC-style -W options.
        #
        check_and_add_compiler_option(-W)
        check_and_add_compiler_option(-Wall)
        check_and_add_compiler_option(-Wassign-enum)
        check_and_add_compiler_option(-Wcast-qual)
        check_and_add_compiler_option(-Wmissing-prototypes)
        check_and_add_compiler_option(-Wmissing-variable-declarations)
        check_and_add_compiler_option(-Wold-style-definition)
        if(NOT CMAKE_C_COMPILER_ID MATCHES "Sun")
            # In Sun C versions that implement GCC compatibility "-Wpedantic"
            # means the same as "-pedantic".  The latter is mutually exclusive
            # with several other options.  One of those is "-xc99", which has
            # already been set for Sun C above.
            check_and_add_compiler_option(-Wpedantic)
        endif()
        check_and_add_compiler_option(-Wpointer-arith)
        check_and_add_compiler_option(-Wpointer-sign)
        check_and_add_compiler_option(-Wshadow)
        check_and_add_compiler_option(-Wsign-compare)
        check_and_add_compiler_option(-Wstrict-prototypes)
        check_and_add_compiler_option(-Wundef)
        check_and_add_compiler_option(-Wunreachable-code-return)
        check_and_add_compiler_option(-Wused-but-marked-unused)
        check_and_add_compiler_option(-Wwrite-strings)
    endif()
endif()

#
# Extra compiler options for the build matrix scripts to request -Werror or
# its equivalent if required.  The CMake variable name cannot be CFLAGS
# because that is already used for a different purpose in CMake.  Example
# usage: cmake -DEXTRA_CFLAGS='-Wall -Wextra -Werror' ...
#
if(NOT "${EXTRA_CFLAGS}" STREQUAL "")
    # The meaning of EXTRA_CFLAGS is "use the exact specified options, or the
    # build risks failing to fail", not "try every specified option, omit those
    # that do not work and use the rest".  Thus use add_compile_options(), not
    # foreach()/check_and_add_compiler_option().  Another reason to do that is
    # that the effect lasts in testprogs/ and testprogs/fuzz/.
    string(REPLACE " " ";" _extra_cflags_list ${EXTRA_CFLAGS})
    add_compile_options(${_extra_cflags_list})
    message(STATUS "Added extra compile options (${EXTRA_CFLAGS})")
endif()

######################################
# Input files
######################################

if(ENABLE_SMB)
    #
    # We allow the SMB dissector to be omitted.
    #
    set(LOCALSRC ${LOCALSRC}
        print-smb.c
        smbutil.c)
endif(ENABLE_SMB)

set(NETDISSECT_SOURCE_LIST_C
    addrtoname.c
    addrtostr.c
    af.c
    ascii_strcasecmp.c
    checksum.c
    cpack.c
    gmpls.c
    in_cksum.c
    ipproto.c
    l2vpn.c
    machdep.c
    netdissect.c
    netdissect-alloc.c
    nlpid.c
    oui.c
    ntp.c
    parsenfsfh.c
    print.c
    print-802_11.c
    print-802_15_4.c
    print-ah.c
    print-ahcp.c
    print-aodv.c
    print-aoe.c
    print-ap1394.c
    print-arcnet.c
    print-arista.c
    print-arp.c
    print-ascii.c
    print-atalk.c
    print-atm.c
    print-babel.c
    print-bcm-li.c
    print-beep.c
    print-bfd.c
    print-bgp.c
    print-bootp.c
    print-brcmtag.c
    print-bt.c
    print-calm-fast.c
    print-carp.c
    print-cdp.c
    print-cfm.c
    print-chdlc.c
    print-cip.c
    print-cnfp.c
    print-dccp.c
    print-decnet.c
    print-dhcp6.c
    print-domain.c
    print-dsa.c
    print-dtp.c
    print-dvmrp.c
    print-eap.c
    print-egp.c
    print-eigrp.c
    print-enc.c
    print-esp.c
    print-ether.c
    print-fddi.c
    print-forces.c
    print-fr.c
    print-frag6.c
    print-ftp.c
    print-geneve.c
    print-geonet.c
    print-gre.c
    print-hncp.c
    print-hsrp.c
    print-http.c
    print-icmp.c
    print-icmp6.c
    print-igmp.c
    print-igrp.c
    print-ip-demux.c
    print-ip.c
    print-ip6.c
    print-ip6opts.c
    print-ipcomp.c
    print-ipfc.c
    print-ipnet.c
    print-ipoib.c
    print-ipx.c
    print-isakmp.c
    print-isoclns.c
    print-juniper.c
    print-krb.c
    print-l2tp.c
    print-lane.c
    print-ldp.c
    print-lisp.c
    print-llc.c
    print-lldp.c
    print-lmp.c
    print-loopback.c
    print-lspping.c
    print-lwapp.c
    print-lwres.c
    print-m3ua.c
    print-macsec.c
    print-mobile.c
    print-mobility.c
    print-mpcp.c
    print-mpls.c
    print-mptcp.c
    print-msdp.c
    print-msnlb.c
    print-nflog.c
    print-nfs.c
    print-nsh.c
    print-ntp.c
    print-null.c
    print-olsr.c
    print-openflow-1.0.c
    print-openflow-1.3.c
    print-openflow.c
    print-ospf.c
    print-ospf6.c
    print-otv.c
    print-pflog.c
    print-pgm.c
    print-pim.c
    print-pktap.c
    print-ppi.c
    print-ppp.c
    print-pppoe.c
    print-pptp.c
    print-ptp.c
    print-radius.c
    print-raw.c
    print-realtek.c
    print-resp.c
    print-rip.c
    print-ripng.c
    print-rpki-rtr.c
    print-rsvp.c
    print-rt6.c
    print-rtsp.c
    print-rx.c
    print-sctp.c
    print-sflow.c
    print-sip.c
    print-sl.c
    print-sll.c
    print-slow.c
    print-smtp.c
    print-snmp.c
    print-someip.c
    print-ssh.c
    print-stp.c
    print-sunatm.c
    print-sunrpc.c
    print-symantec.c
    print-syslog.c
    print-tcp.c
    print-telnet.c
    print-tftp.c
    print-timed.c
    print-tipc.c
    print-token.c
    print-udld.c
    print-udp.c
    print-unsupported.c
    print-usb.c
    print-vjc.c
    print-vqp.c
    print-vrrp.c
    print-vsock.c
    print-vtp.c
    print-vxlan-gpe.c
    print-vxlan.c
    print-wb.c
    print-whois.c
    print-zep.c
    print-zephyr.c
    print-zeromq.c
    ${LOCALSRC}
    signature.c
    strtoaddr.c
    util-print.c
)

#
# Replace missing functions
#
foreach(FUNC strlcat strlcpy strdup strsep getservent getopt_long)
    string(TOUPPER ${FUNC} FUNC_UPPERCASE)
    set(HAVE_FUNC_UPPERCASE HAVE_${FUNC_UPPERCASE})
    if(NOT ${HAVE_FUNC_UPPERCASE})
        set(NETDISSECT_SOURCE_LIST_C ${NETDISSECT_SOURCE_LIST_C} missing/${FUNC}.c)
    endif()
endforeach()

add_library(netdissect STATIC
    ${NETDISSECT_SOURCE_LIST_C}
)
if(NOT C_ADDITIONAL_FLAGS STREQUAL "")
    set_target_properties(netdissect PROPERTIES COMPILE_FLAGS ${C_ADDITIONAL_FLAGS})
endif()

set(TCPDUMP_SOURCE_LIST_C fptype.c tcpdump.c)

if(NOT HAVE_BPF_DUMP)
    set(TCPDUMP_SOURCE_LIST_C ${TCPDUMP_SOURCE_LIST_C} bpf_dump.c)
endif(NOT HAVE_BPF_DUMP)
if(NOT HAVE_PCAP_DUMP_FTELL)
    set(TCPDUMP_SOURCE_LIST_C ${TCPDUMP_SOURCE_LIST_C} missing/pcap_dump_ftell.c)
endif(NOT HAVE_PCAP_DUMP_FTELL)

if(NOT HAVE_PCAP_LIST_DATALINKS)
    set(TCPDUMP_SOURCE_LIST_C ${TCPDUMP_SOURCE_LIST_C} missing/datalinks.c)
endif(NOT HAVE_PCAP_LIST_DATALINKS)

if((NOT HAVE_PCAP_DATALINK_NAME_TO_VAL) OR (NOT HAVE_PCAP_DATALINK_VAL_TO_DESCRIPTION))
    set(TCPDUMP_SOURCE_LIST_C ${TCPDUMP_SOURCE_LIST_C} missing/dlnames.c)
endif((NOT HAVE_PCAP_DATALINK_NAME_TO_VAL) OR (NOT HAVE_PCAP_DATALINK_VAL_TO_DESCRIPTION))

set(PROJECT_SOURCE_LIST_C ${NETDISSECT_SOURCE_LIST_C} ${TCPDUMP_SOURCE_LIST_C})

file(GLOB PROJECT_SOURCE_LIST_H
    *.h
)

#
# Assume, by default, no support for shared libraries and V7/BSD
# convention for man pages (devices in section 4, file formats in
# section 5, miscellaneous info in section 7, administrative commands
# and daemons in section 8).  Individual cases can override this.
# Individual cases can override this.
#
set(MAN_FILE_FORMATS 5)
set(MAN_MISC_INFO 7)
if(CMAKE_SYSTEM_NAME STREQUAL "AIX")
    # Workaround to enable certain features
    set(_SUN TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "HP-UX")
    #
    # Use System V conventions for man pages.
    #
    set(MAN_FILE_FORMATS 4)
    set(MAN_MISC_INFO 5)
elseif(CMAKE_SYSTEM_NAME STREQUAL "IRIX" OR CMAKE_SYSTEM_NAME STREQUAL "IRIX64")
    #
    # Use IRIX conventions for man pages; they're the same as the
    # System V conventions, except that they use section 8 for
    # administrative commands and daemons.
    #
    set(MAN_FILE_FORMATS 4)
    set(MAN_MISC_INFO 5)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OSF1")
    #
    # DEC OSF/1, a/k/a Digital UNIX, a/k/a Tru64 UNIX.
    # Use Tru64 UNIX conventions for man pages; they're the same as the
    # System V conventions except that they use section 8 for
    # administrative commands and daemons.
    #
    set(MAN_FILE_FORMATS 4)
    set(MAN_MISC_INFO 5)
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS" AND CMAKE_SYSTEM_VERSION MATCHES "5[.][0-9.]*")
    #
    # SunOS 5.x.
    #
    if(CMAKE_SYSTEM_VERSION STREQUAL "5.12")
    else()
        #
        # Use System V conventions for man pages.
        #
        set(MAN_FILE_FORMATS 4)
        set(MAN_MISC_INFO 5)
    endif()
endif()

source_group("Source Files" FILES ${PROJECT_SOURCE_LIST_C})
source_group("Header Files" FILES ${PROJECT_SOURCE_LIST_H})

######################################
# Register targets
######################################

add_executable(tcpdump ${TCPDUMP_SOURCE_LIST_C})
if(NOT C_ADDITIONAL_FLAGS STREQUAL "")
    set_target_properties(tcpdump PROPERTIES COMPILE_FLAGS ${C_ADDITIONAL_FLAGS})
endif()
target_link_libraries(tcpdump netdissect ${TCPDUMP_LINK_LIBRARIES})

######################################
# Write out the config.h file
######################################

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmakeconfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

######################################
# Install tcpdump and man pages
######################################

#
# "Define GNU standard installation directories", which actually
# are also defined, to some degree, by autotools, and at least
# some of which are general UN*X conventions.
#
include(GNUInstallDirs)

set(MAN1_EXPAND tcpdump.1.in)

if(WIN32)
    # XXX TODO where to install on Windows?
else(WIN32)
    install(TARGETS tcpdump DESTINATION bin)
endif(WIN32)

# On UN*X, and on Windows when not using MSVC, process man pages and
# arrange that they be installed.
if(NOT MSVC)
    #
    # Man pages.
    #
    # For each section of the manual for which we have man pages
    # that require macro expansion, do the expansion.
    #
    set(MAN1 "")
    foreach(TEMPLATE_MANPAGE ${MAN1_EXPAND})
        string(REPLACE ".in" "" MANPAGE ${TEMPLATE_MANPAGE})
        configure_file(${CMAKE_SOURCE_DIR}/${TEMPLATE_MANPAGE} ${CMAKE_CURRENT_BINARY_DIR}/${MANPAGE} @ONLY)
        set(MAN1 ${MAN1} ${CMAKE_CURRENT_BINARY_DIR}/${MANPAGE})
    endforeach(TEMPLATE_MANPAGE)
    install(FILES ${MAN1} DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif(NOT MSVC)

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

#
# Tcpdump tests
# We try to find the Perl interpreter and, if we do, we have the check
# rule run tests/TESTrun with it, because just trying to run the TESTrun
# script as a command won't work on Windows.
#
find_program(PERL perl)
if(PERL)
    message(STATUS "Found perl at ${PERL}")
    add_custom_target(check
        COMMAND ${PERL} ${CMAKE_SOURCE_DIR}/tests/TESTrun)
else()
    message(STATUS "Didn't find perl")
endif()
