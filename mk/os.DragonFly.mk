# $Id: os.DragonFly.mk 2569 2012-09-04 16:34:04Z jkoshy $
#
# Build definitions for DragonFly

MKTESTS?=	yes	# Enable the test suites.
MKDOC?=		yes	# Build documentation.
MKNOWEB?=	no	# Build literate programs.

NOSHARED=	yes	# Link programs statically by default.
