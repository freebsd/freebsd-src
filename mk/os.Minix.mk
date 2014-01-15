# $Id: os.Minix.mk 2569 2012-09-04 16:34:04Z jkoshy $
#
# Build definitions for Minix 3.2.0.

MKDOC?=		yes	# Build documentation.
MKTESTS?=	no	# Enable the test suites.
MKNOWEB?=	no	# Build literate programs.

# Use GCC to compile the source tree.
CC=/usr/pkg/bin/gcc

# Use the correct compiler type (override <sys.mk>).
COMPILER_TYPE=gnu

# Also choose GNU 'ar'.
AR=ar
