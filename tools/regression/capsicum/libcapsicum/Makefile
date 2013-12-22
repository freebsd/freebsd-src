# $FreeBSD$

SERVICES=	dns
SERVICES+=	grp
SERVICES+=	pwd
SERVICES+=	sysctl

CFLAGS=		-O2 -pipe -std=gnu99 -fstack-protector
CFLAGS+=	-Wsystem-headers -Werror -Wall -Wno-format-y2k -W -Wno-unused-parameter
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type
CFLAGS+=	-Wcast-qual -Wwrite-strings -Wswitch -Wshadow -Wunused-parameter
CFLAGS+=	-Wcast-align -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls
CFLAGS+=	-Wold-style-definition -Wno-pointer-sign

CFLAGS+=	-I${.CURDIR}/../../../../lib/libcapsicum
CFLAGS+=	-ggdb

all:	${SERVICES}

.for SERVICE in ${SERVICES}

${SERVICE}:	${SERVICE}.c
	${CC} ${CFLAGS} ${@}.c -o $@ -lcapsicum -lnv

.endfor

test:	all
	@prove -r ${.CURDIR}

clean:
	rm -f ${SERVICES}
