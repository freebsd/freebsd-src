# $FreeBSD$

PACKAGE=	runtime

CONFS=	services
PROG=	services_mkdb
MAN=	services_mkdb.8
SRCS=	services_mkdb.c uniq.c extern.h
SERVICES_MKDB_CMD?=	${PROG}

.include <bsd.endian.mk>
.include <src.opts.mk>
.if ${MK_SERVICESDB} != "no"
afterinstallconfig:
	${SERVICES_MKDB_CMD} ${CAP_MKDB_ENDIAN} -q -o ${DESTDIR}/var/db/services.db \
		${DESTDIR}/etc/services
.if defined(NO_ROOT) && defined(METALOG)
	echo "./var/db/services.db type=file mode=0644 uname=root gname=wheel" | \
		cat -l >> ${METALOG}
.endif
.endif

.include <bsd.prog.mk>
