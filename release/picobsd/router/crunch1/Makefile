#
# $FreeBSD$
#
SRC?=/usr/src

all: crunch

crunch:
	@cat crunch.conf|sed -e "s@/usr/src@${SRC}@" >crunch1.conf
	@crunchgen ./crunch1.conf
	@${MAKE} -f crunch1.mk NOSECURE=yes NOCRYPT=yes all \
	    "CFLAGS=${CFLAGS} -DRELEASE_CRUNCH" #2>&1 >/dev/null

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
	cp crunch1 ${MFS_MOUNTPOINT}/stand/crunch
	chmod 555 ${MFS_MOUNTPOINT}/stand/crunch
	for i in `crunchgen -l crunch1.conf` ; \
	do \
		ln ${MFS_MOUNTPOINT}/stand/crunch ${MFS_MOUNTPOINT}/stand/$${i}; \
	done
	rm ${MFS_MOUNTPOINT}/stand/crunch


.include <bsd.prog.mk>
