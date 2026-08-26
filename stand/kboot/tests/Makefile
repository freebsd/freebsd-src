SRCTOP?=	${.CURDIR:H:H:H}

# Local kboot HTTP(S) pytest harness; artifacts live under WORKDIR.
WORKDIR?=	${TMPDIR:U/tmp}/kboot-http-tests
TEST_OBJDIR?=	${WORKDIR}/obj
CERTDIR?=	${WORKDIR}/certs

KBOOTVARS=	KBOOT_HTTP_TEST=yes CERT_DIR=${CERTDIR}
MAKEENV=	MAKEOBJDIRPREFIX=${TEST_OBJDIR}

all build clean: .PHONY

all: build

${CERTDIR}/ca.pem: ${SRCTOP}/crypto/openssl/demos/guide/rootcert.pem
	mkdir -p ${CERTDIR}
	cp ${.ALLSRC} ${.TARGET}

build: ${CERTDIR}/ca.pem
	mkdir -p ${TEST_OBJDIR}
	env ${MAKEENV} ${MAKE} -C ${SRCTOP}/stand ${KBOOTVARS} kboot

clean:
	rm -rf ${WORKDIR}
