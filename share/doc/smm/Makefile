#	from: @(#)Makefile	5.2.1.2 (Berkeley) 5/7/91
#	Makefile,v 1.3 1993/07/31 15:32:55 mycroft Exp

# Missing: 03.kdebug 09.uucpimpl 12.uchanges 13.kchanges 17.security
#	   18.password 19.porttour 21.uucpnet
BINDIR=	/usr/share/doc/smm
FILES=	00.contents Makefile Title
SUBDIR=	02.config 04.quotas 05.fsck 06.lpd 07.sendmailop \
	08.timedop 10.newsop 11.named \
	14.fastfs 15.net 16.sendmail \
	20.termdesc 22.timed

.if	${MACHINE} == "tahoe"
SUBDIR+=01.setup
.elif	${MACHINE} == "vax"
SUBDIR+=01.setup
.endif

beforeinstall:
	@if [ ! -d ${DESTDIR}${BINDIR} ]; then \
                /bin/rm -f ${DESTDIR}${BINDIR} ; \
                mkdir -p ${DESTDIR}${BINDIR} ; \
                chown root.wheel ${DESTDIR}${BINDIR} ; \
                chmod 755 ${DESTDIR}${BINDIR} ; \
        else \
                true ; \
        fi
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}

.include <bsd.subdir.mk>
