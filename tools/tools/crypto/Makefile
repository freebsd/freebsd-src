#	$FreeBSD$

ALL=	cryptotest cryptokeytest cryptostats

all:	${ALL}

cryptotest: cryptotest.c
	${CC} -o cryptotest cryptotest.c

cryptokeytest: cryptokeytest.c
	${CC} -o cryptokeytest cryptokeytest.c -lcrypto

cryptostats: cryptostats.c
	${CC} -o cryptostats cryptostats.c

ubsecstats: ubsecstats.c
	${CC} -o ubsecstats ubsecstats.c

clean:
	rm -f ${ALL} core a.out
