#
#	Hacked Makefile to compile and run the DES-certification program,
#	but not install anything.
#
# $Id: Makefile,v 1.4 1995/05/30 06:11:48 rgrimes Exp $
#
LIBCRYPT= $(.OBJDIR)/libdescrypt.a

#CFLAGS+= -DHAVE_CRYPT16

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

cert: cert.c ${LIBCRYPT}
	$(CC) $(CFLAGS) -o cert ${.CURDIR}/cert.c ${LIBCRYPT}

speedcrypt: speedcrypt.c ${LIBCRYPT}
	$(CC) $(CFLAGS) -o speedcrypt ${.CURDIR}/speedcrypt.c ${LIBCRYPT}


clean:
	rm -f ${TARGETS}

install:

obj:

.include <bsd.prog.mk>
