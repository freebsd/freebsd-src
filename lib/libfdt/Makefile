LIB=		fdt

INTERNALLIB=

LIBFDTSRCDIR=	${SRCTOP}/sys/contrib/libfdt
.PATH:		${LIBFDTSRCDIR}

SRCS=					\
		fdt.c			\
		fdt_addresses.c		\
		fdt_empty_tree.c	\
		fdt_overlay.c		\
		fdt_ro.c		\
		fdt_rw.c		\
		fdt_strerror.c		\
		fdt_sw.c		\
		fdt_wip.c

INCS=					\
		fdt.h			\
		libfdt.h		\
		ibfdt_env.h

CFLAGS+=	-I${LIBFDTSRCDIR}
WARNS?=		2

MAN=

.include <bsd.lib.mk>
