## Copyright (c) 1996, 1997 by Internet Software Consortium
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

# $Id: Makefile,v 8.31 1998/03/20 00:49:46 halley Exp $

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
LEX = lex -I
YACC = yacc -d
SYSLIBS = -ll -lutil
PIDDIR = /var/run
DESTBIN = /usr/local/bin
DESTSBIN = /usr/local/sbin
DESTEXEC = /usr/local/libexec
DESTMAN = /usr/share/man
DESTHELP= /usr/share/misc
AR= ar cruv
INSTALL= install
STRIP=-s

PS=ps
LDFLAGS=
CFLAGS=	${CDEBUG} -I${PORTINCL} -I${INCL} ${DEFS}

VER= LOCAL-`date +%y%m%d.%H%M%S`
HOSTNAMECMD= hostname || uname -n

PROG=	named
HDRS=	db_defs.h db_glob.h ns_defs.h ns_glob.h named.h pathnames.h
SRCS=	db_dump.c db_load.c db_lookup.c db_save.c db_update.c \
	db_glue.c \
	ns_parser.c ns_lexer.c ns_parseutil.c \
	ns_forw.c ns_init.c ns_main.c ns_maint.c ns_req.c \
	ns_resp.c ns_stats.c ns_ncache.c ns_xfr.c ns_glue.c \
	ns_udp.c ns_config.c ns_update.c
OBJS=	db_dump.${O} db_load.${O} db_lookup.${O} db_save.${O} db_update.${O} \
	db_glue.${O} \
	ns_parser.${O} ns_lexer.${O} ns_parseutil.${O} \
	ns_forw.${O} ns_init.${O} ns_main.${O} ns_maint.${O} ns_req.${O} \
	ns_resp.${O} ns_stats.${O} ns_ncache.${O} ns_xfr.${O} ns_glue.${O} \
	ns_udp.${O} ns_config.${O} ns_update.${O}

all: ${PROG} pathnames

${PROG}: pathnames.h ${OBJS} ${LIBBIND} Makefile tmp_version.${O}
	${CC} ${CDEBUG} ${LDFLAGS} -o ${PROG} ${OBJS} tmp_version.${O} \
		${LIBBIND} ${SYSLIBS}

ns_parser.c ns_parser.h: ns_parser.y
	${YACC} ns_parser.y
	mv y.tab.c ns_parser.c
	mv y.tab.h ns_parser.h

tmp_version.${O}: tmp_version.c

tmp_version.c: version.c Makefile ../Makefile ${SRCS} ${HDRS}
	(u=$${USER-root} d=`pwd` h=`${HOSTNAMECMD}` t=`date`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < version.c > tmp_version.c)

pathnames.h: ${TOP}/.settings Makefile
	rm -f pathnames.h
	sed -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    -e "s|%DESTEXEC%|${DESTEXEC}|" \
	    -e "s|%DESTETC%|${DESTETC}|" \
	    -e "s|%DESTRUN%|${DESTRUN}|" \
	    < pathtemplate.h > pathnames.h

pathnames: pathnames.${O} pathnames.h ${LIBBIND} Makefile
	${CC} ${CDEBUG} ${LDFLAGS} -o $@ pathnames.${O} \
		${LIBBIND} ${SYSLIBS}

distclean: clean
	rm -f ns_parser.c ns_parser.h

clean: FRC
	rm -f ${PROG} ${OBJS} core .depend
	rm -f *.BAK *.CKP *~ *.orig
	rm -f tmp_version.c tmp_version.${O}
	rm -f pathnames pathnames.${O} tmp_pathnames.h pathnames.h
	rm -f y.tab.h y.tab.c

depend: ${SRCS} pathnames.h
	mkdep ${CPPFLAGS} -I${INCL} -I${PORTINCL} ${DEFS} ${SRCS}

${DESTDIR}${DESTSBIN}:
	mkdir -p ${DESTDIR}${DESTSBIN}

install: ${DESTDIR}${DESTSBIN} ${PROG}
	${INSTALL} ${STRIP} -c -m 755 ${PROG} ${DESTDIR}${DESTSBIN}/${PROG}

links: FRC
	@ln -s SRC/*.[chy] SRC/test .; rm -f ns_parser.[ch]

tags: FRC
	ctags ${SRCS} *.h

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
