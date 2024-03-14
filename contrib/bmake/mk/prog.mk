#	$Id: prog.mk,v 1.44 2024/02/19 00:06:19 sjg Exp $

# should be set properly in sys.mk
_this ?= ${.PARSEFILE:S,bsd.,,}

.if !target(__${_this}__)
__${_this}__: .NOTMAIN

.include <init.mk>

# FreeBSD at least expects MAN8 etc.
.if defined(MAN) && !empty(MAN)
_sect:=${MAN:E}
MAN${_sect}=${MAN}
.endif

.SUFFIXES: .out .o .c ${CXX_SUFFIXES} .y .l ${CCM_SUFFIXES} ${PCM}

CFLAGS+=	${COPTS}

.if ${TARGET_OSNAME} == "NetBSD"
.if ${MACHINE_ARCH} == "sparc64"
CFLAGS+=	-mcmodel=medlow
.endif

# ELF platforms depend on crtbegin.o and crtend.o
.if ${OBJECT_FMT} == "ELF"
.ifndef LIBCRTBEGIN
LIBCRTBEGIN=	${DESTDIR}/usr/lib/crtbegin.o
.MADE: ${LIBCRTBEGIN}
.endif
.ifndef LIBCRTEND
LIBCRTEND=	${DESTDIR}/usr/lib/crtend.o
.MADE: ${LIBCRTEND}
.endif
_SHLINKER=	${SHLINKDIR}/ld.elf_so
.else
LIBCRTBEGIN?=
LIBCRTEND?=
_SHLINKER=	${SHLINKDIR}/ld.so
.endif

.ifndef LIBCRT0
LIBCRT0=	${DESTDIR}/usr/lib/crt0.o
.MADE: ${LIBCRT0}
.endif
.endif	# NetBSD

# here is where you can define what LIB* are
.-include <libnames.mk>
.if ${MK_DPADD_MK} == "yes"
# lots of cool magic, but might not suit everyone.
.include <dpadd.mk>
.endif

.if ${MK_GPROF} == "yes"
CFLAGS+= ${CC_PG} ${PROFFLAGS}
LDADD+= ${CC_PG}
.if ${MK_DPADD_MK} == "no"
LDADD_LIBC_P?= -lc_p
LDADD_LAST+= ${LDADD_LIBC_P}
.endif
.endif

.if defined(SHAREDSTRINGS)
CLEANFILES+=strings
.c.o:
	@${COMPILE.c:N-c} -E ${.IMPSRC} | xstr -c -
	@${COMPILE.c} x.c -o ${.TARGET}
	@rm -f x.c

# precompiled C++ Modules
${CCM_SUFFIXES:%=%${PCM}}:
	@${COMIPILE.cc:N-c} -E ${.IMPSRC} | xstr -c -
	@mv -f x.c x.cc
	@${COMPILE.pcm} x.cc -o ${.TARGET}
	@rm -f x.cc

${CXX_SUFFIXES:N.c*m:%=%.o}:
	@${COMIPILE.cc:N-c} -E ${.IMPSRC} | xstr -c -
	@mv -f x.c x.cc
	@${COMPILE.cc} x.cc -o ${.TARGET}
	@rm -f x.cc
.endif

.if defined(PROG_CXX)
PROG=		${PROG_CXX}
_SUPCXX?=	-lstdc++ -lm
.endif

.if defined(PROG)
BINDIR ?= ${prefix}/bin

.if empty(SRCS)
# init.mk handling of QUALIFIED_VAR_LIST means
# SRCS will be defined - even if empty.
SRCS = ${PROG}.c
.endif

