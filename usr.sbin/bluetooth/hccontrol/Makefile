# $Id: Makefile,v 1.7 2003/08/14 20:06:17 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/hccontrol/Makefile,v 1.3.36.1 2010/12/21 17:10:29 kensmith Exp $

PROG=		hccontrol
MAN=		hccontrol.8
SRCS=		send_recv.c link_policy.c link_control.c \
		host_controller_baseband.c info.c status.c node.c hccontrol.c \
		util.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH}
LDADD=		-lbluetooth

.include <bsd.prog.mk>
