#
# $Id: Makefile,v 1.3 1999/01/19 23:12:50 abial Exp $
#
SRC?=/usr/src

all: crunch

crunch:
#	@if [ ! -f /usr/src/usr.bin/passwd/.picobsd.patched ]; then \
#		cat passwd.diff|(cd /usr/src/usr.bin/passwd; patch -p0); \
#		touch /usr/src/usr.bin/passwd/.picobsd.patched; \
#	fi
	@cat crunch.conf|sed -e "s@/usr/src@${SRC}@" >crunch1.conf
	@crunchgen ./crunch1.conf
	@${MAKE} -f crunch1.mk -DNOPAM all \
	    "CFLAGS=${CFLAGS} -DNOPAM -DRELEASE_CRUNCH -DNOSECURE -DNOCRYPT" #2>&1 >/dev/null

clean:
#	@if [ -f /usr/src/usr.bin/passwd/.picobsd.patched ]; then \
#		cat passwd.diff|(cd /usr/src/usr.bin/passwd; patch -R -p0); \
#		rm /usr/src/usr.bin/passwd/.picobsd.patched; \
#	fi
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
	# Install the MIB files
	#cp mibs/*.txt ${MFS_MOUNTPOINT}/usr/local/share/snmp/mibs/


.include <bsd.prog.mk>
