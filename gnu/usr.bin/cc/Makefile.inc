#
# $Id: Makefile.inc,v 1.4 1994/09/18 22:35:36 wollman Exp $
#

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../include
CFLAGS+=	-Dbsd4_4
CFLAGS+=	-DGCC_INCLUDE_DIR=\"FOO\"
CFLAGS+=	-DDEFAULT_TARGET_VERSION=\"2.6.0\"
CFLAGS+=	-DDEFAULT_TARGET_MACHINE=\"i386--freebsd\"
CFLAGS+=	-DMD_EXEC_PREFIX=\"/usr/libexec/\"
CFLAGS+=	-DSTANDARD_STARTFILE_PREFIX=\"/usr/lib\"
CFLAGS+=	-DGCC_NAME=\"cc\"

.if exists(${.CURDIR}/../cc_int/obj)
LIBDESTDIR=	${.CURDIR}/../cc_int/obj
.else
LIBDESTDIR=	${.CURDIR}/../cc_int
.endif

# XXX LDDESTDIR isn't a directory and there is no standard name for the dir
LDDESTDIR=	-L${LIBDESTDIR}
.if !defined(NO_SHARED_LIBCC_INT)
LIBCC_INT=	${LIBDESTDIR}/libcc_int.so.26.0
.else
LIBCC_INT=	${LIBDESTDIR}/libcc_int.a
.endif
