#	@(#)Makefile	8.2 (Berkeley) 1/25/94
# $FreeBSD$

PROG=	mail
SRCS=	version.c aux.c cmd1.c cmd2.c cmd3.c cmdtab.c collect.c edit.c fio.c \
	getname.c head.c v7.local.c lex.c list.c main.c names.c popen.c \
	quit.c send.c strings.c temp.c tty.c vars.c
WARNS?=	0
FILES=	mail.help mail.tildehelp
FILESDIR=	${SHAREDIR}/misc
EFILES=	mail.rc
LINKS=	${BINDIR}/mail ${BINDIR}/Mail ${BINDIR}/mail ${BINDIR}/mailx
MLINKS=	mail.1 Mail.1 mail.1 mailx.1

.PATH: ${.CURDIR}/misc

etc-mailrc:
	cd ${.CURDIR}/misc; ${INSTALL} -c -o root -g wheel \
	    -m 644 ${EFILES} ${DESTDIR}/etc

.include <bsd.prog.mk>
