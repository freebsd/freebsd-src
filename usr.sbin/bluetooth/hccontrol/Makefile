# $Id: Makefile,v 1.6 2002/09/06 18:52:41 max Exp $
# $FreeBSD$

DESTDIR=	/usr/sbin/
MANDIR=		../share/man/man
PROG=		hccontrol
MAN8=		hccontrol.8
WARNS?=		2
CFLAGS+=	-g -I../../../sys/netgraph/bluetooth/include
SRCS=		send_recv.c link_policy.c link_control.c \
		host_controller_baseband.c info.c status.c node.c hccontrol.c \
		util.c

.include <bsd.prog.mk>
