## Copyright (c) 1996-2000 by Internet Software Consortium
##
## Permission to use, copy, modify, and distribute this software for any
## purpose with or without fee is hereby granted, provided that the above
## copyright notice and this permission notice appear in all copies.
##
## THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
## ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
## OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
## CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
## DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
## PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
## ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
## SOFTWARE.

# $Id: Makefile,v 8.50 2000/07/11 06:41:32 vixie Exp $
# $FreeBSD$

DESTDIR=
CC= cc
SHELL= /bin/sh

CDEBUG= -g

#(net2 and its descendents)
SYSTYPE = bsdos
TOP = ../..
INCL =  ${TOP}/include
PORTINCL = ${TOP}/port/${SYSTYPE}/include
LIBBIND = ${TOP}/lib/libbind.a
A=a
O=o
EXE=
YACC = yacc -d
SYSLIBS = -ll -lutil
DESTBIN = /usr/local/bin
DESTSBIN = /usr/local/sbin
DESTEXEC = /usr/local/libexec
DESTMAN = /usr/share/man
DESTHELP= /usr/share/misc
DESTETC= /etc
DESTRUN= /var/run
AR= ar cru
INSTALL= install
STRIP=-s
INSTALL_EXEC=
INSTALL_LIB=-o bin -g bin

PS=ps
LDFLAGS=
CFLAGS=	${CDEBUG}
CPPFLAGS= -I${PORTINCL} -I${INCL} ${DEFS}

VER= LOCAL-`date +%Y%m%d.%H%M%S`
HOSTNAMECMD= hostname || uname -n

PROG=	named
HDRS=	db_defs.h db_glob.h ns_defs.h ns_glob.h named.h pathnames.h
SRCS=	db_dump.c db_load.c db_lookup.c db_save.c db_update.c \
	db_glue.c db_ixfr.c db_sec.c db_tsig.c \
	ns_parser.c ns_lexer.c ns_parseutil.c ns_ctl.c \
	ns_forw.c ns_init.c ns_main.c ns_maint.c ns_req.c \
	ns_resp.c ns_stats.c ns_ncache.c ns_xfr.c ns_glue.c \
	ns_udp.c ns_config.c ns_update.c ns_ixfr.c ns_signal.c \
	ns_sort.c ns_notify.c
OBJS=	db_dump.${O} db_load.${O} db_lookup.${O} db_save.${O} db_update.${O} \
	db_glue.${O} db_ixfr.${O} db_sec.${O} db_tsig.${O} \
	ns_parser.${O} ns_lexer.${O} ns_parseutil.${O} ns_ctl.${O} \
	ns_forw.${O} ns_init.${O} ns_main.${O} ns_maint.${O} ns_req.${O} \
	ns_resp.${O} ns_stats.${O} ns_ncache.${O} ns_xfr.${O} ns_glue.${O} \
	ns_udp.${O} ns_config.${O} ns_update.${O} ns_ixfr.${O} ns_signal.${O} \
	ns_sort.${O} ns_notify.${O}

all: ${PROG}${EXE}

${PROG}${EXE}: pathnames.h ${OBJS} ${LIBBIND} Makefile tmp_version.${O}
	${CC} ${CDEBUG} ${LDFLAGS} ${BOUNDS} -o ${PROG}${EXE} ${OBJS} \
		tmp_version.${O} ${LIBBIND} ${SYSLIBS}

ns_parser.c ns_parser.h: ns_parser.y
	${YACC} ns_parser.y
	mv y.tab.c ns_parser.c
	mv y.tab.h ns_parser.h

tmp_version.${O}: tmp_version.c

tmp_version.c: version.c Makefile ../Makefile ${SRCS} ${HDRS}
	(u=$${USER-root} d=`pwd` h=`${HOSTNAMECMD}` t=`date`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < version.c > tmp_version.c); sleep 1

pathnames.h: ${TOP}/.settings Makefile pathtemplate.h
	rm -f pathnames.h
	sed -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    -e "s|%DESTEXEC%|${DESTEXEC}|" \
	    -e "s|%DESTETC%|${DESTETC}|" \
	    -e "s|%DESTRUN%|${DESTRUN}|" \
	    < pathtemplate.h > pathnames.h

ns_signal.${O}: ns_signal.c
	${CC} ${CPPFLAGS} ${CFLAGS} -c $*.c

.c.${O}:
	${CC} ${CPPFLAGS} ${CFLAGS} ${BOUNDS} -c $*.c

distclean: clean
	rm -f ns_parser.c ns_parser.h

clean: FRC
	rm -f ${PROG}${EXE} ${OBJS} core .depend
	rm -f *.BAK *.CKP *~ *.orig
	rm -f tmp_version.c tmp_version.${O}
	rm -f pathnames.h y.tab.h y.tab.c

depend: ${SRCS} pathnames.h
	mkdep ${CPPFLAGS} -I${INCL} -I${PORTINCL} ${DEFS} ${SRCS}

${DESTDIR}${DESTSBIN}:
	mkdir -p ${DESTDIR}${DESTSBIN}

install: ${DESTDIR}${DESTSBIN} ${PROG}${EXE}
	${INSTALL} ${STRIP} -c ${INSTALL_EXEC} -m 755 ${PROG}${EXE} ${DESTDIR}${DESTSBIN}/${PROG}${EXE}

links: FRC
	@ln -s SRC/*.[chy] SRC/test .; rm -f ns_parser.[ch]

tags: FRC
	ctags ${SRCS} *.h

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
