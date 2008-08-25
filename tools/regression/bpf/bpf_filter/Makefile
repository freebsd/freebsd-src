# $FreeBSD$

TEST_CASES=	test0001 test0002 test0003 test0004	\
		test0005 test0006 test0007 test0008	\
		test0009 test0010 test0011 test0012	\
		test0013 test0014 test0015 test0016	\
		test0017 test0018 test0019 test0020	\
		test0021 test0022 test0023 test0024	\
		test0025 test0026 test0027 test0028	\
		test0029 test0030 test0031 test0032	\
		test0033 test0034 test0035 test0036	\
		test0037 test0038 test0039 test0040	\
		test0041 test0042 test0043 test0044	\
		test0045 test0046 test0047 test0048	\
		test0049 test0050 test0051 test0052	\
		test0053 test0054 test0055 test0056	\
		test0057 test0058 test0059 test0060	\
		test0061 test0062 test0063 test0064	\
		test0065 test0066 test0067 test0068	\
		test0069 test0070 test0071 test0072	\
		test0073 test0074

SYSDIR?=	${.CURDIR}/../../../../sys

SRCS=		${.CURDIR}/bpf_test.c

CFLAGS+=	-g -fno-builtin-abort -I${.CURDIR}/tests

.if defined(LOG_LEVEL)
CFLAGS+=	-DLOG_LEVEL="${LOG_LEVEL}"
.endif

.if defined(BPF_VALIDATE)
CFLAGS+=	-DBPF_VALIDATE
.endif

.if (${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386") && defined(BPF_JIT)
SRCS+=		${SYSDIR}/net/bpf_jitter.c	\
		${SYSDIR}/${MACHINE_ARCH}/${MACHINE_ARCH}/bpf_jit_machdep.c
CFLAGS+=	-I${SYSDIR} -DBPF_JIT_COMPILER
WARNS?=		6
.else
SRCS+=		${SYSDIR}/net/bpf_filter.c
WARNS?=		1
.endif

.for TEST in ${TEST_CASES}
${TEST}:	${.CURDIR}/tests/${TEST}.h ${SRCS}
	@${CC} ${CFLAGS} -DBPF_TEST_H=\"${TEST}.h\" -o ${.CURDIR}/${TEST} ${SRCS}
.endfor

all:		${TEST_CASES}
.for TEST in ${TEST_CASES}
.if !defined(LOG_LEVEL) || (${LOG_LEVEL} > 0)
	@${ECHO} -n "${TEST}:	"
.endif
	@-${.CURDIR}/${TEST}
	@rm -f ${.CURDIR}/${TEST}
.endfor

clean:
.for TEST in ${TEST_CASES}
	@rm -f ${.CURDIR}/${TEST}
.endfor

.include <bsd.prog.mk>
