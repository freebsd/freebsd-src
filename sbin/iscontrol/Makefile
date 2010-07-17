# $FreeBSD: src/sbin/iscontrol/Makefile,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

SRCS= iscontrol.c pdu.c fsm.c config.c login.c auth_subr.c misc.c
PROG= iscontrol
DPADD= ${LIBCAM} ${LIBMD}
LDADD= -lcam -lmd

CFLAGS += -I${.CURDIR}/../../sys/dev/iscsi/initiator
#CFLAGS += -g -DDEBUG

MAN= iscsi.conf.5 iscontrol.8

.include <bsd.prog.mk>
