_spath=${SRCTOP}/contrib/ofed/libibverbs
.PATH: ${_spath}/examples ${_spath}/man

BINDIR?=	/usr/bin
CFLAGS+=	-I${_spath}
LIBADD+=	ibverbs mlx4 mlx5 cxgb4 irdma pthread

WARNS?=	2
