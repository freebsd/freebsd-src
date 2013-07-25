#**** serf Win32 -*- Makefile -*- ********************************************
#
# Define DEBUG_BUILD to create a debug version of the library.

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE
NULL=nul
!ENDIF

CFLAGS = /Zi /W3 /EHsc /I "./"

!IF "$(DEBUG_BUILD)" == ""
INTDIR = Release
CFLAGS = /MD /O2 /D "NDEBUG" $(CFLAGS)
STATIC_LIB = $(INTDIR)\serf-1.lib
!ELSE
INTDIR = Debug
CFLAGS = /MDd /Od /W3 /Gm /D "_DEBUG" $(CFLAGS)
STATIC_LIB = $(INTDIR)\serf-1.lib
!ENDIF

########
# Support for OpenSSL integration
!IF "$(OPENSSL_SRC)" == ""
!ERROR OpenSSL is required. Please define OPENSSL_SRC.
!ELSE
OPENSSL_FLAGS = /I "$(OPENSSL_SRC)\inc32"
!ENDIF

!IF "$(HTTPD_SRC)" != ""
!IF "$(APR_SRC)" == ""
APR_SRC=$(HTTPD_SRC)\srclib\apr
!ENDIF

!IF "$(APRUTIL_SRC)" == ""
APRUTIL_SRC=$(HTTPD_SRC)\srclib\apr-util
!ENDIF

!ENDIF

########
# APR
!IF "$(APR_SRC)" == ""
!ERROR APR is required. Please define APR_SRC or HTTPD_SRC.
!ENDIF

APR_FLAGS = /I "$(APR_SRC)\include"
!IF [IF EXIST "$(APR_SRC)\$(INTDIR)\libapr-1.lib" exit 1] == 1
APR_LIBS = "$(APR_SRC)\$(INTDIR)\libapr-1.lib"
!ELSE
APR_LIBS = "$(APR_SRC)\$(INTDIR)\libapr.lib"
!ENDIF

########
# APR Util
!IF "$(APRUTIL_SRC)" == ""
!ERROR APR-Util is required. Please define APRUTIL_SRC or HTTPD_SRC.
!ENDIF

APRUTIL_FLAGS = /I "$(APRUTIL_SRC)\include"
!IF [IF EXIST "$(APRUTIL_SRC)\$(INTDIR)\libaprutil-1.lib" exit 1] == 1
APRUTIL_LIBS = "$(APRUTIL_SRC)\$(INTDIR)\libaprutil-1.lib"
!ELSE
APRUTIL_LIBS = "$(APRUTIL_SRC)\$(INTDIR)\libaprutil.lib"
!ENDIF

########
# Support for zlib integration
!IF "$(ZLIB_SRC)" == ""
!ERROR ZLib is required. Please define ZLIB_SRC.
!ELSE
ZLIB_FLAGS = /I "$(ZLIB_SRC)"
!IF "$(ZLIB_DLL)" == ""
!IF "$(ZLIB_LIBDIR)" == ""
!IF "$(DEBUG_BUILD)" == ""
ZLIB_LIBS = "$(ZLIB_SRC)\zlibstat.lib"
!ELSE
ZLIB_LIBS = "$(ZLIB_SRC)\zlibstatD.lib"
!ENDIF
!ELSE
ZLIB_LIBS = "$(ZLIB_LIBDIR)\x86\ZlibStat$(INTDIR)\zlibstat.lib"
ZLIB_FLAGS = $(ZLIB_FLAGS) /D ZLIB_WINAPI
!ENDIF
!ELSE
ZLIB_FLAGS = $(ZLIB_FLAGS) /D ZLIB_DLL
ZLIB_LIBS = "$(ZLIB_SRC)\zlibdll.lib"
!ENDIF
!ENDIF


# Exclude stuff we don't need from the Win32 headers
WIN32_DEFS = /D WIN32 /D WIN32_LEAN_AND_MEAN /D NOUSER /D NOGDI /D NONLS /D NOCRYPT /D SERF_HAVE_SSPI

CPP=cl.exe
CPP_PROJ = /c /nologo $(CFLAGS) $(WIN32_DEFS) $(APR_FLAGS) $(APRUTIL_FLAGS) $(OPENSSL_FLAGS) $(ZLIB_FLAGS) /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\"
LIB32=link.exe
LIB32_FLAGS=/nologo

