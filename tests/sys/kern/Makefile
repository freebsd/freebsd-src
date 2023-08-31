
.include <src.opts.mk>

PACKAGE=	tests

TESTSRC=	${SRCTOP}/contrib/netbsd-tests/kernel
.PATH:		${SRCTOP}/sys/kern

TESTSDIR=	${TESTSBASE}/sys/kern

ATF_TESTS_C+=	basic_signal
.if ${MACHINE_ARCH} != "i386" && ${MACHINE_ARCH} != "powerpc" && \
	${MACHINE_ARCH} != "powerpcspe"
# No support for atomic_load_64 on i386 or (32-bit) powerpc
ATF_TESTS_C+=	kcov
.endif
ATF_TESTS_C+=	kern_copyin
ATF_TESTS_C+=	kern_descrip_test
ATF_TESTS_C+=	fdgrowtable_test
ATF_TESTS_C+=	kill_zombie
.if ${MK_OPENSSL} != "no"
ATF_TESTS_C+=	ktls_test
.endif
ATF_TESTS_C+=	module_test
ATF_TESTS_C+=	ptrace_test
TEST_METADATA.ptrace_test+=		timeout="15"
ATF_TESTS_C+=	reaper
ATF_TESTS_C+=	sched_affinity
ATF_TESTS_C+=	sigaltstack
ATF_TESTS_C+=	sigwait
ATF_TESTS_C+=	socket_accf
ATF_TESTS_C+=	socket_msg_trunc
ATF_TESTS_C+=	socket_msg_waitall
TEST_METADATA.sigwait+=	is_exclusive="true"
.if ${MACHINE_ARCH} != "i386" && ${MACHINE_ARCH:Mpowerpc*} == ""
ATF_TESTS_C+=	subr_physmem_test
.endif
PLAIN_TESTS_C+=	subr_unit_test
ATF_TESTS_C+=	sysctl_kern_proc
ATF_TESTS_C+=	sys_getrandom
ATF_TESTS_C+=	unix_dgram
ATF_TESTS_C+=	unix_passfd_dgram
TEST_METADATA.unix_passfd_dgram+=	is_exclusive="true"
ATF_TESTS_C+=	unix_passfd_stream
TEST_METADATA.unix_passfd_stream+=	is_exclusive="true"
ATF_TESTS_C+=	unix_seqpacket_test
TEST_METADATA.unix_seqpacket_test+=	timeout="15"
ATF_TESTS_C+=	unix_socketpair_test
ATF_TESTS_C+=	waitpid_nohang
ATF_TESTS_C+=	pdeathsig

ATF_TESTS_SH+=	coredump_phnum_test
ATF_TESTS_SH+=	sonewconn_overflow
TEST_METADATA.sonewconn_overflow+=	required_programs="python"
TEST_METADATA.sonewconn_overflow+=	required_user="root"
ATF_TESTS_SH+=	sendfile_test

${PACKAGE}FILES+=	sonewconn_overflow.py
${PACKAGE}FILESMODE_sonewconn_overflow.py=0555

BINDIR=		${TESTSDIR}
PROGS+=		coredump_phnum_helper
PROGS+=		pdeathsig_helper
PROGS+=		sendfile_helper

CFLAGS.sys_getrandom+=			-I${SRCTOP}/sys/contrib/zstd/lib
LIBADD.sys_getrandom+=			zstd
LIBADD.sys_getrandom+=			c
LIBADD.sys_getrandom+=			pthread
LIBADD.ptrace_test+=			pthread
LIBADD.unix_seqpacket_test+=		pthread
LIBADD.kcov+=				pthread
CFLAGS.ktls_test+=			-DOPENSSL_API_COMPAT=0x10100000L
LIBADD.ktls_test+=			crypto util
LIBADD.socket_msg_waitall+=		pthread
LIBADD.sendfile_helper+=		pthread
LIBADD.fdgrowtable_test+=		util pthread kvm procstat
LIBADD.sigwait+=			rt

NETBSD_ATF_TESTS_C+=	lockf_test
NETBSD_ATF_TESTS_C+=	mqueue_test
NETBSD_ATF_TESTS_C+=	sysv_test

CFLAGS.mqueue_test+=	-I${SRCTOP}/tests
LIBADD.mqueue_test+=	rt

ATF_TESTS_C+=	libkern_crc32
SRCS.libkern_crc32+=	libkern_crc32.c
.PATH: ${SRCTOP}/sys/libkern
SRCS.libkern_crc32+=	gsb_crc32.c
CFLAGS.libkern_crc32+=	-DTESTING
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
.PATH: ${SRCTOP}/sys/libkern/x86
SRCS.libkern_crc32+=	crc32_sse42.c
.elif ${MACHINE_CPUARCH} == "aarch64"
.PATH: ${SRCTOP}/sys/libkern/arm64
SRCS.libkern_crc32+=	crc32c_armv8.S
.endif

CFLAGS.subr_physmem.c+=	-D_WANT_FREEBSD_BITSET
SRCS.subr_physmem_test+= subr_physmem_test.c subr_physmem.c

# subr_unit.c contains functions whose prototypes lie in headers that cannot be
# included in userland.  But as far as subr_unit_test goes, they're effectively
# static.  So it's ok to disable -Wmissing-prototypes for this program.
CFLAGS.subr_unit.c+=	-Wno-missing-prototypes
SRCS.subr_unit_test+=	subr_unit.c

WARNS?=	3

TESTS_SUBDIRS+=	acct
TESTS_SUBDIRS+=	execve
TESTS_SUBDIRS+=	pipe

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
