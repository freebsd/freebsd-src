# $Id: unexport.mk,v 1.1.1.3 2020/08/08 22:34:25 sjg Exp $

# pick up a bunch of exported vars
FILTER_CMD=	grep ^UT_
.include "export.mk"

.unexport UT_ZOO UT_FOO

UT_TEST = unexport

# Until 2020-08-08, Var_UnExport had special handling for '\n', that code
# was not reachable though.  At that point, backslash-newline has already
# been replaced with a simple space, and variables are not yet expanded.
UT_BEFORE_NL=	before
UT_AFTER_NL=	after
.export UT_BEFORE_NL UT_AFTER_NL
.unexport \
  UT_BEFORE_NL
.unexport ${.newline} UT_AFTER_NL
