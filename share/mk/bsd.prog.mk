#	from: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
#	$Id: bsd.prog.mk,v 1.28 1996/03/09 23:48:55 wosch Exp $

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S

CFLAGS+=${COPTS} ${DEBUG_FLAGS}
.if defined(DESTDIR)
CFLAGS+= -I${DESTDIR}/usr/include
CXXINCLUDES+= -I${DESTDIR}/usr/include/${CXX}
.endif

.if !defined(DEBUG_FLAGS)
STRIP?=	-s
.endif

BINGRP?=	bin
BINOWN?=	bin
BINMODE?=	555

LIBCRT0?=	${DESTDIR}/usr/lib/crt0.o
LIBKZHEAD?=	${DESTDIR}/usr/lib/kzhead.o
LIBKZTAIL?=	${DESTDIR}/usr/lib/kztail.o

LIBC?=		${DESTDIR}/usr/lib/libc.a
LIBC_PIC=	${DESTDIR}/usr/lib/libc_pic.a
LIBCOM_ERR=	${DESTDIR}/usr/lib/libcom_err.a
LIBCOMPAT?=	${DESTDIR}/usr/lib/libcompat.a
LIBCRYPT?=	${DESTDIR}/usr/lib/libcrypt.a
LIBCURSES?=	${DESTDIR}/usr/lib/libcurses.a
LIBDES?=	${DESTDIR}/usr/lib/libdes.a	# XXX doesn't exist
LIBDIALOG?=	${DESTDIR}/usr/lib/libdialog.a
LIBDISK?=	${DESTDIR}/usr/lib/libdisk.a
LIBEDIT?=	${DESTDIR}/usr/lib/libedit.a
LIBF2C?=	${DESTDIR}/usr/lib/libf2c.a
LIBFL?=		"don't use LIBFL, use LIBL"
LIBFORMS?=	${DESTDIR}/usr/lib/libforms.a
LIBGPLUSPLUS?=	${DESTDIR}/usr/lib/libg++.a
LIBGCC?=	${DESTDIR}/usr/lib/libgcc.a
LIBGCC_PIC?=	${DESTDIR}/usr/lib/libgcc_pic.a
LIBGMP?=	${DESTDIR}/usr/lib/libgmp.a
LIBGNUREGEX?=	${DESTDIR}/usr/lib/libgnuregex.a
LIBIPX?=	${DESTDIR}/usr/lib/libipx.a
LIBKDB?=	${DESTDIR}/usr/lib/libkdb.a	# XXX doesn't exist
LIBKRB?=	${DESTDIR}/usr/lib/libkrb.a	# XXX doesn't exist
LIBKEYCAP?=	${DESTDIR}/usr/lib/libkeycap.a
LIBKVM?=	${DESTDIR}/usr/lib/libkvm.a
LIBL?=		${DESTDIR}/usr/lib/libl.a
LIBLN?=		"don't use, LIBLN, use LIBL"
LIBM?=		${DESTDIR}/usr/lib/libm.a
LIBMD?=		${DESTDIR}/usr/lib/libmd.a
LIBMP?=		${DESTDIR}/usr/lib/libmp.a
LIBMYTINFO?=	${DESTDIR}/usr/lib/libmytinfo.a
LIBNCURSES?=	${DESTDIR}/usr/lib/libncurses.a
LIBPC?=		${DESTDIR}/usr/lib/libpc.a	# XXX doesn't exist
LIBPCAP?=	${DESTDIR}/usr/lib/libpcap.a
LIBPLOT?=	${DESTDIR}/usr/lib/libplot.a	# XXX doesn't exist
LIBREADLINE?=	${DESTDIR}/usr/lib/libreadline.a
LIBRESOLV?=	${DESTDIR}/usr/lib/libresolv.a
LIBRPCSVC?=	${DESTDIR}/usr/lib/librpcsvc.a
LIBSCRYPT?=	"don't use LIBSCRYPT, use LIBCRYPT"
LIBSCSI?=	${DESTDIR}/usr/lib/libscsi.a
LIBSKEY?=	${DESTDIR}/usr/lib/libskey.a
LIBSS?=		${DESTDIR}/usr/lib/libss.a
LIBTELNET?=	${DESTDIR}/usr/lib/libtelnet.a
LIBTERMCAP?=	${DESTDIR}/usr/lib/libtermcap.a
LIBTERMLIB?=	"don't use LIBTERMLIB, use LIBTERMCAP"
LIBUTIL?=	${DESTDIR}/usr/lib/libutil.a
LIBXPG4?=	${DESTDIR}/usr/lib/libxpg4.a
LIBY?=		${DESTDIR}/usr/lib/liby.a

.if defined(NOSHARED)
LDFLAGS+= -static
.endif

.if defined(DESTDIR)
LDDESTDIR+=	-L${DESTDIR}/usr/lib
.endif

.if defined(PROG)
.if defined(SRCS)

DPSRCS+= ${SRCS:M*.h}
.if !defined(NOOBJ)
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}
.endif

