# $FreeBSD$

PROG=	getconf

SRCS=	confstr.c getconf.c limits.c pathconf.c progenv.c sysconf.c
CFLAGS+= -I${.CURDIR}
CLEANFILES+=	confstr.c limits.c pathconf.c progenv.c sysconf.c \
		confstr.names limits.names pathconf.names sysconf.names \
		conflicting.names unique.names

.SUFFIXES: .gperf .names
.PHONY: conflicts

all:	conflicts

.gperf.c:
	LC_ALL=C awk -f ${.CURDIR}/fake-gperf.awk ${.IMPSRC} >${.TARGET}

.gperf.names:
	LC_ALL=C awk '/^[_A-Z]/ { print; }' ${.IMPSRC} | \
	    sed -e 's/,$$//' >${.TARGET}

conflicts: conflicting.names unique.names
	@if test `wc -l <conflicting.names` != `wc -l <unique.names`; then \
		echo "Name conflicts found!" >&2; \
		exit 1; \
	fi

# pathconf.names is not included here because pathconf names are
# syntactically distinct from the other kinds.
conflicting.names:	confstr.names limits.names sysconf.names
	cat ${.ALLSRC} >${.TARGET}

unique.names:		conflicting.names
	LC_ALL=C sort -u ${.ALLSRC} >${.TARGET}

.include <bsd.prog.mk>
