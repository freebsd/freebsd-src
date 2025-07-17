PACKAGE=	tests

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/netlink

KMOD=	ktest_netlink_message_writer
SRCS=	ktest_netlink_message_writer.c

.include <bsd.kmod.mk>

