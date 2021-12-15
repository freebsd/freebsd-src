#	$FreeBSD$

.include <src.opts.mk>

WARNS?=		2
NO_WFORMAT=
NO_WARRAY_BOUNDS=

CFLAGS+=	-I${SRCTOP}/sys
CFLAGS+=	-I${SRCTOP}/sys/netpfil/ipfilter
CFLAGS+=	-I${SRCTOP}/sbin/ipf/common
CFLAGS+=	-DSTATETOP -D__UIO_EXPOSE

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=	-DUSE_INET6
.else
CFLAGS+=	-DNOINET6
.endif

.if ${.CURDIR:M*/libipf} == ""
LIBADD+=	ipf
.endif

CLEANFILES+=	y.tab.c y.tab.h

.PATH:	${SRCTOP}/sbin/ipf/libipf		\
	${SRCTOP}/sbin/ipf/common

.include "../Makefile.inc"
