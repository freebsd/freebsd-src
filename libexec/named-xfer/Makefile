# $FreeBSD: src/libexec/named-xfer/Makefile,v 1.9.2.1 2001/01/29 20:40:18 asmodai Exp $

USE_LIBBIND=	true
.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/named-xfer
.PATH:	${BIND_DIR}/bin/named
.PATH:	${BIND_DIR}/doc/man

PROG=	named-xfer
SRCS=	pathnames.h \
	named-xfer.c db_glue.c ns_glue.c tmp_version.c 

.if exists(${.OBJDIR}/../../lib/libisc)
LIBISCDIR:=	${.OBJDIR}/../../lib/libisc
.else
LIBISCDIR!=	cd ${.CURDIR}/../../lib/libisc; make -V .OBJDIR
.endif
LIBISC:=	${LIBISCDIR}/libisc.a

DPADD+=		${LIBISC}
LDADD+=		${LIBISC}

MAN8=	named-xfer.8

.include <bsd.prog.mk>
