# $FreeBSD$
BINDIR= /usr/local/bin

ALL= aac_checkq

all:	$(ALL)

aac_checkq: aac_checkq.c
	${CC} -o aac_checkq aac_checkq.c

install: ${ALL}
	${INSTALL} aac_checkq ${DESTDIR}${BINDIR}
