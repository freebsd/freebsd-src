#
# $FreeBSD$
#
SRC?=/usr/src

COMMOBJ?=	${.OBJDIR}/../../common-obj

all: crunch

crunch1.conf: crunch.conf
	@cat ${.ALLSRC} | sed -e "s@/usr/src@${SRC}@" > ${.TARGET}

crunch.mk: crunch1.conf crunch.inc
	@crunchgen -h ${.CURDIR}/crunch.inc -m crunch.mk crunch1.conf

crunch1.mk: crunch.mk
	@sed -e "s@make depend@make obj \&\& make depend@" \
		-e "s@/usr/obj/@${COMMOBJ}@" crunch.mk > ${.TARGET}

crunch: crunch1.mk
	@if [ ! -d build ]; then mkdir build; fi
	@cp crunch1.c build/
	@(cd build && env MAKEOBJDIRPREFIX=${COMMOBJ} \
	    ${MAKE} -f ../crunch1.mk -DNOPAM all \
	    "CFLAGS=${CFLAGS} -DRELEASE_CRUNCH -DNOSECURE -DNOCRYPT") #2>&1 >/dev/null

clean:
	rm -rf build
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
	cp build/crunch1 ${MFS_MOUNTPOINT}/sbin/crunch
	chmod 555 ${MFS_MOUNTPOINT}/sbin/crunch
	for i in `crunchgen -l crunch1.conf` ; \
	do \
		ln ${MFS_MOUNTPOINT}/sbin/crunch ${MFS_MOUNTPOINT}/sbin/$${i}; \
	done
	rm ${MFS_MOUNTPOINT}/sbin/crunch
	# Install the MIB files
	#cp mibs/*.txt ${MFS_MOUNTPOINT}/usr/local/share/snmp/mibs/


.include <bsd.prog.mk>
