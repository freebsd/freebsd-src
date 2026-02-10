.include <src.opts.mk>

PACKAGE=ipfw15
PROG=	ipfw15

CFLAGS+=	-I ${.CURDIR}/include15

LINKS=	${BINDIR}/ipfw15 ${BINDIR}/dnctl15
MK_MAN=	no

SRCS=	ipfw2.c dummynet.c ipv6.c main.c nat.c tables.c
SRCS+=	nat64clat.c nat64lsn.c nat64stl.c nptv6.c

.if ${MK_PF} != "no"
SRCS+=	altq.c
CFLAGS+=-DPF
.endif

LIBADD=	jail util

.include <bsd.prog.mk>

CWARNFLAGS+= -Wno-cast-align
