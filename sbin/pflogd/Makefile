# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/pf/pflogd

PACKAGE=pf
PROG=	pflogd
SRCS= 	pflogd.c pidfile.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

CFLAGS+=-include ${.CURDIR}/../../lib/libpcap/config.h

LIBADD=	pcap

WARNS?=	2

.include <bsd.prog.mk>
