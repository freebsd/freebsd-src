PROG=	vinum
SRCS=	v.c list.c parser.c util.c vext.h commands.c
MAN8=	vinum.8

CFLAGS=	-I${.CURDIR}/../../lkm/vinum -g -Wall -DDEBUG -DRAID5

LDADD+=		-lutil -lreadline -ltermcap
# DPADD+=		${LIBKVM}
BINGRP=		kmem
BINMODE=	2555

parser.c:
	rm -f $@
	ln -s ${.CURDIR}/../../lkm/vinum/$@ .

util.c:	statetexts.h
	rm -f $@
	ln -s ${.CURDIR}/../../lkm/vinum/$@ .

statetexts.h:
	(cd ${.CURDIR}/../../lkm/vinum; make $@)

.include <bsd.prog.mk>
