
.include <src.opts.mk>

TESTSDIR=		${TESTSBASE}/sys

TESTS_SUBDIRS+=		acl
TESTS_SUBDIRS+=		aio
TESTS_SUBDIRS+=		${_audit}
TESTS_SUBDIRS+=		auditpipe
TESTS_SUBDIRS+=		capsicum
TESTS_SUBDIRS+=		${_cddl}
# XXX: Currently broken in CI
#TESTS_SUBDIRS+=		compat32
TESTS_SUBDIRS+=		devrandom
TESTS_SUBDIRS+=		fifo
TESTS_SUBDIRS+=		file
TESTS_SUBDIRS+=		fs
TESTS_SUBDIRS+=		geom
TESTS_SUBDIRS+=		kern
TESTS_SUBDIRS+=		kqueue
TESTS_SUBDIRS+=		mac
TESTS_SUBDIRS+=		mqueue
TESTS_SUBDIRS+=		net
TESTS_SUBDIRS+=		${_netgraph}
TESTS_SUBDIRS+=		netinet
TESTS_SUBDIRS+=		netinet6
TESTS_SUBDIRS+=		netipsec
TESTS_SUBDIRS+=		netlink
TESTS_SUBDIRS+=		netmap
TESTS_SUBDIRS+=		netpfil
TESTS_SUBDIRS+=		opencrypto
TESTS_SUBDIRS+=		posixshm
TESTS_SUBDIRS+=		ses
TESTS_SUBDIRS+=		sys
TESTS_SUBDIRS+=		vfs
TESTS_SUBDIRS+=		vm
TESTS_SUBDIRS+=		vmm

.if ${MK_AUDIT} != "no"
_audit=	audit
.endif

.if ${MK_CDDL} != "no"
_cddl=	cddl
.endif

.if ${MK_NETGRAPH} != "no"
_netgraph= netgraph
.endif

# Items not integrated into kyua runs by default
SUBDIR+=		pjdfstest

SUBDIR+=		common

.include <bsd.test.mk>
