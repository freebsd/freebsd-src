# $FreeBSD: src/usr.sbin/ngctl/Makefile,v 1.22.10.1 2010/02/10 00:26:20 kensmith Exp $
# $Whistle: Makefile,v 1.3 1999/01/16 00:10:11 archie Exp $

.include <bsd.own.mk>

PROG=	ngctl
MAN=	ngctl.8
SRCS=	main.c mkpeer.c config.c connect.c dot.c name.c show.c list.c \
	msg.c debug.c shutdown.c rmhook.c status.c types.c write.c
WARNS?=	3

.if defined(RELEASE_CRUNCH)
NGCTL_NO_LIBEDIT=
.endif

.if ${MK_LIBTHR} == "no" && ${MK_LIBKSE} == "no"
NGCTL_NO_LIBEDIT=
.endif

DPADD=	${LIBNETGRAPH}
LDADD=	-lnetgraph

.if !defined(NGCTL_NO_LIBEDIT)
CFLAGS+= -DEDITLINE
DPADD+=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD+=	-lpthread -ledit -ltermcap
.endif

.include <bsd.prog.mk>
