.PATH: ${SRCTOP}/sys/kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h opt_capsicum.h

EXPORT_SYMS=	\
	kern_kmq_notify \
	kern_kmq_open \
	kern_kmq_setattr \
	kern_kmq_timedreceive \
	kern_kmq_timedsend \
	sys_kmq_unlink

.include <bsd.kmod.mk>
