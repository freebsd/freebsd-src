#
# $FreeBSD$
#
SRC?=/usr/src

all: crunch

crunch:
	@cat crunch.conf|sed -e "s@/usr/src@${SRC}@" >crunch1.conf
	@crunchgen ./crunch1.conf
	@${MAKE} -f crunch1.mk -DNOPAM all \
	    "CFLAGS=${CFLAGS} -DRELEASE_CRUNCH -DCRUNCHED_BINARY -DNOSECURE -DNOCRYPT" #2>&1 >/dev/null

clean:
	rm  -f *.o *.stub *.lo *_stub.c *.mk \
		crunch.cache \
		crunch.mk \
		crunch.c \
		crunch1* \
		crunch \
		.tmp_* \
		*~ \
		*.gz

install: 
	rm -f ${MFS_MOUNTPOINT}/sbin/*
	cp crunch1 ${MFS_MOUNTPOINT}/sbin/crunch
	chmod 555 ${MFS_MOUNTPOINT}/sbin/crunch
	for i in `crunchgen -l crunch1.conf` ; \
	do \
		ln ${MFS_MOUNTPOINT}/sbin/crunch ${MFS_MOUNTPOINT}/sbin/$${i}; \
	done
	rm ${MFS_MOUNTPOINT}/sbin/crunch
	# Install the MIB files
	#cp mibs/*.txt ${MFS_MOUNTPOINT}/usr/local/share/snmp/mibs/


.include <bsd.prog.mk>
