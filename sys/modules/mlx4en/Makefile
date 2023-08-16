.PATH:  ${SRCTOP}/sys/dev/mlx4/mlx4_en

KMOD=	mlx4en
SRCS= \
	mlx4_en_cq.c \
	mlx4_en_main.c \
	mlx4_en_netdev.c \
	mlx4_en_port.c \
	mlx4_en_resources.c \
	mlx4_en_rx.c \
	mlx4_en_tx.c
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_inet.h opt_inet6.h

CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= ${LINUXKPI_INCLUDES}

.include <bsd.kmod.mk>
