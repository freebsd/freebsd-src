# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
LIB_BIND_DIR=	${.CURDIR}/..
SRCDIR=		${BIND_DIR}/lib/lwres

.include	"${LIB_BIND_DIR}/config.mk"

LIB=		lwres

.PATH:		${SRCDIR}
SRCS+=		context.c gai_strerror.c getaddrinfo.c gethost.c \
		getipnode.c getnameinfo.c getrrset.c herror.c \
		lwbuffer.c lwconfig.c lwpacket.c lwresutil.c \
		lwres_gabn.c lwres_gnba.c lwres_grbn.c lwres_noop.c \
		lwinetaton.c lwinetpton.c lwinetntop.c print.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/..

DPADD=		${LIBPTHREAD}
LDADD=		-lpthread

INCS=		${SRCDIR}/include/lwres/context.h \
		${SRCDIR}/include/lwres/int.h \
		${SRCDIR}/include/lwres/ipv6.h \
		${SRCDIR}/include/lwres/lang.h \
		${SRCDIR}/include/lwres/list.h \
		${SRCDIR}/include/lwres/lwbuffer.h \
		${SRCDIR}/include/lwres/lwpacket.h \
		${SRCDIR}/include/lwres/lwres.h \
		${SRCDIR}/include/lwres/result.h \
		${SRCDIR}/include/lwres/version.h \
		${SRCDIR}/unix/include/lwres/net.h \
		lwres/netdb.h \
		lwres/platform.h

INCSDIR=	${INCLUDEDIR}/lwres

.include <bsd.lib.mk>
