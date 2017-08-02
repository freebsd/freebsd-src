BINDIR?=	/usr/bin
CFLAGS+=	-I${.CURDIR}/../../..
LDFLAGS+=	-libverbs -lrdmacm -lmlx4 -lmlx5 -lcxgb4 -lpthread
