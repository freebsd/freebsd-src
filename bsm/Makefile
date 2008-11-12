#
# $P4: //depot/projects/trustedbsd/openbsm/bsm/Makefile#7 $
#

INCS=	audit.h								\
	audit_internal.h						\
	audit_kevents.h							\
	audit_record.h							\
	audit_uevents.h							\
	libbsm.h

TARGET=	${DESTDIR}/usr/include/bsm

all:
default:
depend:
clean:

install:
	mkdir -p -m 0755 ${TARGET}
	install -o root -g wheel -m 0644 ${INCS} ${TARGET}

