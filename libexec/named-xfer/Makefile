# $FreeBSD: src/libexec/named-xfer/Makefile,v 1.9 1999/08/28 00:09:43 peter Exp $

USE_LIBBIND=	true
.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/named-xfer
.PATH:	${BIND_DIR}/bin/named
.PATH:	${BIND_DIR}/doc/man

PROG=	named-xfer
SRCS=	pathnames.h \
	named-xfer.c db_glue.c ns_glue.c tmp_version.c 
MAN8=	named-xfer.8

.include <bsd.prog.mk>
