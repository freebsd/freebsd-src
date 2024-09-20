.PATH:		${SRCTOP}/tests
KYUAFILE=	yes

# Note: we start the gmock_main and gmock tests first since those take up to
# 60 seconds to build, so starting them late seriously reduces build parallism.
SUBDIR=		gmock_main gmock gtest_main gtest

SUBDIR_PARALLEL=

.include <bsd.test.mk>
