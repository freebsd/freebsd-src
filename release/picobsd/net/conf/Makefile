# $Id$
#

SRC?=/usr/src
CONF?=$(SRC)/sys/i386/conf

CONFFILE=PICOBSD-N.${SIZE}
COMPILE=$(SRC)/sys/compile/$(CONFFILE)
KERNFILE=$(COMPILE)/kernel

all: $(KERNFILE)

$(KERNFILE): PICOBSD
	cat PICOBSD | grep -v "MFS_ROOT" > ${CONF}/${CONFFILE}
	echo "options	\"MFS_ROOT=${SIZE}\"" >> ${CONF}/${CONFFILE}
	(cd ${CONF}; \
	config ${CONFFILE}; \
	cd ${COMPILE}; \
	make depend && make)

