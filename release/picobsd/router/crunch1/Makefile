#
# $FreeBSD: src/release/picobsd/router/crunch1/Makefile,v 1.5 1999/08/28 01:33:47 peter Exp $
#
SRC?=/usr/src

all: crunch

crunch:
	@cat crunch.conf|sed -e "s@/usr/src@${SRC}@" >crunch1.conf
	@crunchgen ./crunch1.conf
	@${MAKE} -f crunch1.mk NOSECURE=yes NOCRYPT=yes all \
	    "CFLAGS=${CFLAGS} -DCRUNCHED_BINARY -DRELEASE_CRUNCH" #2>&1 >/dev/null

clean:
	rm  -f *.o *.stub *.lo *_stub.c *.mk \
		crunch.cache \
		crunch.mk \
		crunch.c \
		crunch1* \
		crunch \
		.tmp_* \
		*.gz

install: 
	cp crunch1 /mnt/stand/crunch
	chmod 555 /mnt/stand/crunch
	for i in `crunchgen -l crunch1.conf` ; \
	do \
		ln /mnt/stand/crunch /mnt/stand/$${i}; \
	done
	rm /mnt/stand/crunch


.include <bsd.prog.mk>
