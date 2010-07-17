# $FreeBSD: src/usr.sbin/wpa/wpa_cli/Makefile,v 1.5.2.2.4.1 2010/06/14 02:09:06 kensmith Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH.c:${WPA_SUPPLICANT_DISTDIR}

PROG=	wpa_cli
SRCS=	wpa_cli.c wpa_ctrl.c os_unix.c

MAN=	wpa_cli.8

CFLAGS+= -DCONFIG_CTRL_IFACE
CFLAGS+= -DCONFIG_CTRL_IFACE_UNIX
# enable use of d_type to identify unix domain sockets
CFLAGS+= -D_DIRENT_HAVE_D_TYPE

#CFLAGS+= -DCONFIG_READLINE
#LDADD+= -ledit -ltermcap
#DPADD+= ${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
