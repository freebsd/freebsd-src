
.include <src.opts.mk>

# XXX MISSING:	man3f
SUBDIR=	man1 man3 man3lua man4 man5 man6 man7 man8 man9
SUBDIR_PARALLEL=

MAKEWHATIS?=	makewhatis

makedb:
.if ${MK_MAN_UTILS} != "no"
	${MAKEWHATIS} ${DESTDIR}${BINDIR}/man
.if defined(NO_ROOT) && defined(METALOG)
	echo ".${DISTBASE}${BINDIR}/man/mandoc.db type=file mode=0644 uname=root gname=wheel" | \
		cat -l >> ${METALOG}
.endif
.if ${MK_OPENSSL} != "no"
	${MAKEWHATIS} ${DESTDIR}${BINDIR}/openssl/man
.if defined(NO_ROOT) && defined(METALOG)
	echo ".${DISTBASE}${BINDIR}/openssl/man/mandoc.db type=file mode=0644 uname=root gname=wheel" | \
		cat -l >> ${METALOG}
.endif
.endif
.endif

.include "${.CURDIR}/../Makefile.inc"
.include <bsd.subdir.mk>
