#	@(#)Makefile	8.3 (Berkeley) 3/27/94

PROG=	mount_portal
SRCS=	mount_portal.c activate.c conf.c getmntopts.c pt_conf.c \
	pt_exec.c pt_file.c pt_tcp.c
MAN8=	mount_portal.0

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I/sys -I${MOUNT}
.PATH:	${MOUNT}

DPADD=	$(LIBCOMPAT)
LDADD=	-lcompat

.include <bsd.prog.mk>
