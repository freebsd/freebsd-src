# $Header: /a/cvs/386BSD/src/usr.bin/tn3270/mset/Makefile,v 1.3 1993/07/07 22:27:07 nate Exp $

.include <../../Makefile.inc>

CFLAGS += -I${.CURDIR} -I.

MAN1 = mset.1
MAN5 = map3270.5

SRCS += astosc.c map3270.c mset.c

# this and the dependency hacks below to make 'depend' target
# work right...

DEPSRCS += astosc.OUT map3270.c mset.c

PROG = mset

astosc.o: astosc.OUT
CLEANFILES += astosc.OUT astosc.out
.if exists(${.CURDIR}/../tools/mkastosc/obj)
OBJ_DIR=obj
.else
OBJ_DIR=.
.endif

astosc.OUT: ${.CURDIR}/../ctlr/hostctlr.h ${.CURDIR}/../ctlr/function.h \
	${.CURDIR}/../ctlr/${KBD} ${.CURDIR}/../tools/mkastosc/${OBJ_DIR}/mkastosc
	${.CURDIR}/../tools/mkastosc/${OBJ_DIR}/mkastosc \
	${.CURDIR}/../ctlr/hostctlr.h ${.CURDIR}/../ctlr/function.h \
		< ${.CURDIR}/../ctlr/${KBD} > ${.TARGET}
	/bin/rm -f astosc.out
	ln -s astosc.OUT astosc.out

# astosc.out

${.CURDIR}/../tools/mkastosc/${OBJ_DIR}/mkastosc:
	cd ${.CURDIR}/../tools/mkastosc; make


depend: .depend
.depend: ${DEPSRCS}
	mkdep ${MKDEP} ${CFLAGS:M-[ID]*} ${.ALLSRC:M*.c}

.include <bsd.prog.mk>

.PATH: ${.CURDIR}/../api ${.CURDIR}/../ascii
