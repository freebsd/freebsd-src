#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

# missing libsysdecode
WANT_CHERI=none

.include <src.opts.mk>

.PATH: ${SRCTOP}/usr.bin/ktrace

PROG=		kdump
SRCS=		kdump.c subr.c
CFLAGS+=	-I${SRCTOP}/usr.bin/ktrace

LIBADD=		sysdecode
.if ${MK_CASPER} != "no"
LIBADD+=	casper
LIBADD+=	cap_grp
LIBADD+=	cap_pwd
CFLAGS+=-DHAVE_LIBCASPER
.endif

#NO_WERROR?=	YES

.include <bsd.prog.mk>
