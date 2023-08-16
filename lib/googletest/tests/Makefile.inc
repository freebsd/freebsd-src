
.include "../Makefile.inc"
# Keep the existing tests directory structure (with subdirs per component)
# rather than installing all of them to /usr/tests/lib/googletest
TESTSDIR=	${TESTSBASE}/lib/googletest/${.CURDIR:T}

# Clang's optimizer spends a really long time on these tests at -O2. Changing
# -O2 to -O1 reduces the -j32 time for lib/googletest/test from 131s to 71s.
# Using -O0 further reduces the time to 29s, and also reduces the disk usage
# from 144MB (at -O2) / 92MB (at -O1) to 82MB, so we use -O0.
# Note: Building without debug info saves about 10-15% of the build time, so we
# only enable debug info if DEBUG_FLAGS is not empty (71s -> 64s at -O1 and -j32).
CFLAGS.clang+=	-O0
.if empty(DEBUG_FLAGS)
MK_DEBUG_FILES:=no
CFLAGS.clang+=	-g0
.endif
