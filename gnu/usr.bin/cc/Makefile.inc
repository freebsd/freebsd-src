#
# $Id: Makefile.inc,v 1.13 1995/03/11 03:48:42 nate Exp $
#

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../include
CFLAGS+=	-Dbsd4_4
CFLAGS+=	-DGCC_INCLUDE_DIR=\"FOO\"
CFLAGS+=	-DTOOL_INCLUDE_DIR=\"FOO\"
CFLAGS+=	-DGPLUSPLUS_INCLUDE_DIR=\"FOO\"
CFLAGS+=	-DDEFAULT_TARGET_VERSION=\"2.6.3\"
CFLAGS+=	-DDEFAULT_TARGET_MACHINE=\"i386--freebsd\"
CFLAGS+=	-DSTANDARD_EXEC_PREFIX=\"/usr/libexec/\"
CFLAGS+=	-DSTANDARD_STARTFILE_PREFIX=\"/usr/lib/\"
CFLAGS+=	-DHAVE_PUTENV
CFLAGS+=	-DGCC_NAME=\"cc\"
CFLAGS+=	-DLINK_LIBGCC_SPECIAL_1

.if exists(${.OBJDIR}/../cc_int)
LIBDESTDIR=	${.OBJDIR}/../cc_int
.else
LIBDESTDIR=	${.CURDIR}/../cc_int
.endif

# XXX LDDESTDIR isn't a directory and there is no standard name for the dir
LDDESTDIR=	-L${LIBDESTDIR}
.if defined(SHARED_LIBCC_INT)
LIBCC_INT=	${LIBDESTDIR}/libcc_int.so.263.0
.else
LIBCC_INT=	${LIBDESTDIR}/libcc_int.a
.endif
