#	$Id: Makefile,v 1.1 1997/05/21 14:18:26 yokota Exp $

KMOD=	daemon_saver_mod
SRCS= 	daemon_saver.c

NOMAN=
CFLAGS+= -DLKM -I${.CURDIR}/.. -I${.CURDIR}/../../../sys

# Omits the bouncing message text and shows only the daemon.
#CFLAGS+= -DDAEMON_ONLY

# Includes the host name in the message text.
#CFLAGS+= -DSHOW_HOSTNAME

.include <bsd.kmod.mk>
