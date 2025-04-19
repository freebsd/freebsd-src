
# Consider this file an example.
#
# For Junos this is how we obtain trust anchor .pems
# the signing server (http://www.crufty.net/sjg/blog/signing-server.htm)
# for each key will provide the appropriate certificate chain on request

# allow site control
.-include "site.trust.mk"

#VE_DEBUG_LEVEL?=3
#VE_VERBOSE_DEFAULT?=2

VE_HASH_LIST?= \
	SHA256 \
	SHA384 \

VE_SELF_TESTS?= yes

# client for the signing server above
SIGNER?= /opt/sigs/sign.py

.if exists(${SIGNER})
OPENPGP_SIGNER?= ${SIGNER:H}/openpgp-sign.py
OPENPGP_SIGN_FLAGS= -a
OPENPGP_SIGN_HOST?= localhost
SIGN_HOST ?= localhost

# A list of name/ext/url tuples.
# name should be one of ECDSA, OPENPGP or RSA, they can be repeated
# Order of ext list implies runtime preference so do not sort!
VE_SIGN_URL_LIST?= \
	ECDSA/esig/${SIGN_HOST}:${133%y:L:localtime} \
	RSA/rsig/${SIGN_HOST}:${163%y:L:localtime} \
	OPENPGP/asc/${OPENPGP_SIGN_HOST}:1234 \

.for sig ext url in ${VE_SIGN_URL_LIST:@x@${x:H:H} ${x:H:T} ${x:T}@}
SIGN_${sig}:= ${PYTHON} ${${sig}_SIGNER:U${SIGNER}} -u ${url} ${${sig}_SIGN_FLAGS:U-h sha256}

VE_SIGNATURE_LIST+= ${sig}
VE_SIGNATURE_EXT_LIST+= ${ext}

_SIGN_${sig}_USE:	.USE
	${SIGN_${sig}} ${.ALLSRC}

_TA_${sig}_USE:       .USE
	${SIGN_${sig}} -C ${.TARGET}

.if ${sig} == "OPENPGP"
ta_${sig:tl}.${ext}: _TA_${sig}_USE
ta_${ext}.h: ta_${sig:tl}.${ext}
.else
${ext:S/sig/certs/}.pem: _TA_${sig}_USE
# the last cert in the chain is the one we want
ta_${ext}.pem: ${ext:S/sig/certs/}.pem _LAST_PEM_USE
ta.h: ta_${ext}.pem
.if ${VE_SELF_TESTS} != "no"
# we use the 2nd last cert to test verification
vc_${ext}.pem: ${ext:S/sig/certs/}.pem _2ndLAST_PEM_USE
ta.h: vc_${ext}.pem
.endif
.endif
.endfor

# cleanup duplicates
VE_SIGNATURE_LIST:= ${VE_SIGNATURE_LIST:O:u}

.if target(ta_asc.h)
XCFLAGS.opgp_key+= -DHAVE_TA_ASC_H

.if ${VE_SELF_TESTS} != "no"
# for self test
vc_openpgp.asc: ta_openpgp.asc
	${SIGN_OPENPGP} ${.ALLSRC:M*.asc}
	mv ta_openpgp.asc.asc ${.TARGET}

ta_asc.h: vc_openpgp.asc
.endif
.endif

.else
VE_SIGNATURE_LIST?= RSA

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
.if ${VE_SIGNATURE_EXT_LIST:Mesig} != ""
VE_SIGNATURE_LIST+= ECDSA
.endif
.endif

.if ${TRUST_ANCHORS:T:Mt*.asc} != ""
VE_SIGNATURE_LIST+= OPENPGP
VE_SIGNATURE_EXT_LIST+= asc
ta_asc.h: ${TRUST_ANCHORS:M*.asc}
.endif
# we take the mtime of this as our baseline time
BUILD_UTC_FILE?= ${TRUST_ANCHORS:[1]}
.endif
