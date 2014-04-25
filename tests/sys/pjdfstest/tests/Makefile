TOP=		${.CURDIR:H:H:H:H}

.PATH:		${TOP}/tools/regression/pjdfstest/tests
.PATH:		${.CURDIR:H:H:H}

TESTSDIR=	${TESTSBASE}/sys/pjdfstest

KYUAFILE=	yes

SCRIPTSDIR=	${TESTSDIR}
SCRIPTS+=	conf
.for script in kyua.sh misc.sh
SCRIPTS+=	${script}
SCRIPTSNAME_${script}=	${script}
.endfor

TESTS_SUBDIRS+=	chflags
TESTS_SUBDIRS+=	chmod
TESTS_SUBDIRS+=	chown
TESTS_SUBDIRS+=	ftruncate
TESTS_SUBDIRS+=	granular
TESTS_SUBDIRS+=	link
TESTS_SUBDIRS+=	mkdir
TESTS_SUBDIRS+=	mkfifo
TESTS_SUBDIRS+=	mknod
TESTS_SUBDIRS+=	open
TESTS_SUBDIRS+=	rename
TESTS_SUBDIRS+=	rmdir
TESTS_SUBDIRS+=	symlink
TESTS_SUBDIRS+=	truncate
TESTS_SUBDIRS+=	unlink

.include <bsd.test.mk>
