#	from: @(#)sys.mk	8.2 (Berkeley) 3/21/94
# $FreeBSD$

unix		?=	We run FreeBSD, not UNIX.

# If the special target .POSIX appears (without prerequisites or
# commands) before the first noncomment line in the makefile, make shall
# process the makefile as specified by the Posix 1003.2 specification.
# make(1) sets the special macro %POSIX in this case (to the actual
# value "1003.2", for what it's worth).
#
# The rules below use this macro to distinguish between Posix-compliant
# and default behaviour.

.if defined(%POSIX)
.SUFFIXES:	.o .c .y .l .a .sh .f
.else
.SUFFIXES:	.out .a .ln .o .c .cc .cpp .cxx .C .m .F .f .e .r .y .l .S .s .cl .p .h .sh
.endif

.LIBS:		.a

X11BASE		?=	/usr/X11R6

AR		?=	ar
.if defined(%POSIX)
ARFLAGS		?=	-rv
.else
ARFLAGS		?=	rl
.endif
RANLIB		?=	ranlib

AS		?=	as
AFLAGS		?=

.if defined(%POSIX)
CC		?=	c89
.else
CC		?=	cc
.endif
CFLAGS		?=	-O -pipe

CXX		?=	c++
CXXFLAGS	?=	${CXXINCLUDES} ${CFLAGS}

CPP		?=	cpp

.if ${.MAKEFLAGS:M-s} == ""
ECHO		?=	echo
ECHODIR		?=	echo
.else
ECHO		?=	true
.if ${.MAKEFLAGS:M-s} == "-s"
ECHODIR		?=	echo
.else
ECHODIR		?=	true
.endif
.endif

.if defined(%POSIX)
FC		?=	fort77
FFLAGS		?=	-O 1
.else
FC		?=	f77
FFLAGS		?=	-O
.endif
EFLAGS		?=

INSTALL		?=	install

LEX		?=	lex
LFLAGS		?=

LD		?=	ld
LDFLAGS		?=

LINT		?=	lint
LINTFLAGS	?=	-chapbx

MAKE		?=	make

OBJC		?=	cc
OBJCFLAGS	?=	${OBJCINCLUDES} ${CFLAGS} -Wno-import

PC		?=	pc
PFLAGS		?=

RC		?=	f77
RFLAGS		?=

SHELL		?=	sh

YACC		?=	yacc
.if defined(%POSIX)
YFLAGS		?=
.else
YFLAGS		?=	-d
.endif

# FreeBSD/i386 has traditionally been built with a version of make
# which knows MACHINE, but not MACHINE_ARCH. When building on other
# architectures, assume that the version of make being used has an
# explicit MACHINE_ARCH setting and treat a missing MACHINE_ARCH
# as an i386 architecture.
MACHINE_ARCH	?=	i386

.if defined(%POSIX)
# Posix 1003.2 mandated rules
#
# Quoted directly from the Posix 1003.2 draft, only the macros
# $@, $< and $* have been replaced by ${.TARGET}, ${.IMPSRC}, and
# ${.PREFIX}, resp.

# SINGLE SUFFIX RULES
.c:
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.IMPSRC}

.f:
	${FC} ${FFLAGS} ${LDFLAGS} -o ${.TARGET} ${.IMPSRC}

.sh:
	cp ${.IMPSRC} ${.TARGET}
	chmod a+x ${.TARGET}

# DOUBLE SUFFIX RULES

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}

.f.o:
	${FC} ${FFLAGS} -c ${.IMPSRC}

.y.o:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c y.tab.c
	rm -f y.tab.c
	mv y.tab.o ${.TARGET}

.l.o:
	${LEX} ${LFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c lex.yy.c
	rm -f lex.yy.c
	mv lex.yy.o ${.TARGET}

.y.c:
	${YACC} ${YFLAGS} ${.IMPSRC}
	mv y.tab.c ${.TARGET}

.l.c:
	${LEX} ${LFLAGS} ${.IMPSRC}
	mv lex.yy.c ${.TARGET}

.c.a:
	${CC} ${CFLAGS} -c ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o

.f.a:
	${FC} ${FFLAGS} -c ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o

.else

# non-Posix rule set

.sh:
	cp -p ${.IMPSRC} ${.TARGET}
	chmod a+x ${.TARGET}

.c:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}

.cc .cpp .cxx .C:
	${CXX} ${CXXFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}

.cc.o .cpp.o .cxx.o .C.o:
	${CXX} ${CXXFLAGS} -c ${.IMPSRC}

.m.o:
	${OBJC} ${OBJCFLAGS} -c ${.IMPSRC}

.p.o:
	${PC} ${PFLAGS} -c ${.IMPSRC}

.e .r .F .f:
	${FC} ${RFLAGS} ${EFLAGS} ${FFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} \
	    -o ${.TARGET}

.e.o .r.o .F.o .f.o:
	${FC} ${RFLAGS} ${EFLAGS} ${FFLAGS} -c ${.IMPSRC}

.S.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}

.s.o:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}

# XXX not -j safe
.y.o:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c y.tab.c -o ${.TARGET}
	rm -f y.tab.c

.l.o:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.PREFIX}.tmp.c
	${CC} ${CFLAGS} -c ${.PREFIX}.tmp.c -o ${.TARGET}
	rm -f ${.PREFIX}.tmp.c

# XXX not -j safe
.y.c:
	${YACC} ${YFLAGS} ${.IMPSRC}
	mv y.tab.c ${.TARGET}

.l.c:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.TARGET}

.s.out .c.out .o.out:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}

.f.out .F.out .r.out .e.out:
	${FC} ${EFLAGS} ${RFLAGS} ${FFLAGS} ${LDFLAGS} ${.IMPSRC} \
	    ${LDLIBS} -o ${.TARGET}
	rm -f ${.PREFIX}.o

# XXX not -j safe
.y.out:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} ${LDFLAGS} y.tab.c ${LDLIBS} -ly -o ${.TARGET}
	rm -f y.tab.c

.l.out:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.PREFIX}.tmp.c
	${CC} ${CFLAGS} ${LDFLAGS} ${.PREFIX}.tmp.c ${LDLIBS} -ll -o ${.TARGET}
	rm -f ${.PREFIX}.tmp.c

.endif

.if exists(/etc/defaults/make.conf)
.include </etc/defaults/make.conf>
.endif

__MAKE_CONF?=/etc/make.conf
.if exists(${__MAKE_CONF})
.include "${__MAKE_CONF}"
.endif

.include <bsd.cpu.mk>

.if exists(/etc/make.conf.local)
.error Error, original /etc/make.conf should be moved to the /etc/defaults/ directory and /etc/make.conf.local should be renamed to /etc/make.conf.
.include </etc/make.conf.local>
.endif

#
# The build tools are indirected by /usr/bin/objformat which determines the
# object format from the OBJFORMAT environment variable and if this is not
# defined, it reads /etc/objformat.
#
.if exists(/etc/objformat) && !defined(OBJFORMAT)
.include "/etc/objformat"
.endif

# Default executable format
OBJFORMAT?=	elf
