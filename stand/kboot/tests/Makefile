SRCTOP?=	${.CURDIR:H:H:H}

PYTEST_ARGS?=	-q

# Local kboot HTTP(S) pytest harness; artifacts live under WORKDIR.
WORKDIR?=	${TMPDIR:U/tmp}/kboot-http-tests
TEST_OBJDIR?=	${WORKDIR}/obj
CERTDIR?=	${WORKDIR}/certs
PYTEST?=	pytest

KBOOTVARS=	KBOOT_HTTP_TEST=yes CERT_DIR=${CERTDIR}
MAKEENV=	MAKEOBJDIRPREFIX=${TEST_OBJDIR}

all build clean test: .PHONY

all: test

${CERTDIR}/ca.pem: ${SRCTOP}/crypto/openssl/demos/guide/rootcert.pem
	mkdir -p ${CERTDIR}
	cp ${.ALLSRC} ${.TARGET}

build: ${CERTDIR}/ca.pem
	mkdir -p ${TEST_OBJDIR}
	env ${MAKEENV} ${MAKE} -C ${SRCTOP}/stand ${KBOOTVARS} kboot

test: build
	${PYTEST} --version >/dev/null 2>&1 || \
	    { echo "pytest is not available; install it or set PYTEST"; false; }
	kboot_objdir=`env ${MAKEENV} \
	    ${MAKE} -C ${SRCTOP}/stand/kboot/kboot \
	    ${KBOOTVARS} \
	    -V .OBJDIR`; \
	cd ${.CURDIR}; \
	env KBOOT_LOADER="$${kboot_objdir}/loader.kboot" \
	    FREEBSD_SRC="${SRCTOP}" \
	    ${PYTEST} ${PYTEST_ARGS} .

clean:
	rm -rf ${WORKDIR}
