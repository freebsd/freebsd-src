#	$FreeBSD: src/tools/tools/crypto/Makefile,v 1.2.2.1 2003/02/24 22:52:36 sam Exp $

ALL=	cryptotest cryptokeytest cryptostats

all:	${ALL}

cryptotest: cryptotest.c
	${CC} -o cryptotest cryptotest.c

cryptokeytest: cryptokeytest.c
	${CC} -o cryptokeytest cryptokeytest.c -lcrypto

cryptostats: cryptostats.c
	${CC} -o cryptostats cryptostats.c

clean:
	rm -f ${ALL} core a.out
