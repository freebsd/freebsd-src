Experimental cmake-based build support for APR-Util on Microsoft Windows

Status
------

This build support is currently intended only for Microsoft Windows.
Only Windows NT-based systems can be targeted.  (The traditional 
Windows build support for APR can target Windows 9x as well.)

This build support is experimental.  Specifically,

* It does not support all features of APR-Util.
* Some components may not be built correctly and/or in a manner
  compatible with the previous Windows build support.
* Build interfaces, such as the mechanisms which are used to enable
  optional functionality or specify prerequisites, may change from
  release to release as feedback is received from users and bugs and
  limitations are resolved.

Important: Refer to the "Known Bugs and Limitations" section for further
           information.

           It is beyond the scope of this document to document or explain
           how to utilize the various cmake features, such as different
           build backends or provisions for finding support libraries.

           Please refer to the cmake documentation for additional information
           that applies to building any project with cmake.

Prerequisites
-------------

The following tools must be in PATH:

* cmake, version 2.8 or later
  cmake version 3.1.3 or later is required to work with current OpenSSL
  releases.  (OpenSSL is an optional prerequisite of APR-Util.)
* If using a command-line compiler: compiler and linker and related tools
  (Refer to the cmake documentation for more information.)

The following support libraries are mandatory:

* APR 1.4.x or APR 1.5.x, built with cmake

Optional support libraries allow optional features of APR to be enabled:

* OpenSSL
* many others potentially, though the build support isn't currently
  implemented

How to build
------------

1. cd to a clean directory for building (i.e., don't build in your
   source tree)

2. Some cmake backends may want your compile tools in PATH.  (Hint: "Visual
   Studio Command Prompt")

3. set CMAKE_LIBRARY_PATH=d:\path\to\prereq1\lib;d:\path\to\prereq2\lib;...

4. set CMAKE_INCLUDE_PATH=d:\path\to\prereq1\include;d:\path\to\prereq2\include;...

5. cmake -G "some backend, like 'NMake Makefiles'"
     -DCMAKE_INSTALL_PREFIX=d:/path/to/aprinst
     -DAPR-Util-specific-flags
     d:/path/to/aprutilsource

   If APR 1.x was installed to a different directory than APR-Util,
   also pass these additional arguments:

     -DAPR_INCLUDE_DIR=d:/path/to/apr1inst/include
     -DAPR_LIBRARIES=d:/path/to/apr1inst/lib/libapr-1.lib

   Alternately, use cmake-gui and update settings in the GUI.

   APR-Util feature flags:

       APU_HAVE_CRYPTO        Build crypt support (only the OpenSSL
                              implementation is currently supported)
                              Default: OFF
       APU_HAVE_ODBC          Build ODBC DBD driver
                              Default: ON
       APR_BUILD_TESTAPR      Build APR-Util test suite
                              Default: OFF
       TEST_STATIC_LIBS       Build the test suite to test the APR static
                              library instead of the APR dynamic library.
                              Default: OFF
                              In order to build the test suite against both
                              static and dynamic libraries, separate builds
                              will be required, one with TEST_STATIC_LIBS
                              set to ON.
       INSTALL_PDB            Install .pdb files if generated.
                              Default: ON

   CMAKE_C_FLAGS_RELEASE, _DEBUG, _RELWITHDEBINFO, _MINSIZEREL

   CMAKE_BUILD_TYPE

       For NMake Makefiles the choices are at least DEBUG, RELEASE,
       RELWITHDEBINFO, and MINSIZEREL
       Other backends make have other selections.

6. build using chosen backend (e.g., "nmake install")

Known Bugs and Limitations
--------------------------

* If include/apu.h or other generated files have been created in the source
  directory by another build system, they will be used unexpectedly and
  cause the build to fail.
* Options should be provided for remaining features, along with finding any
  necessary libraries
  + DBM:
    . APU_HAVE_GDBM
    . APU_HAVE_NDBM
    . APU_HAVE_DB
  + DBD:
    . APU_HAVE_PGSQL
    . APU_HAVE_MYSQL
    . APU_HAVE_SQLITE3
    . APU_HAVE_SQLITE2
    . APU_HAVE_ORACLE
  + CRYPTO:
    . APU_HAVE_NSS
  + XLATE, APU_HAVE_ICONV (no way to consume an apr-iconv build yet)
* Static builds of APR modules are not supported.
* CHANGES/LICENSE/NOTICE is not installed, unlike Makefile.win.
  (But unlike Makefile.win we want to call them APR-Util-CHANGES.txt
  and so on.)  But perhaps that is a job for a higher-level script.

Generally:

* Many APR-Util features have not been tested with this build.
* Developers need to examine the existing Windows build in great detail and see
  what is missing from the cmake-based build, whether a feature or some build
  nuance.
* Any feedback you can provide on your experiences with this build will be
  helpful.
