# $FreeBSD$

PACKAGE=		tests

FILESYSTEM?=		${.CURDIR:T}

TESTSDIR=		${TESTSBASE}/sys/fs/${FILESYSTEM}

TESTSRC=		${SRCTOP}/contrib/netbsd-tests/fs/${FILESYSTEM}

# XXX: uses /dev/MAKEDEV to create pseudo /dev/{null,zero} character devices
#NETBSD_ATF_TESTS_SH+=	devices_test
NETBSD_ATF_TESTS_SH+=	create_test
NETBSD_ATF_TESTS_SH+=	read_write_test
NETBSD_ATF_TESTS_SH+=	dots_test
NETBSD_ATF_TESTS_SH+=	exec_test
NETBSD_ATF_TESTS_SH+=	link_test
NETBSD_ATF_TESTS_SH+=	mkdir_test
NETBSD_ATF_TESTS_SH+=	mknod_test
NETBSD_ATF_TESTS_SH+=	mount_test
# XXX: need to replace `mknod ... p` with something more sensible
#NETBSD_ATF_TESTS_SH+=	pipes_test
NETBSD_ATF_TESTS_SH+=	trail_slash_test
NETBSD_ATF_TESTS_SH+=	readdir_test
NETBSD_ATF_TESTS_SH+=	remove_test
NETBSD_ATF_TESTS_SH+=	rename_test
NETBSD_ATF_TESTS_SH+=	rmdir_test
NETBSD_ATF_TESTS_SH+=	setattr_test
NETBSD_ATF_TESTS_SH+=	sizes_test
NETBSD_ATF_TESTS_SH+=	sockets_test
NETBSD_ATF_TESTS_SH+=	statvfs_test
NETBSD_ATF_TESTS_SH+=	symlink_test
NETBSD_ATF_TESTS_SH+=	times_test
NETBSD_ATF_TESTS_SH+=	truncate_test
NETBSD_ATF_TESTS_SH+=	vnd_test
NETBSD_ATF_TESTS_SH+=	vnode_leak_test

${PACKAGE}FILES+=	h_funcs.subr
${PACKAGE}FILESDIR=	${TESTSDIR}

PROGS+=			h_tools
BINDIR.h_tools=		${TESTSDIR}

ATF_TESTS_SH_SED_mount_test=		\
					-e 's,-o -g,-o gid=,g' \
					-e 's,-o -m,-o mode=,g' \
					-e 's,-o -s,-o size=,g' \
					-e 's,-o -u,-o uid=,g' \
					-e 's,mount_${FILESYSTEM},mount -t ${FILESYSTEM},g'
ATF_TESTS_SH_SED_sizes_test=		-e 's,-o -s,-o size=,g'
ATF_TESTS_SH_SED_statvfs_test=		-e 's,-o -s,-o size=,g'
ATF_TESTS_SH_SED_vnode_leak_test=	-e 's,-o -s,-o size=,g'

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
