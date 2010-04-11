#	from: @(#)sys.mk	8.2 (Berkeley) 3/21/94
# $FreeBSD$

unix		?=	We run FreeBSD, not UNIX.
.FreeBSD	?=	true

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
.SUFFIXES:	.out .a .ln .o .c .cc .cpp .cxx .C .m .F .f .e .r .y .l .S .asm .s .cl .p .h .sh
.endif

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
CFLAGS		?=	-O
.else
CC		?=	clang
.if ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "mips"
CFLAGS		?=	-O -pipe
.else
CFLAGS		?=	-O2 -pipe
.endif
.if defined(NO_STRICT_ALIASING)
CFLAGS		+=	-fno-strict-aliasing
.endif
.endif
PO_CFLAGS	?=	${CFLAGS}

# Turn CTF conversion off by default for now. This default could be
# changed later if DTrace becomes popular.
.if !defined(WITH_CTF)
NO_CTF		=	1
.endif

# C Type Format data is required for DTrace
CTFFLAGS	?=	-L VERSION

CTFCONVERT	?=	ctfconvert
CTFMERGE	?=	ctfmerge
.if defined(CFLAGS) && (${CFLAGS:M-g} != "")
CTFFLAGS	+=	-g
.else
# XXX: What to do here? Is removing the CFLAGS part completely ok here?
# For now comment it out to not compile with -g unconditionally.
#CFLAGS		+=	-g
.endif

CXX		?=	clang++
CXXFLAGS	?=	${CFLAGS:N-std=*:N-Wnested-externs:N-W*-prototypes:N-Wno-pointer-sign:N-Wold-style-definition}
PO_CXXFLAGS	?=	${CXXFLAGS}

CPP		?=	cpp

.if empty(.MAKEFLAGS:M-s)
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

.if !empty(.MAKEFLAGS:M-n) && ${.MAKEFLAGS:M-n} == "-n"
_+_		?=
.else
_+_		?=	+
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
LINTFLAGS	?=	-cghapbx
LINTKERNFLAGS	?=	${LINTFLAGS}
LINTOBJFLAGS	?=	-cghapbxu -i
LINTOBJKERNFLAGS?=	${LINTOBJFLAGS}
LINTLIBFLAGS	?=	-cghapbxu -C ${LIB}

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

.if defined(%POSIX)

# Posix 1003.2 mandated rules
#
# Quoted directly from the Posix 1003.2 draft, only the macros
# $@, $< and $* have been replaced by ${.TARGET}, ${.IMPSRC}, and
# ${.PREFIX}, resp.

# SINGLE SUFFIX RULES
.c:
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.f:
	${FC} ${FFLAGS} ${LDFLAGS} -o ${.TARGET} ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.sh:
	cp ${.IMPSRC} ${.TARGET}
	chmod a+x ${.TARGET}

# DOUBLE SUFFIX RULES

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.f.o:
	${FC} ${FFLAGS} -c ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.y.o:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c y.tab.c
	rm -f y.tab.c
	mv y.tab.o ${.TARGET}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.l.o:
	${LEX} ${LFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c lex.yy.c
	rm -f lex.yy.c
	mv lex.yy.o ${.TARGET}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

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

.c.ln:
	${LINT} ${LINTOBJFLAGS} ${CFLAGS:M-[DIU]*} ${.IMPSRC} || \
	    touch ${.TARGET}

.cc.ln .C.ln .cpp.ln .cxx.ln:
	${LINT} ${LINTOBJFLAGS} ${CXXFLAGS:M-[DIU]*} ${.IMPSRC} || \
	    touch ${.TARGET}

.c:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.cc .cpp .cxx .C:
	${CXX} ${CXXFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}

.cc.o .cpp.o .cxx.o .C.o:
	${CXX} ${CXXFLAGS} -c ${.IMPSRC}

.m.o:
	${OBJC} ${OBJCFLAGS} -c ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.p.o:
	${PC} ${PFLAGS} -c ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.e .r .F .f:
	${FC} ${RFLAGS} ${EFLAGS} ${FFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} \
	    -o ${.TARGET}

.e.o .r.o .F.o .f.o:
	${FC} ${RFLAGS} ${EFLAGS} ${FFLAGS} -c ${.IMPSRC}

.S.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.asm.o:
	${CC} -x assembler-with-cpp ${CFLAGS} -c ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.s.o:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

# XXX not -j safe
.y.o:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c y.tab.c -o ${.TARGET}
	rm -f y.tab.c
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.l.o:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.PREFIX}.tmp.c
	${CC} ${CFLAGS} -c ${.PREFIX}.tmp.c -o ${.TARGET}
	rm -f ${.PREFIX}.tmp.c
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

# XXX not -j safe
.y.c:
	${YACC} ${YFLAGS} ${.IMPSRC}
	mv y.tab.c ${.TARGET}

.l.c:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.TARGET}

.s.out .c.out .o.out:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.f.out .F.out .r.out .e.out:
	${FC} ${EFLAGS} ${RFLAGS} ${FFLAGS} ${LDFLAGS} ${.IMPSRC} \
	    ${LDLIBS} -o ${.TARGET}
	rm -f ${.PREFIX}.o
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

# XXX not -j safe
.y.out:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} ${LDFLAGS} y.tab.c ${LDLIBS} -ly -o ${.TARGET}
	rm -f y.tab.c
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

.l.out:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.PREFIX}.tmp.c
	${CC} ${CFLAGS} ${LDFLAGS} ${.PREFIX}.tmp.c ${LDLIBS} -ll -o ${.TARGET}
	rm -f ${.PREFIX}.tmp.c
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

# FreeBSD build pollution.  Hide it in the non-POSIX part of the ifdef.
__MAKE_CONF?=/etc/make.conf
.if exists(${__MAKE_CONF})
.include "${__MAKE_CONF}"
.endif

.if defined(__MAKE_SHELL) && !empty(__MAKE_SHELL)
SHELL=	${__MAKE_SHELL}
.SHELL: path=${__MAKE_SHELL}
.endif

# Default executable format
# XXX hint for bsd.port.mk
OBJFORMAT?=	elf

# Toggle on warnings
.WARN: dirsyntax

.endif

.include <bsd.compat.mk>
.include <bsd.cpu.mk>
