PACKAGE=nvme-tools
NVMECONTROLDIR=	${SRCTOP}/sbin/nvmecontrol

MK_INSTALLLIB=	no

CFLAGS+=	-I${NVMECONTROLDIR}

SHLIB_NAME?=	${LIB}.so
LIBDIR=		/lib/nvmecontrol
