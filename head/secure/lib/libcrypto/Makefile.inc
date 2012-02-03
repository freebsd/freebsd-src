# $FreeBSD$

.include <bsd.own.mk>

# OpenSSL version used for manual page generation
OPENSSL_VER=	0.9.8q
OPENSSL_DATE=	2010-12-02

LCRYPTO_SRC=	${.CURDIR}/../../../crypto/openssl
LCRYPTO_DOC=	${.CURDIR}/../../../crypto/openssl/doc

CFLAGS+=	-DTERMIOS -DANSI_SOURCE
CFLAGS+=	-I${LCRYPTO_SRC} -I${LCRYPTO_SRC}/crypto -I${.OBJDIR}
CFLAGS+=	-DOPENSSL_THREADS -DDSO_DLFCN -DHAVE_DLFCN_H

.if ${MK_IDEA} == "no"
CFLAGS+=	-DOPENSSL_NO_IDEA
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
CFLAGS+= -DL_ENDIAN
.endif

MANDIR=	${SHAREDIR}/openssl/man/man

.if defined(LIB)
_docs=	${LIB}
_skip=	des_modes
_sec=	3
.else
_docs=	apps
_skip=	config
_sec=	1
.endif

man-update:
.for manpage in ${MAN}
	@(sec=${manpage:E}; \
	pod=${manpage:R}.pod; \
	cp ${LCRYPTO_DOC}/${_docs}/$$pod .; \
	pod2man --section=$$sec --release="${OPENSSL_VER}" \
	  --date="${OPENSSL_DATE}" --center="OpenSSL"  \
	  $$pod > ${.CURDIR}/man/${manpage}; \
	rm -f $$pod; \
	${ECHO} ${manpage})
.endfor

man-makefile-update:
	rm -f ${.CURDIR}/Makefile.man
	echo '# $$'FreeBSD'$$' >> ${.CURDIR}/Makefile.man
	echo '# DO NOT EDIT: generated from man-makefile-update target' >> \
	    ${.CURDIR}/Makefile.man
	for i in ${LCRYPTO_DOC}/${_docs}/*.pod; do \
		fn=`basename $$i .pod`; \
		if [ "$$fn" != "${_skip}" ]; then \
		${ECHO} "MAN+= $$fn.${_sec}" >> ${.CURDIR}/Makefile.man; \
		fi; \
	done
	for i in ${LCRYPTO_DOC}/${_docs}/*.pod; do \
		fn=`basename $$i .pod`; \
		if [ "$$fn" != "${_skip}" ]; then \
		perl ${LCRYPTO_SRC}/util/extract-names.pl < $$i | \
		  awk "/^$$fn\$$/ { next; } \
		  { print \"MLINKS+= $$fn.${_sec} \" \$$1 \".${_sec}\" }" >> \
		  ${.CURDIR}/Makefile.man; \
		fi; \
	done
