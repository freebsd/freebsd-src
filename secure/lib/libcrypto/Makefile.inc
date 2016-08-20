# $FreeBSD$

.include <bsd.own.mk>

# OpenSSL version used for manual page generation
OPENSSL_VER=	1.0.2h
OPENSSL_DATE=	2016-05-03

LCRYPTO_SRC=	${.CURDIR}/../../../crypto/openssl
LCRYPTO_DOC=	${LCRYPTO_SRC}/doc

CFLAGS+=	-I${LCRYPTO_SRC}
CFLAGS+=	-DTERMIOS -DANSI_SOURCE
CFLAGS+=	-DOPENSSL_THREADS -DDSO_DLFCN -DHAVE_DLFCN_H

.include <bsd.endian.mk>

.if ${TARGET_ENDIANNESS} == 1234
CFLAGS+=-DL_ENDIAN
.elif ${TARGET_ENDIANNESS} == 4321
CFLAGS+=-DB_ENDIAN
.endif

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
_ASM_AVX!=	{ \
		    echo vzeroall | \
		    ${CC} -x assembler -o /dev/null -c - 2> /dev/null; \
		} && echo yes || echo no
.if ${_ASM_AVX} == yes
ASM_${MACHINE_CPUARCH}=
.endif
.endif

.if defined(ASM_amd64)
CFLAGS+=-DOPENSSL_IA32_SSE2
CFLAGS+=-DAES_ASM -DBSAES_ASM -DVPAES_ASM
CFLAGS+=-DECP_NISTZ256_ASM
CFLAGS+=-DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_BN_ASM_GF2m
CFLAGS+=-DMD5_ASM
CFLAGS+=-DGHASH_ASM
CFLAGS+=-DSHA1_ASM -DSHA256_ASM -DSHA512_ASM
CFLAGS+=-DWHIRLPOOL_ASM
.elif defined(ASM_i386)
CFLAGS+=-DOPENSSL_IA32_SSE2
CFLAGS+=-DAES_ASM -DVPAES_ASM
CFLAGS+=-DOPENSSL_BN_ASM_PART_WORDS -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_GF2m
CFLAGS+=-DMD5_ASM
CFLAGS+=-DGHASH_ASM
CFLAGS+=-DRMD160_ASM
CFLAGS+=-DSHA1_ASM -DSHA256_ASM -DSHA512_ASM
CFLAGS+=-DWHIRLPOOL_ASM
.endif

MANDIR=	${SHAREDIR}/openssl/man/man

.if defined(LIB)
_docs=	${LIB}
_skip=	SSLeay_version des_modes
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
		if ! echo '${_skip}' | grep -qw "$$fn"; then \
		${ECHO} "MAN+= $$fn.${_sec}" >> ${.CURDIR}/Makefile.man; \
		fi; \
	done
	for i in ${LCRYPTO_DOC}/${_docs}/*.pod; do \
		fn=`basename $$i .pod`; \
		if ! echo '${_skip}' | grep -qw "$$fn"; then \
		perl ${LCRYPTO_SRC}/util/extract-names.pl < $$i | \
		  awk "/^$$fn\$$/ { next; } \
		  { print \"MLINKS+= $$fn.${_sec} \" \$$1 \".${_sec}\" }" >> \
		  ${.CURDIR}/Makefile.man; \
		fi; \
	done
