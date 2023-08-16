# $Id: Makefile,v 1.7 2003/08/14 20:06:17 max Exp $

PACKAGE=	bluetooth
CONFS=		bluetooth.device.conf
CONFSDIR=	/etc/defaults
PROG=		hccontrol
MAN=		hccontrol.8
SRCS=		send_recv.c link_policy.c link_control.c le.c\
		host_controller_baseband.c info.c status.c node.c hccontrol.c \
		util.c adv_data.c
WARNS?=		2

LIBADD=		bluetooth

.include <bsd.prog.mk>
