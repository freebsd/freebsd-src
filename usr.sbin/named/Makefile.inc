# From: Id: Makefile.inc,v 8.4 1996/03/03 17:42:43 vixie Exp
# $Id: Makefile.inc,v 1.7 1998/05/06 13:23:39 bde Exp $

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

CFLAGS+=	-I ${BIND_DIR}/port/freebsd/include

# This is mostly for named and named-xfer
.if defined(USE_LIBBIND)
CFLAGS+=	-idirafter ${BIND_DIR}/include

.if exists(${.OBJDIR}/../../lib/libbind)
LIBBINDDIR:=	${.OBJDIR}/../../lib/libbind
.else
LIBBINDDIR!=	cd ${.CURDIR}/../../lib/libbind; make -V .OBJDIR
.endif
LIBBIND:=	${LIBBINDDIR}/libbind.a

DPADD+=		${LIBBIND}
LDADD+=		${LIBBIND}

CLEANFILES+=	tmp_version.c pathnames.h
CFLAGS+=	-I.

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

.endif

.include	"Makefile.maninc"
.endif
