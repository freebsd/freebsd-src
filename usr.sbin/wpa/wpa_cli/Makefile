# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

.PATH.c:${WPA_SUPPLICANT_DISTDIR}

PROG=	wpa_cli
SRCS=	common.c edit.c eloop.c os_unix.c wpa_cli.c wpa_ctrl.c wpa_debug.c

MAN=	wpa_cli.8

CFLAGS+= -DCONFIG_CTRL_IFACE
CFLAGS+= -DCONFIG_CTRL_IFACE_UNIX
# enable use of d_type to identify unix domain sockets
CFLAGS+= -D_DIRENT_HAVE_D_TYPE

CFLAGS+= -DCONFIG_READLINE -I${DESTDIR}/${INCLUDEDIR}/edit
LDADD+= -ledit -ltermcapw -lutil
DPADD+= ${LIBEDIT} ${LIBTERMCAPW} ${LIBUTIL}

.include <bsd.prog.mk>
