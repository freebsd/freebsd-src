CONTRIB=${SRCTOP}/contrib/bsnmp
.PATH: ${CONTRIB}/tests

PROG_CXX=ctest
SRCS= main.cc asn1.cc snmp_parse_server.cc
CFLAGS += -I/${CONTRIB}/lib
CXXFLAGS+= -std=c++2a
CXXWARNFLAGS += -Wno-missing-variable-declarations
LIBADD= bsnmp

CFLAGS += -DBOGUS_CVE_2019_5610_FIX
MK_MAN= no

.include <bsd.prog.mk>


run:
	${.OBJDIR}/ctest
