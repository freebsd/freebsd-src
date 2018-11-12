IBMGMT=		${.CURDIR}/../../management
IBCOMMONDIR=	${IBMGMT}/libibcommon
IBMADDIR=	${IBMGMT}/libibmad
UMADDIR=	${IBMGMT}/libibumad
OPENSMDIR=	${IBMGMT}/opensm
COMPLIBDIR=	${OPENSMDIR}/complib
VENDORLIBDIR=	${OPENSMDIR}/libvendor
IBVERBSDIR=	${.CURDIR}/../../libibverbs
IBINC=		${.CURDIR}/../../include
RDMACMDIR=	${.CURDIR}/../../librdmacm

CFLAGS+=	-I${.CURDIR} \
		-I${IBINC}/infiniband \
		-I${IBINC} \
		-I${IBCOMMONDIR}/include/infiniband \
		-I${IBCOMMONDIR}/include \
		-I${IBMADDIR}/include/infiniband \
		-I${IBMADDIR}/include \
		-I${UMADDIR}/include/infiniband \
		-I${UMADDIR}/include \
		-I${OPENSMDIR}/include \
		-I${RDMACMDIR}/include \
		-I${IBVERBSDIR}/include

