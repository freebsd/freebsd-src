# This mess is to make tn3270 work with Berkeley Make
# We also need to check for obj/no_obj directories

.if exists(${.CURDIR}/../tools/mkastosc/obj)
SC_OBJ_DIR=obj
.else
SC_OBJ_DIR=.
.endif

.if exists(${.CURDIR}/../tools/mkastods/obj)
DS_OBJ_DIR=obj
.else
DS_OBJ_DIR=.
.endif

.if exists(${.CURDIR}/../tools/mkdstoas/obj)
AS_OBJ_DIR=obj
.else
AS_OBJ_DIR=.
.endif

.if exists(${.CURDIR}/../tools/mkhits/obj)
HITS_OBJ_DIR=obj
.else
HITS_OBJ_DIR=.
.endif

.include <../../Makefile.inc>

CFLAGS += -I${.CURDIR} -I.
LDADD += -lcurses -ltermcap -ltelnet
DPADD += ${LIBCURSES} /usr/lib/libtermcap.a /usr/lib/libtelnet.a

MAN1 = tn3270.1

SRCS += apilib.c api_bsd.c api_exch.c asc_ebc.c astosc.c dctype.c
SRCS += disp_asc.c ebc_disp.c
SRCS += map3270.c termin.c
SRCS += api.c function.c inbound.c oia.c options.c outbound.c
SRCS += genbsubs.c globals.c system.c termout.c
SRCS += commands.c main.c network.c ring.c sys_bsd.c telnet.c terminal.c
SRCS += tn3270.c utilities.c

# this and the dependency hacks below to make 'depend' target
# work right...

DEPSRCS += astosc.OUT asc_disp.OUT disp_asc.OUT kbd.OUT
DEPSRCS += apilib.c api_bsd.c api_exch.c asc_ebc.c dctype.c
DEPSRCS += ebc_disp.c
DEPSRCS += map3270.c termin.c
DEPSRCS += api.c function.c inbound.c oia.c options.c outbound.c
DEPSRCS += genbsubs.c globals.c system.c termout.c
DEPSRCS += commands.c main.c network.c ring.c sys_bsd.c telnet.c terminal.c
DEPSRCS += tn3270.c utilities.c

PROG = tn3270

astosc.o: astosc.OUT
CLEANFILES += astosc.OUT astosc.out

astosc.OUT: ${.CURDIR}/../ctlr/hostctlr.h ${.CURDIR}/../ctlr/function.h \
	${.CURDIR}/../ctlr/${KBD} ${.CURDIR}/../tools/mkastosc/${SC_OBJ_DIR}/mkastosc
	${.CURDIR}/../tools/mkastosc/${SC_OBJ_DIR}/mkastosc ${.CURDIR}/../ctlr/hostctlr.h \
	${.CURDIR}/../ctlr/function.h < ${.CURDIR}/../ctlr/${KBD} \
		> ${.TARGET}
	/bin/rm -f astosc.out
	ln -s astosc.OUT astosc.out

disp_asc.o: asc_disp.OUT disp_asc.OUT
CLEANFILES += asc_disp.OUT asc_disp.out disp_asc.OUT disp_asc.out
asc_disp.OUT: ${.CURDIR}/../tools/mkastods/${DS_OBJ_DIR}/mkastods
	${.CURDIR}/../tools/mkastods/${DS_OBJ_DIR}/mkastods > ${.TARGET}
	/bin/rm -f asc_disp.out
	ln -s asc_disp.OUT asc_disp.out

disp_asc.OUT: ${.CURDIR}/../tools/mkdstoas/${AS_OBJ_DIR}/mkdstoas
	${.CURDIR}/../tools/mkdstoas/${AS_OBJ_DIR}/mkdstoas > ${.TARGET}
	/bin/rm -f disp_asc.out
	ln -s disp_asc.OUT disp_asc.out

inbound.o: kbd.OUT
CLEANFILES += kbd.OUT kbd.out TMPfunc.out
kbd.OUT: ${.CURDIR}/../ctlr/hostctlr.h ${.CURDIR}/../ctlr/${KBD} \
	 ${.CURDIR}/../tools/mkhits/${HITS_OBJ_DIR}/mkhits
	${CC} ${CFLAGS} -E ${.CURDIR}/../ctlr/function.c > TMPfunc.out
	${.CURDIR}/../tools/mkhits/${HITS_OBJ_DIR}/mkhits ${.CURDIR}/../ctlr/hostctlr.h \
		TMPfunc.out < ${.CURDIR}/../ctlr/${KBD} > ${.TARGET}
	/bin/rm -f kbd.out
	ln -s kbd.OUT kbd.out

# astosc.out
# asc_disp.out disp_asc.out
# default.map
# kbd.out

${.CURDIR}/../tools/mkastosc/${SC_OBJ_DIR}/mkastosc:
	cd ${.CURDIR}/../tools/mkastosc; make
${.CURDIR}/../tools/mkastods/${DS_OBJ_DIR}/mkastods:
	cd ${.CURDIR}/../tools/mkastods; make
${.CURDIR}/../tools/mkdstoas/${AS_OBJ_DIR}/mkdstoas:
	cd ${.CURDIR}/../tools/mkdstoas; make
${.CURDIR}/../tools/mkhits/${HITS_OBJ_DIR}/mkhits:
	cd ${.CURDIR}/../tools/mkhits; make


depend: .depend
.depend: ${DEPSRCS}
	mkdep ${MKDEP} ${CFLAGS:M-[ID]*} ${.ALLSRC:M*.c}

.include <bsd.prog.mk>

.PATH: ${.CURDIR}/../api ${.CURDIR}/../ascii ${.CURDIR}/../ctlr
.PATH: ${.CURDIR}/../general ${.CURDIR}/../sys_curses ${.CURDIR}/../../telnet


