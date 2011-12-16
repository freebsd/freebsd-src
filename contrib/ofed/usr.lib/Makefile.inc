IBMGMT=		${.CURDIR}/../../management
IBCOMMONDIR=	${IBMGMT}/libibcommon
IBMADDIR=	${IBMGMT}/libibmad
UMADDIR=	${IBMGMT}/libibumad
OPENSMDIR=	${IBMGMT}/opensm
COMPLIBDIR=	${OPENSMDIR}/complib
VENDORLIBDIR=	${OPENSMDIR}/libvendor
IBVERBSDIR=	${.CURDIR}/../../libibverbs
IBINC=		${.CURDIR}/../../include

CFLAGS+=	-I${.CURDIR} -I${IBINC}/infiniband
CFLAGS+=	-I${IBCOMMONDIR}/include/infiniband
CFLAGS+=	-I${IBMADDIR}/include/infiniband
CFLAGS+=	-I${UMADDIR}/include/infiniband
CFLAGS+=	-I${OPENSMDIR}/include
# CFLAGS+=	-I${UMADDIR}/include
# CFLAGS+=	-I${IBVERBSDIR}/include
