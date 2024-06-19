
PACKAGE=examples
FILESDIR=${SHAREDIR}/examples/${PROG}
PROG=	scsi_target
SRCS=	scsi_target.h scsi_target.c scsi_cmds.c
DPADD=	${LIBCAM} ${LIBSBUF}
LIBADD+=	cam
LIBADD+=	sbuf
# cast-qual is triggered only in a code path where the volatile keyword doesn't
# matter
CFLAGS.scsi_cmds.c=	-Wno-cast-qual

MAN=	scsi_target.8

.include <bsd.prog.mk>
