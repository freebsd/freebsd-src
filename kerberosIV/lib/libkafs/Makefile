# $FreeBSD$

LIB=		kafs
CFLAGS+=	-I${KRB4DIR}/include \
		-I${INCLUDEOBJDIR} \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/lib/kafs
SRCS=		afssys.c afskrb.c common.c issuid.c \
		strlcpy.c strlcat.c
MAN=		${KRB4DIR}/man/kafs.3
MLINKS=		kafs.3 k_hasafs.3 kafs.3 k_afsklog.3 kafs.3 k_afsklog_uid.3 \
		kafs.3 k_pioctl.3 kafs.3 k_unlog.3 kafs.3 k_setpag.3 \
		kafs.3 k_afs_cell_of_file.3


INCLUDES=	${KRB4DIR}/lib/kafs/kafs.h

.include <bsd.lib.mk>

.PATH:		${KRB4DIR}/lib/kafs
.PATH:		${KRB4DIR}/lib/roken
