# $FreeBSD: src/tools/regression/usr.bin/uudecode/regress.sh,v 1.4 2002/06/24 14:22:44 jmallett Exp $

REGRESSION_START($1)

REGRESSION_TEST_ONE(`uudecode -p < regress.traditional.in', `traditional')
REGRESSION_TEST_ONE(`uudecode -p < regress.base64.in', `base64')

REGRESSION_END()
