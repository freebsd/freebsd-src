#
# $FreeBSD: src/lib/libauditd/Makefile,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
#

OPENBSMDIR=		${.CURDIR}/../../contrib/openbsm
LIBAUDITDDIR=		${OPENBSMDIR}/libauditd
LIBBSMDIR=		${OPENBSMDIR}/libbsm

LIB=		auditd

.PATH:		${LIBAUDITDDIR}

SRCS=	auditd_lib.c

#
# Must use BSM include files from within the contrib area, not the system.
#
CFLAGS+=	-I${OPENBSMDIR} -I${LIBBSMDIR}

NO_MAN=

.include <bsd.lib.mk>
