# $FreeBSD$

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

NOSHARED?=	YES

MAN=	named-xfer.8

.include <bsd.prog.mk>
