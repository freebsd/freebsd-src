#
# $Id: Makefile,v 1.3.2.1 1999/05/07 10:02:34 abial Exp $
#
NOCRYPT?=	yes
SRC?=/usr/src

all: crunch

crunch:
	if [ "X${INIT}" != "X" ]; \
	then \
		echo "progs ${INIT}" >crunch1.conf ; \
	else \
		echo "progs init getty" >crunch1.conf ; \
	fi;
	@cat crunch.conf|sed -e "s@/usr/src@${SRC}@" >>crunch1.conf
	@crunchgen ${.CURDIR}/crunch1.conf
	@${MAKE} -DRELEASE_CRUNCH -f crunch1.mk all NOCRYPT=yes \
	    "CFLAGS=${CFLAGS} -DCRUNCHED_BINARY" #2>&1 >/dev/null

clean:
	rm  -f *.o *.stub *.lo *_stub.c *.mk \
		crunch.cache \
		crunch.mk \
		crunch.c \
		crunch \
		crunch1* \
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
