#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	tn3270
CFLAGS+=-I${.CURDIR} -I.
LDADD+=	-lcurses -ltermcap -ltelnet
DPADD+=	${LIBCURSES} /usr/lib/libtermcap.a /usr/lib/libtelnet.a
CLEANFILES+= asc_disp.OUT asc_disp.out disp_asc.OUT disp_asc.out TMPfunc.out
.PATH:	${.CURDIR}/../api ${.CURDIR}/../ascii ${.CURDIR}/../ctlr
.PATH:	${.CURDIR}/../general ${.CURDIR}/../sys_curses ${.CURDIR}/../../telnet

MAN1=	tn3270.0

SRCS+=	apilib.c api_bsd.c api_exch.c asc_ebc.c astosc.c dctype.c
SRCS+=	disp_asc.c ebc_disp.c
SRCS+=	map3270.c termin.c
SRCS+=	api.c function.c inbound.c oia.c options.c outbound.c
SRCS+=	genbsubs.c globals.c system.c termout.c
SRCS+=	commands.c main.c network.c ring.c sys_bsd.c telnet.c terminal.c
SRCS+=	tn3270.c utilities.c

# This and the dependency hacks below to make 'depend' target
# work right...

DEPSRCS+= astosc.OUT asc_disp.OUT disp_asc.OUT kbd.OUT
DEPSRCS+= apilib.c api_bsd.c api_exch.c asc_ebc.c dctype.c
DEPSRCS+= ebc_disp.c
DEPSRCS+= map3270.c termin.c
DEPSRCS+= api.c function.c inbound.c oia.c options.c outbound.c
DEPSRCS+= genbsubs.c globals.c system.c termout.c
DEPSRCS+= commands.c main.c network.c ring.c sys_bsd.c telnet.c terminal.c
DEPSRCS+= tn3270.c utilities.c

astosc.o: astosc.OUT
CLEANFILES+= astosc.OUT astosc.out
astosc.OUT: ${.CURDIR}/../ctlr/hostctlr.h ${.CURDIR}/../ctlr/function.h
astosc.OUT: ${.CURDIR}/../ctlr/${KBD} ${.CURDIR}/../tools/mkastosc/obj/mkastosc
	${.CURDIR}/../tools/mkastosc/obj/mkastosc \
	    ${.CURDIR}/../ctlr/hostctlr.h \
	    ${.CURDIR}/../ctlr/function.h < ${.CURDIR}/../ctlr/${KBD} \
	    > ${.TARGET}
	rm -f astosc.out; ln -s astosc.OUT astosc.out

disp_asc.o: asc_disp.OUT disp_asc.OUT
asc_disp.OUT: ${.CURDIR}/../tools/mkastods/obj/mkastods
	${.CURDIR}/../tools/mkastods/obj/mkastods > ${.TARGET}
	rm -f asc_disp.out; ln -s asc_disp.OUT asc_disp.out

disp_asc.OUT: ${.CURDIR}/../tools/mkdstoas/obj/mkdstoas
	${.CURDIR}/../tools/mkdstoas/obj/mkdstoas > ${.TARGET}
	rm -f disp_asc.out; ln -s disp_asc.OUT disp_asc.out

inbound.o: kbd.OUT
CLEANFILES += kbd.OUT kbd.out
kbd.OUT: ${.CURDIR}/../ctlr/hostctlr.h ${.CURDIR}/../ctlr/${KBD}
kbd.OUT: ${.CURDIR}/../tools/mkhits/obj/mkhits
	${CC} ${CFLAGS} -E ${.CURDIR}/../ctlr/function.c > TMPfunc.out
	${.CURDIR}/../tools/mkhits/obj/mkhits ${.CURDIR}/../ctlr/hostctlr.h \
	    TMPfunc.out < ${.CURDIR}/../ctlr/${KBD} > ${.TARGET}
	rm -f kbd.out; ln -s kbd.OUT kbd.out

# astosc.out
# asc_disp.out disp_asc.out
# default.map
# kbd.out

${.CURDIR}/../tools/mkastosc/obj/mkastosc:
	cd ${.CURDIR}/../tools/mkastosc; make
${.CURDIR}/../tools/mkastods/obj/mkastods:
	cd ${.CURDIR}/../tools/mkastods; make
${.CURDIR}/../tools/mkdstoas/obj/mkdstoas:
	cd ${.CURDIR}/../tools/mkdstoas; make
${.CURDIR}/../tools/mkhits/obj/mkhits:
	cd ${.CURDIR}/../tools/mkhits; make

depend: .depend
.depend: ${DEPSRCS}
	mkdep ${MKDEP} ${CFLAGS:M-[ID]*} ${.ALLSRC:M*.c}

.include <../../Makefile.inc>
.include <bsd.prog.mk>
