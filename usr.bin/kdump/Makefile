#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <src.opts.mk>

.PATH: ${.CURDIR}/../ktrace

PROG=		kdump
SRCS=		kdump_subr.c kdump_subr.h kdump.c subr.c
CFLAGS+=	-I${.CURDIR}/../ktrace -I${.CURDIR} -I${.CURDIR}/../.. -I.

LIBADD=		sysdecode
.if ${MK_CASPER} != "no"
LIBADD+=	casper
LIBADD+=	cap_grp
LIBADD+=	cap_pwd
CFLAGS+=-DHAVE_LIBCASPER
.endif

NO_WERROR?=	YES

CLEANFILES=	kdump_subr.c kdump_subr.h

kdump_subr.h: mksubr
	sh ${.CURDIR}/mksubr ${DESTDIR}${INCLUDEDIR} | \
	    sed -n 's/^\([a-z].*)\)$$/void \1;/p' >${.TARGET}

kdump_subr.c: mksubr kdump_subr.h
	sh ${.CURDIR}/mksubr ${DESTDIR}${INCLUDEDIR} >${.TARGET}

.include <bsd.prog.mk>
