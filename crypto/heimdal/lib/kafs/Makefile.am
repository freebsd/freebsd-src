# $Id: Makefile.am,v 1.23 2000/12/11 00:44:50 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4) $(AFS_EXTRA_DEFS)

if KRB4
AFSLIBS = libkafs.la

if AIX
AFSL_EXP = $(srcdir)/afsl.exp

if AIX4
AFS_EXTRA_LD = -bnoentry
else
AFS_EXTRA_LD = -e _nostart
endif

if AIX_DYNAMIC_AFS
if HAVE_DLOPEN
AIX_SRC = 
else
AIX_SRC = dlfcn.c
endif
AFS_EXTRA_LIBS = afslib.so
AFS_EXTRA_DEFS =
else
AIX_SRC = afslib.c
AFS_EXTRA_LIBS = 
AFS_EXTRA_DEFS = -DSTATIC_AFS
endif

else
AFSL_EXP =
AIX_SRC =
endif # AIX

else
AFSLIBS = 
endif # KRB4


lib_LTLIBRARIES = $(AFSLIBS)
libkafs_la_LDFLAGS = -version-info 2:3:2
foodir = $(libdir)
foo_DATA = $(AFS_EXTRA_LIBS)
# EXTRA_DATA = afslib.so

CLEANFILES= $(AFS_EXTRA_LIBS)

include_HEADERS = kafs.h

if KRB5
afskrb5_c = afskrb5.c
endif

libkafs_la_SOURCES = afssys.c afskrb.c $(afskrb5_c) common.c $(AIX_SRC) kafs_locl.h afssysdefs.h
#afslib_so_SOURCES = afslib.c

EXTRA_libkafs_la_SOURCES = afskrb5.c dlfcn.c afslib.c dlfcn.h

EXTRA_DIST = README.dlfcn afsl.exp afslib.exp

man_MANS = kafs.3

# AIX: this almost works with gcc, but somehow it fails to use the
# correct ld, use ld instead
afslib.so: afslib.o
	ld -o $@ -bM:SRE -bI:$(srcdir)/afsl.exp -bE:$(srcdir)/afslib.exp $(AFS_EXTRA_LD) afslib.o -lc

$(OBJECTS): ../../include/config.h
