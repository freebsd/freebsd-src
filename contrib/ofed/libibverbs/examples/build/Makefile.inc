BINDIR?=	/usr/bin
CFLAGS+=	-I${.CURDIR}/../../..
LDFLAGS+=	-libverbs -lmlx4 -lmlx5 -lcxgb4 -lpthread

