# $FreeBSD$

TESTS=	nvlist_add
TESTS+=	nvlist_exists
TESTS+=	nvlist_free
TESTS+=	nvlist_get
TESTS+=	nvlist_move
TESTS+=	nvlist_send_recv

CFLAGS=	-O2 -pipe -fstack-protector
CFLAGS+=-Wsystem-headers -Werror -Wall -Wno-format-y2k -W -Wno-unused-parameter
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type
CFLAGS+=-Wcast-qual -Wwrite-strings -Wswitch -Wshadow -Wunused-parameter
CFLAGS+=-Wcast-align -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls
CFLAGS+=-Wold-style-definition -Wno-pointer-sign -Wextra

LDFLAGS+=-lnv

all:	${TESTS} ${TESTS:=.t}

.for TEST in ${TESTS}

${TEST}:	${TEST}.c
	${CC} ${CFLAGS} ${LDFLAGS} ${@}.c -o $@

${TEST}.t:   ${TEST}
	@printf "#!/bin/sh\n\n%s/%s\n" ${.CURDIR} ${@:.t=} > $@

.endfor

test:   all
	@prove -r ${.CURDIR}

clean:
	rm -f ${TESTS} ${TESTS:=.t}
