
# Consider this file an example.
#
# For Junos this is how we obtain trust anchor .pems
# the signing server (http://www.crufty.net/sjg/blog/signing-server.htm)
# for each key will provide the appropriate certificate chain on request

# allow site control
.-include "site.trust.mk"

#VE_DEBUG_LEVEL?=3
#VE_VERBOSE_DEFAULT?=2

# just to be sure
VE_HASH_LIST+= \
	SHA256 \
	SHA384 \

VE_SELF_TESTS?= yes

# client for the signing server above
SIGNER?= /opt/sigs/sign.py

.if exists(${SIGNER})
SIGNER_asc?= ${SIGNER:H}/openpgp-sign.py
SIGN_FLAGS_asc= -a
SIGN_HOST_asc?= localhost
SIGN_HOST ?= localhost

# A list of ext/method/hash/flags/url tuples.
# method should include one of ECDSA, OPENPGP or RSA, they can be repeated
# Order of ext list implies runtime preference so do not sort!
VE_SIGN_URL_LIST?= \
	esig/ECDSA/sha256/1/${SIGN_HOST}:${133%y:L:localtime} \
	rsig/RSA/sha256/0/${SIGN_HOST}:${163%y:L:localtime} \
	asc/OPENPGP/sha256/0/${SIGN_HOST_asc}:1234 \

# Same as VE_SIGN_URL_LIST minus the url
VE_SIGNATURE_LIST += ${VE_SIGN_URL_LIST:H}

.if !target(_SIGN_USE)
_SIGN_USE:      .USE
        @${SIGN_${.TARGET:E}} -o ${.TARGET} ${.ALLSRC:M*${.TARGET:T:R}:u}
.endif

.if !target(_TA_USE)
_TA_USE:       .USE
	@if test ${.TARGET:E} = pem; then \
		${SIGN_${.TARGET:R:S,certs,sig,}} -C ${.TARGET}; \
	else \
		${SIGN_${.TARGET:E}} -C ${.TARGET}; \
	fi
.endif

# we define SIGN_esig etc for use by _SIGN_USE and _TA_USE
.for sig method hash flags url in ${VE_SIGN_URL_LIST:S,/, ,g}
SIGN_${sig}?= ${PYTHON} ${SIGNER_${sig}:U${SIGNER}} -h ${hash} -u ${url} ${SIGN_FLAGS_${sig}:U}

.if ${sig} == "asc"
ta_${method:tl}.${sig}: _TA_USE
ta_${sig}.h: ta_${method:tl}.${sig}
.else
${sig:S/sig/certs/}.pem: _TA_USE
# the last cert in the chain is the one we want
ta_${sig}.pem: ${sig:S/sig/certs/}.pem _LAST_PEM_USE
ta.h: ta_${sig}.pem
.if ${VE_SELF_TESTS} != "no"
# we use the 2nd last cert to test verification
vc_${sig}.pem: ${sig:S/sig/certs/}.pem _2ndLAST_PEM_USE
ta.h: vc_${sig}.pem
.endif
.endif
.endfor

.if target(ta_asc.h)
XCFLAGS.opgp_key+= -DHAVE_TA_ASC_H

.if ${VE_SELF_TESTS} != "no"
# for self test
vc_openpgp.asc: ta_openpgp.asc
	${SIGN_asc} ${.ALLSRC:M*.asc}
	mv ta_openpgp.asc.asc ${.TARGET}

ta_asc.h: vc_openpgp.asc
.endif
.endif

.else

# you need to provide t*.pem or t*.asc files for each trust anchor
# below assumes they are named ta_${ext}.pem eg ta_esig.pem for ECDSA
.if empty(TRUST_ANCHORS)
TRUST_ANCHORS!= cd ${.CURDIR} && 'ls' -1 *.pem t*.asc 2> /dev/null || echo
.endif
.if empty(TRUST_ANCHORS) && ${MK_LOADER_EFI_SECUREBOOT} != "yes"
.error Need TRUST_ANCHORS see ${.PARSEDIR}/README.rst
.endif

.if ${TRUST_ANCHORS:T:Mt*.pem} != ""
ta.h: ${TRUST_ANCHORS:M*.pem}
VE_SIGNATURE_EXT_LIST?= ${TRUST_ANCHORS:T:Mt*.pem:R:S/ta_//}
.endif

.if ${TRUST_ANCHORS:T:Mt*.asc} != ""
VE_SIGNATURE_EXT_LIST+= asc
ta_asc.h: ${TRUST_ANCHORS:M*.asc}
.endif

.if empty(VE_SIGNATURE_LIST)
method.asc?= OPENPGP/sha256/0
method.esig?= ECDSA/sha256/0
.for ext in ${VE_SIGNATURE_EXT_LIST}
VE_SIGNATURE_LIST+= ${ext}/${method.${ext}:URSA/sha256/0}
.endfor
.endif

# we take the mtime of this as our baseline time
BUILD_UTC_FILE?= ${TRUST_ANCHORS:[1]}
.endif
