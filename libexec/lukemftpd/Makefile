#	@(#)Makefile	8.2 (Berkeley) 4/4/94
# $FreeBSD$

MAINTAINER=	obrien

LUKEMFTPD=	${.CURDIR}/../../contrib/lukemftpd
.PATH: ${LUKEMFTPD}/src

PROG=	lukemftpd
MAN=	lukemftpd.8 ftpd.conf.5 ftpusers.5
SRCS=	cmds.c conf.c ftpd.c ftpcmd.y popen.c

WFORMAT= 0
CFLAGS+= -I${.CURDIR} -I${LUKEMFTPD} -I${LUKEMFTPD}/src
YFLAGS=

LDADD=	-lmd -lcrypt -lutil
DPADD=	${LIBMD} ${LIBCRYPT} ${LIBUTIL}

# XXX Kluge! Conversation mechanism needs to be fixed.
LDADD+=	-lopie
DPADD+=	${LIBOPIE}

LSDIR=	../../bin/ls
.PATH: ${.CURDIR}/${LSDIR}
SRCS+=	ls-unmain.c cmp.c lomac.c print.c util.c
CFLAGS+=-I${.CURDIR}/${LSDIR}
DPADD+=	${LIBM}
LDADD+=	-lm

CLEANFILES+=	ls-unmain.c
ls-unmain.c: ls.c
	sed -e 's/^main(/ls_main(/g' -e 's,extern.h,${LSDIR}/extern.h,' \
	    ${.ALLSRC} > ${.TARGET}

.if !defined(NOPAM)
CFLAGS+= -DUSE_PAM
DPADD+=	${LIBPAM}
LDADD+=	${MINUSLPAM}
.endif

ftpd.o ftpcmd.o: version.h

lukemftpd.8: ftpd.8
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
