#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$
#

PROG=	more
CFLAGS+=-I${.CURDIR} -I${.OBJDIR} -DTERMIOS
SRCS=	ch.c command.c defrc.h help.c input.c line.c linenum.c macro.c main.c \
	ncommand.c option.c os.c output.c position.c prim.c screen.c signal.c \
	tags.c ttyin.c
DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap

CLEANFILES+=	defrc.h

EXAMPDIR=	/usr/share/examples/more
EXAMPLES=	default.morerc less.morerc most.morerc

defrc.h: default.morerc
	@echo '/* ${.TARGET:T} auto-generated from ${.ALLSRC:T} */' \
	    > ${.TARGET}
	@echo '#define DEFRC "\' >> ${.TARGET}
	sed -e 's/\\/\\\\/g' -e 's/\"/\\\"/g' -e 's/$$/\\n\\/' \
	    < ${.ALLSRC} >> ${.TARGET}
	@echo \" >> ${.TARGET}

beforeinstall:
	${INSTALL} ${COPY} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
	    ${.CURDIR}/more.help ${DESTDIR}/usr/share/misc
.for xzamp in ${EXAMPLES}
	${INSTALL} ${COPY} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
	    ${.CURDIR}/${xzamp} ${DESTDIR}${EXAMPDIR}/${xzamp}
.endfor

.include <bsd.prog.mk>
