# $Id: Makefile,v 1.20 1998/06/04 07:25:52 bde Exp $

USE_LIBBIND=	true
.include "${.CURDIR}/Makefile.inc"

.PATH:	${BIND_DIR}/bin/named
.PATH:	${BIND_DIR}/doc/man

PROG=	named
SRCS=	tmp_version.c pathnames.h \
	db_dump.c db_load.c db_lookup.c db_save.c db_update.c \
	db_glue.c \
	ns_parser.y ns_lexer.c ns_parseutil.c \
	ns_forw.c ns_init.c ns_main.c ns_maint.c ns_req.c \
	ns_resp.c ns_stats.c ns_ncache.c ns_xfr.c ns_glue.c \
	ns_udp.c ns_config.c ns_update.c
MAN8=	named.8

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${BIND_DIR}/bin/named/named-bootconf.pl \
		${DESTDIR}${DESTSBIN}/named-bootconf

.include <bsd.prog.mk>
