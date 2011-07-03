#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

.include <bsd.own.mk>

PROG=	ifconfig

SRCS=	ifconfig.c		# base support

#
# NB: The order here defines the order in which the constructors
#     are called.  This in turn defines the default order in which
#     status is displayed.  Probably should add a priority mechanism
#     to the registration process so we don't depend on this aspect
#     of the toolchain.
#
SRCS+=	af_link.c		# LLC support
.if ${MK_INET_SUPPORT} != "no"
SRCS+=	af_inet.c		# IPv4 support
.endif
.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	af_inet6.c		# IPv6 support
.endif
SRCS+=	af_atalk.c		# AppleTalk support
.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	af_nd6.c		# ND6 support
.endif

SRCS+=	ifclone.c		# clone device support
SRCS+=	ifmac.c			# MAC support
SRCS+=	ifmedia.c		# SIOC[GS]IFMEDIA support
SRCS+=	iffib.c			# non-default FIB support
SRCS+=	ifvlan.c		# SIOC[GS]ETVLAN support
SRCS+=	ifgre.c			# GRE keys etc
SRCS+=	ifgif.c			# GIF reversed header workaround

SRCS+=	ifieee80211.c regdomain.c # SIOC[GS]IEEE80211 support
DPADD+=	${LIBBSDXML} ${LIBJAIL} ${LIBSBUF}
LDADD+=	-lbsdxml -ljail -lsbuf

SRCS+=	ifcarp.c		# SIOC[GS]VH support
SRCS+=	ifgroup.c		# ...
SRCS+=	ifpfsync.c		# pfsync(4) support

SRCS+=	ifbridge.c		# bridge support
SRCS+=	iflagg.c		# lagg support

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif
.if ${MK_INET_SUPPORT} != "no"
CFLAGS+= -DINET
.endif
.if ${MK_IPX_SUPPORT} != "no" && !defined(RELEASE_CRUNCH)
SRCS+=	af_ipx.c		# IPX support
DPADD+=	${LIBIPX}
LDADD+=	-lipx
.endif

MAN=	ifconfig.8

CFLAGS+= -Wall -Wmissing-prototypes -Wcast-qual -Wwrite-strings -Wnested-externs
WARNS?=	2

.include <bsd.prog.mk>
