#
#	Hacked Makefile to compile and run the DES-certification program,
#	but not install anything.
#
# $Id: Makefile,v 1.5 1996/06/24 20:28:35 markm Exp $
#
LIBCRYPT= -lcrypt

#CFLAGS+= -DHAVE_CRYPT16
CFLAGS+= -ggdb

TARGETS=cert speedcrypt

all: ${TARGETS}

test: all testcrypt testspeed

testcrypt: cert
	@./cert -c

testspeed: cryptspeed

cryptspeed: speedcrypt
	@./speedcrypt 30 1
	@./speedcrypt 30 1
	@./speedcrypt 30 0
	@./speedcrypt 30 0

cert: cert.c
	$(CC) $(CFLAGS) -o cert ${.CURDIR}/cert.c ${LIBCRYPT}

speedcrypt: speedcrypt.c
	$(CC) $(CFLAGS) -o speedcrypt ${.CURDIR}/speedcrypt.c ${LIBCRYPT}

clean:
	rm -f ${TARGETS}

install:

obj:

.include <bsd.prog.mk>
