# $Id: Makefile,v 1.4 1997/02/22 16:08:18 peter Exp $

.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/ndc
.PATH:	${BIND_DIR}/doc/man

PROG=	ndc
STRIP=
MAN8=	ndc.8

CLEANFILES+= ndc

all:	ndc

ndc:	ndc.sh ndcedit.awk ${.CURDIR}/../../usr.sbin/named/Makefile.inc
	sed -e "s|%PIDFILE%|${PIDDIR}/named.pid|" \
	    -e "s|%NAMED%|named|" \
	    -e "s|%PS%|${PS}|" \
	    < ${BIND_DIR}/bin/ndc/ndc.sh | awk -f ${.CURDIR}/ndcedit.awk > ndc

.include <bsd.prog.mk>
