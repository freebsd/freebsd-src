#
# $FreeBSD$
#
SRC?=/usr/src

all: crunch install

crunch:
	@cat crunch.conf|sed -e "s@/usr/src@${SRC}@" >crunch2.conf
	@crunchgen ./crunch2.conf
	${MAKE} -f crunch2.mk RELEASE_CRUNCH= -DNOPAM all \
	    "CFLAGS=${CFLAGS} -DRELEASE_CRUNCH -DCRUNCHED_BINARY -DNOSECURE -DNOCRYPT"

clean:
	rm  -f *.o *.stub *.lo *_stub.c *.mkcrunch.cache Make.log *~ 
	rm -f crunch.mk crunch.c crunch2* crunch .tmp_* *.gz 
	rm -rf floppy2

install: floppy2	
	rm -rf floppy2/sbin/*
	cp crunch2 floppy2/sbin/crunch
	chmod 555 floppy2/sbin/crunch
	for i in `crunchgen -l crunch2.conf` ; \
	do \
		ln floppy2/sbin/crunch floppy2/sbin/$${i}; \
	done
	rm floppy2/sbin/crunch

floppy2:
	mkdir -p $@/sbin

.include <bsd.prog.mk>
