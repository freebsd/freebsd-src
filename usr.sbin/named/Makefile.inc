# From: Id: Makefile.inc,v 8.4 1996/03/03 17:42:43 vixie Exp
# $Id: Makefile.inc,v 1.5 1997/05/17 11:53:03 peter Exp $

.ifndef (Mk.Inc)
Mk.Inc?=defined

BIND_DIR=	${.CURDIR}/../../contrib/bind

VER!=		cat ${BIND_DIR}/Version

PS=		ps
PIDDIR=		/var/run
DESTETC=	/etc/namedb
DESTEXEC=	/usr/libexec
DESTRUN=	/var/run
DESTSBIN=	/usr/sbin
DESTHELP=	/usr/share/misc

INCLUDE=	-I${BIND_DIR}/port/freebsd/include -I${BIND_DIR}/include -I.
CFLAGS+=	${INCLUDE} ${CONFIG}

.if exists(${.OBJDIR}/../../lib/libbind)
LDADD+=	-L${.OBJDIR}/../../lib/libbind -lbind
DPADD+= ${.OBJDIR}/../../lib/libbind/libbind.a
.else
LDADD+=	-L${.CURDIR}/../../lib/libbind -lbind
DPADD+= ${.CURDIR}/../../lib/libbind/libbind.a
.endif

CLEANFILES+= tmp_version.c pathnames.h

tmp_version.c:	version.c ${BIND_DIR}/Version
	(u=$${USER-root} d=`pwd` h=`hostname` t=`LC_TIME=C date`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < ${BIND_DIR}/bin/named/version.c > tmp_version.c)
	
pathnames.h:	pathtemplate.h ${.CURDIR}/../../usr.sbin/named/Makefile.inc
	rm -f pathnames.h
	sed -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    -e "s|%DESTEXEC%|${DESTEXEC}|" \
	    -e "s|%DESTETC%|${DESTETC}|" \
	    -e "s|%DESTRUN%|${DESTRUN}|" \
	    < ${BIND_DIR}/bin/named/pathtemplate.h > pathnames.h

.include	"Makefile.maninc"
.endif
