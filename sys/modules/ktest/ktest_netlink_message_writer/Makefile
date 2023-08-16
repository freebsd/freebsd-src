
PACKAGE=	tests

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/netlink

KMOD=	ktest_netlink_message_writer
SRCS=	ktest_netlink_message_writer.c
SRCS+=	opt_netlink.h

.include <bsd.kmod.mk>

