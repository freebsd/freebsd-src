GROFF_DIST?=	${.CURDIR}/../../../../contrib/groff
DIST_DIR=	${GROFF_DIST}/${.CURDIR:T}
CFLAGS+=	-fno-for-scope

.PATH: ${DIST_DIR}
