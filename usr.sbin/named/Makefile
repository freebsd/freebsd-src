# $Id: Makefile,v 1.12.2.1 1996/11/28 22:04:59 phk Exp $

.include "${.CURDIR}/Makefile.inc"

.PATH:	${BIND_DIR}/named
.PATH:	${BIND_DIR}/man

PROG=	named
SRCS=	version.c db_dump.c db_glue.c db_load.c db_lookup.c db_reload.c \
	db_save.c db_secure.c db_update.c ns_forw.c \
	ns_init.c ns_main.c ns_maint.c ns_ncache.c ns_req.c ns_resp.c \
	ns_sort.c ns_stats.c ns_udp.c ns_validate.c storage.c tree.c
MAN8=	named.8

CLEANFILES+= version.c

version.c: Version.c ${BIND_DIR}/Makefile
	(u=$${USER-root} d=`pwd` h=`hostname` t=`LC_TIME=C date`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < ${BIND_DIR}/named/Version.c > version.c)

.include <bsd.prog.mk>
