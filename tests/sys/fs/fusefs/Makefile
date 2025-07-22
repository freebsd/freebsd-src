.include <bsd.compiler.mk>

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/fs/fusefs

ATF_TESTS_SH+=	ctl

# We could simply link all of these files into a single executable.  But since
# Kyua treats googletest programs as plain tests, it's better to separate them
# out, so we get more granular reporting.
GTESTS+=	access
GTESTS+=	allow_other
GTESTS+=	bad_server
GTESTS+=	bmap
GTESTS+=	cache
GTESTS+=	copy_file_range
GTESTS+=	create
GTESTS+=	default_permissions
GTESTS+=	default_permissions_privileged
GTESTS+=	destroy
GTESTS+=	dev_fuse_poll
GTESTS+=	fallocate
GTESTS+=	fifo
GTESTS+=	flush
GTESTS+=	forget
GTESTS+=	fsync
GTESTS+=	fsyncdir
GTESTS+=	getattr
GTESTS+=	interrupt
GTESTS+=	io
GTESTS+=	last_local_modify
GTESTS+=	link
GTESTS+=	locks
GTESTS+=	lookup
GTESTS+=	lseek
GTESTS+=	mkdir
GTESTS+=	mknod
GTESTS+=	mount
GTESTS+=	nfs
GTESTS+=	notify
GTESTS+=	open
GTESTS+=	opendir
GTESTS+=	pre-init
GTESTS+=	read
GTESTS+=	readdir
GTESTS+=	readlink
GTESTS+=	release
GTESTS+=	releasedir
GTESTS+=	rename
GTESTS+=	rmdir
GTESTS+=	setattr
GTESTS+=	statfs
GTESTS+=	symlink
GTESTS+=	unlink
GTESTS+=	write
GTESTS+=	xattr

.for p in ${GTESTS}
SRCS.$p+=	${p}.cc
SRCS.$p+=	mockfs.cc
SRCS.$p+=	utils.cc
.endfor

TEST_METADATA.default_permissions+=	required_user="unprivileged"
TEST_METADATA.default_permissions_privileged+=	required_user="root"
TEST_METADATA.mknod+=	required_user="root"
TEST_METADATA.nfs+=	required_user="root"
# ctl must be exclusive because it disables/enables camsim
TEST_METADATA.ctl+=	is_exclusive="true"
TEST_METADATA.ctl+=	required_user="root"

TEST_METADATA+=	timeout=10
TEST_METADATA+=	required_kmods="fusefs"

FUSEFS=		${SRCTOP}/sys/fs/fuse
# Suppress warnings that GCC generates for the libc++ and gtest headers.
CXXWARNFLAGS.gcc+=	-Wno-placement-new -Wno-attributes
# Suppress Wcast-align for readdir.cc, because it is unavoidable when using
# getdirentries.
CXXWARNFLAGS.readdir.cc+=	-Wno-cast-align
.if ${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 80000
CXXWARNFLAGS+=	-Wno-class-memaccess
.endif
# Suppress false warning about set but unused DNAME in inval_entry_below_root
CXXWARNFLAGS.gcc+= -Wno-unused-but-set-variable
# Suppress warnings about deprecated implicit copy constructors in gtest.
CXXWARNFLAGS+=  -Wno-deprecated-copy
.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 180000
# clang 18.0.0 introduces a new warning about variable length arrays in C++.
CXXWARNFLAGS+=	-Wno-vla-cxx-extension
.endif
CXXFLAGS+=	-I${SRCTOP}/tests
CXXFLAGS+=	-I${FUSEFS}

LIBADD+=	pthread
LIBADD+=	gmock gtest
LIBADD+=	util

.include <bsd.test.mk>
