BINDIR?= /usr/bin

IBDIR=../../../..

CFLAGS+= \
	-I${IBDIR}/include/infiniband \
	-I${IBDIR}/include \
	-I${IBDIR}/librdmacm/include \
	-I${IBDIR}/libibv/include \
	-I${IBDIR}/libibverbs/include

