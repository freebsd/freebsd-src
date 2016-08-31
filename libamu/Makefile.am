## Process this file with automake to produce Makefile.in

# Package: am-utils
# Level: Makefile for libamu/ directory
# Author: Erez Zadok

#noinst_LIBRARIES = libamu.a
lib_LTLIBRARIES = libamu.la

# headers this depends on, not to be installed
noinst_HEADERS = amu.h

# these sources must be included:
libamu_la_SOURCES =	\
	misc_rpc.c	\
	mount_fs.c	\
	mtab.c		\
	nfs_prot_xdr.c	\
	strutil.c	\
	wire.c		\
	xdr_func.c	\
	xutil.c

## no need to list other optional sources because automake will add them
## automatically.
## XXX: must add the files after memcmp.c until automake 1.4+ is out
EXTRA_DIST = memcmp.c	\
	clnt_sperrno.c	\
	hasmntopt.c	\
	strcasecmp.c	\
	strerror.c	\
	strlcat.c	\
	strlcpy.c	\
	strstr.c	\
	ualarm.c

DISTCLEANFILES = mountutil.c mtabutil.c transputil.c umount_fs.c

# these may be added automatically by automake if needed:
#	alloca.c
#	clnt_sperrno.c
#	hasmntopt.c
#	memcmp.c (via AC_FUNC_MEMCMP)
#	strcasecmp.c
#	strerror.c
#	strstr.c
#	ualarm.c
#
# files optionally compiled:
#	mountutil.c: mount utilities
#	umount_fs.c: un-mount utilities
#
# files that are always compiled, but to a different path:
#	mtabutil.c: mount table utilities
#	transputil.c: transport (Sockets or TLI) utilities
BUILT_SOURCES =		\
	mountutil.c	\
	umount_fs.c	\
	mtabutil.c	\
	transputil.c

## XXX: Use the next line when automake newer than 1.4
## XXX: Instead of the two lines that follow.
## libamu_la_LIBADD = @LTLIBOBJS@ @LTALLOCA@
libamu_la_LIBADD = @AMU_LIB_OBJS@
libamu_la_DEPENDENCIES = @AMU_LIB_OBJS@


# LDFLAGS should include standard ones plus LIBTOOL ones
AM_LDFLAGS = @LDFLAGS@ @LIBTOOL_LDFLAGS@

AM_CPPFLAGS = -I$(top_srcdir)/include

# allow users to add their own flags via "configure --enable-am-flags=ARG"
AMU_CFLAGS = @AMU_CFLAGS@
AM_CFLAGS = $(AMU_CFLAGS)

# dependencies
$(libamu_la_OBJECTS) $(libamu_la_LIBADD):	\
	../config.h				\
	../aux_conf.h				\
	$(top_srcdir)/include/am_compat.h	\
	$(top_srcdir)/include/am_defs.h		\
	$(top_srcdir)/include/am_utils.h	\
	$(top_srcdir)/include/am_xdr_func.h	\
	$(top_srcdir)/include/amq_defs.h	\
	@AMU_NFS_PROT_HEADER@			\
	$(noinst_HEADERS)
