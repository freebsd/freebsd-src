#	$Id: Makefile,v 1.14.4.2 1996/05/02 16:14:02 jdp Exp $

PROG=	ld.so
SRCS=	mdprologue.S rtld.c malloc.c shlib.c etc.c md.c
MAN1=	rtld.1
LDDIR?= $(.CURDIR)/..
PICFLAG=-fpic
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) $(PICFLAG) -DRTLD
LDFLAGS+=-Bshareable -Bsymbolic -assert nosymbolic
ASFLAGS+=-k
DPADD+=	${LIBC:S/c.a/c_pic.a/} ${LIBC:S/c.a/gcc_pic.a/}
LDADD+=	-lc_pic -lgcc_pic
BINDIR= /usr/libexec
MLINKS= rtld.1 ld.so.1

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

$(PROG): ${OBJS} ${DPADD}
	$(LD) -o $(PROG) $(LDFLAGS) $(OBJS) $(LDADD)

realinstall:
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${INSTALLFLAGS} ${PROG} ${DESTDIR}${BINDIR}/${PROG}.new
	test -f ${DESTDIR}${BINDIR}/${PROG}.old && \
		chflags noschg ${DESTDIR}${BINDIR}/${PROG}.old || \
		:
	-chflags noschg ${DESTDIR}${BINDIR}/${PROG}
	-ln -f ${DESTDIR}${BINDIR}/${PROG} ${DESTDIR}${BINDIR}/${PROG}.old
	mv ${DESTDIR}${BINDIR}/${PROG}.new ${DESTDIR}${BINDIR}/${PROG}
	chflags schg ${DESTDIR}${BINDIR}/${PROG}
	-rm ${DESTDIR}${BINDIR}/${PROG}.old

.include <bsd.prog.mk>
