#
#	Hacked Makefile to compile and run the DES-certification program,
#	but not install anything.
#
# $Id: Makefile,v 1.2 1994/08/08 17:29:02 csgr Exp $
#
.if exists (../obj)
OBJDIR = obj
.else
OBJDIR = 
.endif

LIBCRYPT= $(.CURDIR)/../$(OBJDIR)/libdescrypt.a

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
