#	from: @(#)Makefile	5.1.1.1 (Berkeley) 5/7/91
#	Makefile,v 1.5 1993/07/31 15:33:20 mycroft Exp

# Missing:  01.Clang 02.f77 03.f77io 05.as 09.lint 10.adb 11.dbx
#	    12.make 15.yacc 16.lex 17.m4
#
# Broken:   04.pascal 07.ipctut

BINDIR=	/usr/share/doc/ps1
SUBDIR=	Title 00.contents \
	06.sysman \
	08.ipc 13.rcs 14.sccs \
	18.curses

beforeinstall:
	@if [ ! -d ${DESTDIR}${BINDIR} ]; then \
                /bin/rm -f ${DESTDIR}${BINDIR} ; \
                mkdir -p ${DESTDIR}${BINDIR} ; \
                chown root.wheel ${DESTDIR}${BINDIR} ; \
                chmod 755 ${DESTDIR}${BINDIR} ; \
        else \
                true ; \
        fi

.include <bsd.subdir.mk>
