#
#	Hacked Makefile to compile and run the DES-certification program,
#	but not install anything.
#
# $Id: Makefile,v 1.1.1.1 1994/04/04 14:57:18 g89r4222 Exp $
#
LIBCRYPT!=cd $(.CURDIR)/..; \
	printf "xxx:\n\techo \$${.OBJDIR}/libcrypt.a\n" | make -r -s -f - xxx

#CFLAGS+= -DHAVE_CRYPT16

TARGETS=cert speedcrypt speeddes

all: ${TARGETS}

test: all testcrypt testencrypt testdes testspeed

testcrypt: cert
	@./cert -c

testencrypt: cert
	@./cert -e < ${.CURDIR}/cert.input

testdes: cert
	@./cert -d < ${.CURDIR}/cert.input

testspeed: cryptspeed desspeed

cryptspeed: speedcrypt
	@./speedcrypt 30 1
	@./speedcrypt 30 1
	@./speedcrypt 30 0
	@./speedcrypt 30 0

desspeed: speeddes
	@./speeddes 30 1
	@./speeddes 30 1
	@./speeddes 40 25
	@./speeddes 40 25

cert: cert.c ${LIBCRYPT}
	$(CC) $(CFLAGS) -o cert ${.CURDIR}/cert.c ${LIBCRYPT}

speedcrypt: speedcrypt.c ${LIBCRYPT}
	$(CC) $(CFLAGS) -o speedcrypt ${.CURDIR}/speedcrypt.c ${LIBCRYPT}

speeddes: speeddes.c ${LIBCRYPT}
	$(CC) $(CFLAGS) -o speeddes ${.CURDIR}/speeddes.c ${LIBCRYPT}

clean:
	rm -f ${TARGETS}

install:

.include <bsd.obj.mk>