LIB32_OBJS= \
    "$(INTDIR)\aggregate_buckets.obj" \
    "$(INTDIR)\auth.obj" \
    "$(INTDIR)\auth_basic.obj" \
    "$(INTDIR)\auth_digest.obj" \
    "$(INTDIR)\auth_kerb.obj" \
    "$(INTDIR)\auth_kerb_gss.obj" \
    "$(INTDIR)\auth_kerb_sspi.obj" \
    "$(INTDIR)\context.obj" \
    "$(INTDIR)\ssltunnel.obj" \
    "$(INTDIR)\allocator.obj" \
    "$(INTDIR)\barrier_buckets.obj" \
    "$(INTDIR)\buckets.obj" \
    "$(INTDIR)\chunk_buckets.obj" \
    "$(INTDIR)\dechunk_buckets.obj" \
    "$(INTDIR)\deflate_buckets.obj" \
    "$(INTDIR)\file_buckets.obj" \
    "$(INTDIR)\headers_buckets.obj" \
    "$(INTDIR)\incoming.obj" \
    "$(INTDIR)\iovec_buckets.obj" \
    "$(INTDIR)\limit_buckets.obj" \
    "$(INTDIR)\mmap_buckets.obj" \
    "$(INTDIR)\outgoing.obj" \
    "$(INTDIR)\request_buckets.obj" \
    "$(INTDIR)\response_buckets.obj" \
    "$(INTDIR)\response_body_buckets.obj" \
    "$(INTDIR)\simple_buckets.obj" \
    "$(INTDIR)\socket_buckets.obj" \
    "$(INTDIR)\ssl_buckets.obj" \

!IFDEF OPENSSL_STATIC
LIB32_OBJS = $(LIB32_OBJS) "$(OPENSSL_SRC)\out32\libeay32.lib" \
               "$(OPENSSL_SRC)\out32\ssleay32.lib"
!ELSE
LIB32_OBJS = $(LIB32_OBJS) "$(OPENSSL_SRC)\out32dll\libeay32.lib" \
               "$(OPENSSL_SRC)\out32dll\ssleay32.lib"
!ENDIF

LIB32_OBJS = $(LIB32_OBJS) $(APR_LIBS) $(APRUTIL_LIBS) $(ZLIB_LIBS) 

SYS_LIBS = secur32.lib

TEST_OBJS = \
    "$(INTDIR)\CuTest.obj" \
    "$(INTDIR)\test_all.obj" \
    "$(INTDIR)\test_util.obj" \
    "$(INTDIR)\test_context.obj" \
    "$(INTDIR)\test_buckets.obj" \
    "$(INTDIR)\test_ssl.obj" \
    "$(INTDIR)\test_server.obj" \
    "$(INTDIR)\test_sslserver.obj" \

TEST_LIBS = user32.lib advapi32.lib gdi32.lib ws2_32.lib


ALL: $(INTDIR) $(STATIC_LIB) TESTS

CLEAN:
  -@erase /q "$(INTDIR)" >nul

$(INTDIR):
  -@if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

TESTS: $(STATIC_LIB) $(INTDIR)\serf_response.exe $(INTDIR)\serf_get.exe \
       $(INTDIR)\serf_request.exe $(INTDIR)\test_all.exe

CHECK: $(INTDIR) TESTS
  $(INTDIR)\serf_response.exe test\testcases\simple.response
  $(INTDIR)\serf_response.exe test\testcases\chunked-empty.response
  $(INTDIR)\serf_response.exe test\testcases\chunked.response
  $(INTDIR)\serf_response.exe test\testcases\chunked-trailers.response
  $(INTDIR)\serf_response.exe test\testcases\deflate.response
  $(INTDIR)\test_all.exe
  
"$(STATIC_LIB)": $(INTDIR) $(LIB32_OBJS)
  $(LIB32) -lib @<<
    $(LIB32_FLAGS) $(LIB32_OBJS) $(SYS_LIBS) /OUT:$@
<<


.c{$(INTDIR)}.obj:
  $(CPP) @<<
    $(CPP_PROJ) $<
<<

{auth}.c{$(INTDIR)}.obj:
  $(CPP) @<<
    $(CPP_PROJ) $<
<<

{buckets}.c{$(INTDIR)}.obj:
  $(CPP) @<<
    $(CPP_PROJ) $<
<<

{test}.c{$(INTDIR)}.obj: 
  $(CPP) @<<
    $(CPP_PROJ) $<
<<

{test\server}.c{$(INTDIR)}.obj: 
  $(CPP) @<<
    $(CPP_PROJ) $<
<<

$(INTDIR)\serf_response.exe: $(INTDIR)\serf_response.obj $(STATIC_LIB)
  $(LIB32) /DEBUG /OUT:$@ $** $(LIB32_FLAGS) $(TEST_LIBS)

$(INTDIR)\serf_get.exe: $(INTDIR)\serf_get.obj $(STATIC_LIB)
  $(LIB32) /DEBUG /OUT:$@ $** $(LIB32_FLAGS) $(TEST_LIBS)

$(INTDIR)\serf_request.exe: $(INTDIR)\serf_request.obj $(STATIC_LIB)
  $(LIB32) /DEBUG /OUT:$@ $** $(LIB32_FLAGS) $(TEST_LIBS)

$(INTDIR)\test_all.exe: $(TEST_OBJS) $(STATIC_LIB)
  $(LIB32) /DEBUG /OUT:$@ $** $(LIB32_FLAGS) $(TEST_LIBS)