.if defined(LDONLY)

${PROG}: ${LIBCRT0} ${LIBC} ${DPSRCS} ${OBJS} ${DPADD} 
	${LD} ${LDFLAGS} -o ${.TARGET} ${LIBCRT0} ${OBJS} ${LIBC} ${LDDESTDIR} \
		${LDADD}

.else defined(LDONLY)

${PROG}: ${DPSRCS} ${OBJS} ${LIBC} ${DPADD}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDDESTDIR} ${LDADD}

.endif

.else !defined(SRCS)

SRCS=	${PROG}.c

.if 0
${PROG}: ${DPSRCS} ${SRCS} ${LIBC} ${DPADD}
	${CC} ${LDFLAGS} ${CFLAGS} -o ${.TARGET} ${.CURDIR}/${SRCS} \
		${LDDESTDIR} ${LDADD}

MKDEP=	-p
.else
# Always make an intermediate object file because:
# - it saves time rebuilding when only the library has changed
# - the name of the object gets put into the executable symbol table instead of
#   the name of a variable temporary object.
# - it's useful to keep objects around for crunching.
OBJS=	${PROG}.o
${PROG}: ${DPSRCS} ${OBJS} ${LIBC} ${DPADD}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDDESTDIR} ${LDADD}
.endif

.endif

.if	!defined(MAN1) && !defined(MAN2) && !defined(MAN3) && \
	!defined(MAN4) && !defined(MAN5) && !defined(MAN6) && \
	!defined(MAN7) && !defined(MAN8) && !defined(NOMAN)
MAN1=	${PROG}.1
.endif
.endif

_PROGSUBDIR: .USE
.if defined(SUBDIR) && !empty(SUBDIR)
	@for entry in ${SUBDIR}; do \
		(${ECHODIR} "===> ${DIRPRFX}$$entry"; \
		if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			cd ${.CURDIR}/$${entry}.${MACHINE}; \
		else \
			cd ${.CURDIR}/$${entry}; \
		fi; \
		${MAKE} ${.TARGET:S/realinstall/install/:S/.depend/depend/} DIRPRFX=${DIRPRFX}$$entry/); \
	done
.endif

# XXX I think MANDEPEND is only used for groff.  It should be named more
# generally and perhaps not be in the maninstall dependencies now it is
# here (or does maninstall always work when nothing is made?),

.MAIN: all
all: ${PROG} all-man _PROGSUBDIR

.if !target(clean)
clean: _PROGSUBDIR
	rm -f a.out Errs errs mklog ${PROG} ${OBJS} ${CLEANFILES} 
.endif

.if !target(cleandir)
cleandir: _PROGSUBDIR
	rm -f a.out Errs errs mklog ${PROG} ${OBJS} ${CLEANFILES}
	rm -f ${.CURDIR}/tags .depend
	cd ${.CURDIR}; rm -rf obj;
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

realinstall: _PROGSUBDIR
.if defined(PROG)
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${INSTALLFLAGS} ${PROG} ${DESTDIR}${BINDIR}
.endif
.if defined(HIDEGAME)
	(cd ${DESTDIR}/usr/games; rm -f ${PROG}; ln -s dm ${PROG}; \
	    chown games.bin ${PROG})
.endif
.if defined(LINKS) && !empty(LINKS)
	@set ${LINKS}; \
	while test $$# -ge 2; do \
		l=${DESTDIR}$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		${ECHO} $$t -\> $$l; \
		rm -f $$t; \
		ln ${LN_FLAGS} $$l $$t; \
	done; true
.endif

install: afterinstall
.if !defined(NOMAN)
afterinstall: realinstall maninstall
.else
afterinstall: realinstall
.endif
realinstall: beforeinstall
.endif

DISTRIBUTION?=	bin
.if !target(distribute)
distribute:
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${DISTRIBUTION} SHARED=copies
.endif

.if !target(lint)
lint: ${SRCS} _PROGSUBDIR
.if defined(PROG)
	@${LINT} ${LINTFLAGS} ${CFLAGS} ${.ALLSRC} | more 2>&1
.endif
.endif

.if !target(obj)
.if defined(NOOBJ)
obj: _PROGSUBDIR
.else
obj: _PROGSUBDIR
	@cd ${.CURDIR}; rm -rf obj; \
	here=`pwd`; dest=/usr/obj`echo $$here | sed 's,^/usr/src,,'`; \
	${ECHO} "$$here -> $$dest"; ln -s $$dest obj; \
	if test -d /usr/obj -a ! -d $$dest; then \
		mkdir -p $$dest; \
	else \
		true; \
	fi;
.endif
.endif

.if !target(tags)
tags: ${SRCS} _PROGSUBDIR
.if defined(PROG)
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC} | \
	    sed "s;\${.CURDIR}/;;" > tags
.endif
.endif

.if !defined(NOMAN)
.include <bsd.man.mk>
.elif !target(maninstall)
maninstall:
all-man:
.endif

_DEPSUBDIR=	_PROGSUBDIR
.include <bsd.dep.mk>