SRCS ?=	${PROG}.c
OBJS_SRCS = ${SRCS:${OBJS_SRCS_FILTER}}
.for s in ${OBJS_SRCS:M*/*}
${.o .po .lo:L:@o@${s:T:R}$o@}: $s
.endfor
.if !empty(OBJS_SRCS)
OBJS+=	${OBJS_SRCS:T:R:S/$/.o/g}
LOBJS+=	${LSRCS:.c=.ln} ${SRCS:M*.c:.c=.ln}
.endif

.if defined(OBJS) && !empty(OBJS)
.NOPATH: ${OBJS} ${PROG} ${SRCS:M*.[ly]:C/\..$/.c/} ${YHEADER:D${SRCS:M*.y:.y=.h}}

# this is known to work for NetBSD 1.6 and FreeBSD 4.2
.if ${TARGET_OSNAME:NFreeBSD:NNetBSD} == ""
_PROGLDOPTS=
.if ${SHLINKDIR} != "/usr/libexec"	# XXX: change or remove if ld.so moves
_PROGLDOPTS+=	-Wl,-dynamic-linker=${_SHLINKER}
.endif
.if defined(LIBDIR) && ${SHLIBDIR} != ${LIBDIR}
_PROGLDOPTS+=	-Wl,-rpath-link,${DESTDIR}${SHLIBDIR}:${DESTDIR}/usr/lib \
		-L${DESTDIR}${SHLIBDIR}
.endif
_PROGLDOPTS+=	-Wl,-rpath,${SHLIBDIR}:/usr/lib
.endif	# NetBSD

.if ${MK_PROG_LDORDER_MK} != "no"
${PROG}: ldorder

.include <ldorder.mk>
.endif
# avoid -dL errors
LDADD_LDORDER ?=
LDSTATIC ?=

.if defined(DESTDIR) && exists(${LIBCRT0}) && ${LIBCRT0} != "/dev/null"

${PROG}: ${LIBCRT0} ${OBJS} ${LIBC} ${DPADD}
	${_CCLINK} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} -nostdlib ${_PROGLDOPTS} -L${DESTDIR}/usr/lib ${LIBCRT0} ${LIBCRTBEGIN} ${OBJS} ${LDADD_LDORDER} ${LDADD} -L${DESTDIR}/usr/lib ${_SUPCXX} -lgcc -lc -lgcc ${LIBCRTEND}

.else

${PROG}: ${LIBCRT0} ${OBJS} ${LIBC} ${DPADD}
	${_CCLINK} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} ${_PROGLDOPTS} ${OBJS} ${LDADD_LDORDER} ${LDADD}

.endif	# defined(DESTDIR)
.endif	# defined(OBJS) && !empty(OBJS)

.if	!defined(MAN)
MAN=	${PROG}.1
.endif	# !defined(MAN)
.endif	# defined(PROG)

.if !defined(_SKIP_BUILD)
realbuild: ${PROG}
.endif

all: _SUBDIRUSE

.if !target(clean)
cleanprog:
	rm -f a.out [Ee]rrs mklog core *.core \
	    ${PROG} ${OBJS} ${LOBJS} ${CLEANFILES}

clean: _SUBDIRUSE cleanprog
cleandir: _SUBDIRUSE cleanprog
.else
cleandir: _SUBDIRUSE clean
.endif

.if defined(SRCS) && (!defined(MKDEP) || ${MKDEP} != autodep)
afterdepend: .depend
	@(TMP=/tmp/_depend$$$$; \
	    sed -e 's/^\([^\.]*\).o[ ]*:/\1.o \1.ln:/' \
	      < .depend > $$TMP; \
	    mv $$TMP .depend)
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

.if !empty(BINOWN)
PROG_INSTALL_OWN ?= -o ${BINOWN} -g ${BINGRP}
.endif

.if !target(realinstall)
realinstall: proginstall
.endif
.if !target(proginstall)
proginstall:
.if defined(PROG)
	[ -d ${DESTDIR}${BINDIR} ] || \
	${INSTALL} -d ${PROG_INSTALL_OWN} -m 775 ${DESTDIR}${BINDIR}
	${INSTALL} ${COPY} ${STRIP_FLAG} ${PROG_INSTALL_OWN} -m ${BINMODE} \
	    ${PROG} ${DESTDIR}${BINDIR}/${PROG_NAME}
.endif
.if defined(HIDEGAME)
	(cd ${DESTDIR}/usr/games; rm -f ${PROG}; ln -s dm ${PROG})
.endif
.endif

.include <links.mk>

install: maninstall install_links _SUBDIRUSE

install_links:
.if !empty(SYMLINKS)
	@set ${SYMLINKS}; ${_SYMLINKS_SCRIPT}
.endif
.if !empty(LINKS)
	@set ${LINKS}; ${_LINKS_SCRIPT}
.endif

maninstall: afterinstall
afterinstall: realinstall
install_links: realinstall
proginstall: beforeinstall
realinstall: beforeinstall
.endif

.if !target(lint)
lint: ${LOBJS}
.if defined(LOBJS) && !empty(LOBJS)
	@${LINT} ${LINTFLAGS} ${LDFLAGS:M-L*} ${LOBJS} ${LDADD}
.endif
.endif

.NOPATH:	${PROG}
.if defined(OBJS) && !empty(OBJS)
.NOPATH:	${OBJS}
.endif

.if defined(FILES) || defined(FILESGROUPS)
.include <files.mk>
.endif

.if ${MK_MAN} != "no"
.include <man.mk>
.endif

.if ${MK_NLS} != "no"
.include <nls.mk>
.endif

.include <obj.mk>
.include <dep.mk>
.include <subdir.mk>

.if !empty(PROG) && ${MK_STAGING_PROG} == "yes"
STAGE_BINDIR ?= ${STAGE_OBJTOP}${BINDIR}
STAGE_DIR.prog ?= ${STAGE_BINDIR}
.if ${PROG_NAME:U${PROG}} != ${PROG}
STAGE_AS_SETS += prog
STAGE_AS_${PROG} = ${PROG_NAME}
stage_as.prog: ${PROG}
.else
STAGE_SETS += prog
stage_files.prog: ${PROG}
.endif
.endif

.include <final.mk>

.endif
