#	@(#)Makefile	5.8 (Berkeley) 7/28/90

### -DALLOW_T_UNSPEC -Dmalloc=rt_malloc -Dfree=rt_free
### ALLOC=storage.o
PROG=	named
MAN8=	named.8
CFLAGS=	-O -DDEBUG -DSTATS
LDADD=	-lutil
SRCS=	db_dump.c db_glue.c db_load.c db_lookup.c db_reload.c db_save.c \
	db_update.c ns_forw.c ns_init.c ns_main.c ns_maint.c ns_req.c \
	ns_resp.c ns_sort.c ns_stats.c
OBJS+=	version.o
CLEANFILES+=version.c version.o
SUBDIR=	tools xfer

version.c: ${.CURDIR}/Version.c
	(u=$${USER-root} d=`pwd |sed -e 's|/obj/|/src/|'` \
	 h=`hostname` t=`date`; \
	sed -e "s|%WHEN%|$${t}|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < ${.CURDIR}/Version.c > version.c)

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/named.restart \
	    ${.CURDIR}/named.reload ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
