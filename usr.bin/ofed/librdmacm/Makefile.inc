
_spath=${SRCTOP}/contrib/ofed/librdmacm
.PATH: ${_spath}/examples ${_spath}/man

BINDIR?=	/usr/bin
CFLAGS+=	-I${SRCTOP}/contrib/ofed
LIBADD+=	ibverbs rdmacm irdma mlx4 mlx5 cxgb4 pthread

WARNS?=	0
