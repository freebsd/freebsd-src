# $Id: Makefile,v 1.3 1998/10/15 21:40:05 abial Exp $
#

SRC?=/usr/src
CONF?=$(SRC)/sys/i386/conf

CONFFILE=PICOBSD${suffix}.${SIZE}
COMPILE=$(SRC)/sys/compile/$(CONFFILE)
KERNFILE=$(COMPILE)/kernel

all: $(KERNFILE)

$(KERNFILE): PICOBSD
	cat PICOBSD | grep -v "MFS_ROOT" > ${CONF}/${CONFFILE}
	echo "options	MFS_ROOT" >> ${CONF}/${CONFFILE}
	echo "options	MFS_ROOT_SIZE=${SIZE}" >> ${CONF}/${CONFFILE}
	(cd ${CONF}; \
	config ${CONFFILE}; \
	cd ${COMPILE}; \
	make depend all)

