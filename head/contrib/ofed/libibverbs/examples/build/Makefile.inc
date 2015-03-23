BINDIR?= /usr/bin

CFLAGS+= \
	-I../../../../../../sys/ofed/include \
	-I../../../../libibverbs/include \
	-I../../../../include

LDADD+= -libverbs -lmlx4 -lmthca -pthread

